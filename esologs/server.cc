#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "re2/re2.h"

#include "base/log.h"
#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "esologs/server.h"
#include "esologs/stalker.h"
#include "event/loop.h"
#include "proto/brotli.h"
#include "proto/delim.h"
#include "web/server.h"
#include "web/writer.h"

extern "C" {
#include <stdlib.h>
}

namespace esologs {

namespace {

constexpr const char* kStalkerWebsocketProtocol = "v1.stalker.logs.esolangs.org";

} // unnamed namespace

Server::Server(const Config& config, event::Loop* loop) : loop_(loop) {
  if (config.listen_port().empty())
    throw base::Exception("missing required setting: listen_port");
  if (config.target_size() == 0)
    throw base::Exception("no targets configured");

  if (!config.metrics_addr().empty()) {
    metric_exposer_ = std::make_unique<prometheus::Exposer>(config.metrics_addr());
    metric_registry_ = std::make_shared<prometheus::Registry>();
    metric_exposer_->RegisterCollectable(metric_registry_);
  }

  for (const auto& target_config : config.target()) {
    if (target_config.log_path().empty())
      throw base::Exception("missing required setting: log_path");
    if (target_config.nick().empty())
      throw base::Exception("missing required setting: nick");

    auto target = std::make_unique<Target>(target_config);
    if (!targets_.try_emplace(target->config.name(), std::move(target)).second)
      throw base::Exception("duplicate targets");
  }

  if (!config.pipe_socket().empty())
    stalker_ = std::make_unique<Stalker>(config, loop_, this, metric_registry_.get());

  web_server_ = std::make_unique<web::Server>(config.listen_port());
  web_server_->AddHandler("/", this);
  if (stalker_)
    web_server_->AddWebsocketHandler("/", kStalkerWebsocketProtocol, this);
}

LogIndex* Server::index(const std::string& target) {
  auto t = targets_.find(target);
  if (t != targets_.end())
    return &t->second->index;
  return nullptr;
}

int Server::HandleGet(const web::Request& req, web::Response* resp) {
  const char* uri = req.uri();
  if (!uri || *uri != '/') {
    FormatError(resp, 500, "missing request uri");
    return 500;
  }

  Target* tgt;
  if (const char* target_uri = StripTarget(uri, &tgt); target_uri)
    return tgt->HandleGet(this, target_uri, req, resp);

#if !defined(NDEBUG)
  std::string ext;
  if (RE2::FullMatch(uri, "/(?:index|log|stalker)\\.(html|css|js)", &ext)) {
    std::string path("esologs/web"); path += uri;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f) {
      web::Writer w(resp, ext == "css" ? "text/css" : "text/javascript", 200);
      std::string buffer;
      constexpr std::size_t kBufferSize = 65536;
      while (true) {
        buffer.resize(kBufferSize);
        std::size_t got = std::fread(buffer.data(), 1, kBufferSize, f);
        if (got == 0)
          break;
        buffer.resize(got);
        w.Write(buffer);
      }
      return 200;
    }
  }
#endif // !defined(NDEBUG)

  FormatError(resp, 404, "unknown path: %s", uri);
  return 404;
}

static std::pair<std::size_t, std::size_t> AppendETag(const FileInfo& info, std::string* headers);
static bool CheckETag(std::string_view etag, std::string_view cond);

static void AppendLastModified(FileInfo::time_type last_write, std::string* headers);
static bool CheckLastModified(FileInfo::time_type last_write, const char* cond);

int Server::Target::HandleGet(Server* srv, const char* uri, const web::Request& req, web::Response* resp) {
  std::string ys, ms, ds, format;

  if (RE2::FullMatch(uri, srv->re_index_, &ys)) {
    int y;

    std::lock_guard<std::mutex> lock(*index.lock());
    index.Refresh();

    if (ys.empty())
      y = index.default_year();
    else if (ys == "all")
      y = -1;
    else
      y = std::stoi(ys);

    FormatIndex(req, resp, config, index, y);
    return 200;
  }

  if (RE2::FullMatch(uri, srv->re_logfile_, &ys, &ms, &ds, &format)) {
    const YMD date(std::stoi(ys), std::stoi(ms), ds.empty() ? 0 : std::stoi(ds));

    bool found;
    std::optional<YMD> prev, next;
    bool stat_ok;
    FileInfo info;
    {
      std::lock_guard<std::mutex> lock(*index.lock());
      index.Refresh();
      found = index.Lookup(date, &prev, &next);
      if (found)
        stat_ok = index.Stat(date, &info);
    }

    if (!found) {
      if (date.day != 0)
        FormatError(resp, 404, "no logs for date: %04d-%02d-%02d", date.year, date.month, date.day);
      else
        FormatError(resp, 404, "no logs for month: %04d-%02d", date.year, date.month);
      return 404;
    }

    std::string extra_headers{};
    if (stat_ok) {
      auto etag_pos = AppendETag(info, &extra_headers);
      AppendLastModified(info.last_write, &extra_headers);
      if (const char* cond = req.header("If-None-Match")) {
        auto etag = std::string_view(extra_headers).substr(etag_pos.first, etag_pos.second);
        if (CheckETag(etag, cond)) {
          FormatErrorWithHeaders(resp, 304, extra_headers, "not modified");
          return 304;
        }
      } else if (const char* cond = req.header("If-Modified-Since")) {
        if (CheckLastModified(info.last_write, cond)) {
          FormatErrorWithHeaders(resp, 304, extra_headers, "not modified");
          return 304;
        }
      }
    }

    auto fmt = CreateFormatter(format, resp, extra_headers);
    if (req.is_head())
      return 200;

    LogEvent event;
    fmt->FormatHeader(date, prev, next, config.title());

    int d_min = date.day ? date.day : 1;
    int d_max = date.day ? date.day : 31;
    for (int d = d_min; d <= d_max; ++d) {
      // TODO: force UTF-8 (with fallback) for non-raw formats

      auto reader = index.Open(date.year, date.month, d);
      if (!reader)
        continue; // shouldn't happen

      fmt->FormatDay(d_min != d_max, date.year, date.month, d);
      while (reader->Read(&event))
        fmt->FormatEvent(event, config);
    }
    fmt->FormatFooter(date, prev, next);

    return 200;
  }

  if (srv->stalker_ && RE2::FullMatch(uri, srv->re_stalker_, &format)) {
    // TODO consider checking for a connected pipe rather than events being loaded
    if (srv->stalker_->loaded()) {
      auto fmt = CreateFormatter(format, resp, std::string_view());
      srv->stalker_->Format(config, fmt.get());
      return 200;
    } else {
      FormatError(resp, 500, "stalker server temporarily unavailable, try again in a minute");
      return 500;
    }
  }

  FormatError(resp, 404, "unknown path: /%s/%s", config.name().c_str(), uri);
  return 404;
}

web::WebsocketClientHandler* Server::HandleWebsocketClient(const char* uri, const char* protocol) {
  CHECK(stalker_);

  Target* tgt;
  if (const char* target_uri = StripTarget(uri, &tgt); target_uri)
    return tgt->HandleWebsocketClient(this, target_uri, protocol);

  LOG(WARNING) << "unexpected websocket request uri: " << uri;
  return nullptr;
}

web::WebsocketClientHandler* Server::Target::HandleWebsocketClient(Server* srv, const char* uri, const char* protocol) {
  if (std::strcmp(uri, "stalker.ws") != 0) {
    LOG(WARNING) << "unexpected websocket request uri: /" << config.name() << '/' << uri;
    return nullptr;
  }

  if (!protocol || std::strcmp(protocol, kStalkerWebsocketProtocol) != 0) {
    LOG(WARNING) << "unexpected websocket protocol: " << (protocol ? protocol : "(none)");
    return nullptr;
  }

  return srv->stalker_->AddClient(config);
}

const char* Server::StripTarget(const char* uri, Target** target) {
  if (!uri || *uri != '/')
    return nullptr;

  const char* sep = std::strchr(uri+1, '/');
  if (!sep)
    return nullptr;

  std::size_t target_len = (sep - uri) - 1;
  std::string_view target_name{uri + 1, target_len};
  auto tgt = targets_.find(target_name);
  if (tgt == targets_.end())
    return nullptr;

  *target = tgt->second.get();
  return sep + 1;
}

std::unique_ptr<LogFormatter> Server::CreateFormatter(const std::string& format, web::Response* resp, std::string_view extra_headers) {
  if (format == ".html")
    return LogFormatter::CreateHTML(resp, extra_headers);
  else if (format == ".txt")
    return LogFormatter::CreateText(resp, extra_headers);
  else
    return LogFormatter::CreateRaw(resp, extra_headers);
}

static std::pair<std::size_t, std::size_t> AppendETag(const FileInfo& info, std::string* headers) {
  headers->append("ETag: ");
  std::size_t start = headers->size();
  if (info.frozen) {
    headers->append("\"frozen\"");
  } else {
    // TODO: consider adding a to_chars convenience append utility to bracket
    char buf[std::numeric_limits<decltype(info.size)>::digits10+1];
    headers->push_back('"');
    *std::to_chars(buf, buf + sizeof buf, info.size_day).ptr = 0;
    headers->append(buf);
    headers->push_back('-');
    *std::to_chars(buf, buf + sizeof buf, info.size).ptr = 0;
    headers->append(buf);
    headers->push_back('"');
  }
  std::size_t end = headers->size();
  headers->append("\r\n");
  return {start, end - start};
}

static bool CheckETag(std::string_view etag, std::string_view cond) {
  while (!cond.empty()) {
    if (cond.starts_with("W/"))
      cond.remove_prefix(2);
    if (!cond.starts_with('"'))
      return false;
    std::size_t end = cond.find('"', 1);
    if (end == cond.npos)
      return false;
    if (cond.substr(0, end+1) == etag)
      return true;
    cond.remove_prefix(end+1);
    if (!cond.starts_with(','))
      return false;
    cond.remove_prefix(1);
    while (cond.starts_with(' '))
      cond.remove_prefix(1);
  }
  return false;
}

static void AppendLastModified(FileInfo::time_type last_write, std::string* headers) {
  auto t = std::chrono::floor<std::chrono::seconds>(last_write);
  std::format_to(
    std::back_inserter(*headers),
    "Last-Modified: {0:%a}, {0:%d} {0:%b} {0:%Y} {0:%T} GMT\r\n",
    t);
}

static bool CheckLastModified(FileInfo::time_type last_write, const char* cond) {
  std::string input{cond};
  std::istringstream is{input};
  FileInfo::time_type cond_time;
  is >> date::parse("%a, %d %b %Y %T GMT", cond_time);
  return is.good() && last_write <= cond_time;
}

} // namespace esologs
