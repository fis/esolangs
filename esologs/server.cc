#include <chrono>
#include <cstdio>
#include <cstring>
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

#if 0 // temporarily disabled
  if (!config.stalker_socket().empty())
    stalker_ = std::make_unique<Stalker>(config, loop_, index_.get(), metric_registry_.get());
#endif

  web_server_ = std::make_unique<web::Server>(config.listen_port());
  web_server_->AddHandler("/", this);
  if (stalker_)
    web_server_->AddWebsocketHandler("/api/stalker.ws", this);
}

int Server::HandleGet(const web::Request& req, web::Response* resp) {
  const char* uri = req.uri();
  if (!uri || *uri != '/') {
    FormatError(resp, 500, "local_uri missing");
    return 500;
  }

  if (const char *sep = std::strchr(uri+1, '/'); sep != NULL) {
    std::size_t target_len = (sep - uri) - 1;
    std::string_view target_name{uri + 1, target_len};
    auto target = targets_.find(target_name);
    if (target != targets_.end())
      return target->second->HandleGet(this, sep + 1, resp);
    FormatError(resp, 404, "unknown path: %s", uri);
    return 404;
  }

#if 0 // temporarily disabled
  if (stalker_ && RE2::FullMatch(uri, re_stalker_, &format)) {
    if (stalker_->loaded()) {
      auto fmt = CreateFormatter(format, resp);
      stalker_->Format(fmt.get());
      return 200;
    } else {
      FormatError(resp, 500, "stalker server temporarily unavailable, try again in a minute");
      return 500;
    }
  }
#endif

#undef NDEBUG // TODO FIXME
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

int Server::Target::HandleGet(Server* srv, const char *uri, web::Response *resp) {
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

    FormatIndex(resp, config, index, y);
    return 200;
  }

  if (RE2::FullMatch(uri, srv->re_logfile_, &ys, &ms, &ds, &format)) {
    const YMD date(std::stoi(ys), std::stoi(ms), ds.empty() ? 0 : std::stoi(ds));

    bool found;
    std::optional<YMD> prev, next;
    {
      std::lock_guard<std::mutex> lock(*index.lock());
      index.Refresh();
      found = index.Lookup(date, &prev, &next);
    }

    if (!found) {
      if (date.day != 0)
        FormatError(resp, 404, "no logs for date: %04d-%02d-%02d", date.year, date.month, date.day);
      else
        FormatError(resp, 404, "no logs for month: %04d-%02d", date.year, date.month);
      return 404;
    }

    auto fmt = CreateFormatter(format, resp);

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

  FormatError(resp, 404, "unknown path: %s", uri);
  return 404;
}

web::WebsocketClientHandler* Server::HandleWebsocketClient(const char* uri, const char* protocol) {
  CHECK(stalker_);

  if (std::strcmp(uri, "/api/stalker.ws") != 0) {
    LOG(WARNING) << "unexpected websocket request uri: " << uri;
    return nullptr;
  }

  if (!protocol || std::strcmp(protocol, "v1.stalker.logs.esolangs.org") != 0) {
    LOG(WARNING) << "unexpected websocket protocol: " << (protocol ? protocol : "(none)");
    return nullptr;
  }

  return stalker_->AddClient();
}

std::unique_ptr<LogFormatter> Server::CreateFormatter(const std::string& format, web::Response* resp) {
  if (format == ".html")
    return LogFormatter::CreateHTML(resp);
  else if (format == ".txt")
    return LogFormatter::CreateText(resp);
  else
    return LogFormatter::CreateRaw(resp);
}

} // namespace esologs
