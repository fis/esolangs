#include <chrono>
#include <cstdio>
#include <cstring>
#include <experimental/filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "re2/re2.h"

#include "base/log.h"
#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "proto/brotli.h"
#include "proto/delim.h"
#include "proto/util.h"
#include "web/server.h"

extern "C" {
#include <stdlib.h>
}

namespace esologs {

namespace fs = std::experimental::filesystem;

class Server : public web::RequestHandler {
 public:
  Server(const char* config_file);

  int HandleGet(const web::Request& req, web::Response* resp) override;

 private:
  fs::path root_;
  std::string nick_;

  std::unique_ptr<LogIndex> index_;
  std::mutex index_lock_;

  const RE2 re_index_ = RE2("/(?:(\\d+|all)\\.html)?");
  const RE2 re_logfile_ = RE2("/(\\d+)-(\\d+)(?:-(\\d+))?(\\.html|\\.txt|-raw\\.txt)");

  std::unique_ptr<web::Server> web_server_;
};

Server::Server(const char* config_file) {
  Config config;
  proto::ReadText(config_file, &config);

  if (config.listen_port().empty())
    throw base::Exception("missing required setting: listen_port");
  if (config.log_path().empty())
    throw base::Exception("missing required setting: log_path");
  if (config.nick().empty())
    throw base::Exception("missing required setting: nick");

  root_ = config.log_path();
  nick_ = config.nick();

  index_ = std::make_unique<LogIndex>(root_);

  web_server_ = std::make_unique<web::Server>(config.listen_port());
  web_server_->AddHandler("/", this);
}

int Server::HandleGet(const web::Request& req, web::Response* resp) {
  const char* uri = req.uri();
  if (!uri) {
    FormatError(resp, 500, "local_uri missing");
    return 500;
  }

  std::string ys, ms, ds, format;

  if (RE2::FullMatch(uri, re_index_, &ys)) {
    int y;

    std::lock_guard<std::mutex> lock(index_lock_);
    index_->Refresh();

    if (ys.empty())
      y = index_->default_year();
    else if (ys == "all")
      y = -1;
    else
      y = std::stoi(ys);

    FormatIndex(resp, *index_, y);
    return 200;
  }

  if (RE2::FullMatch(uri, re_logfile_, &ys, &ms, &ds, &format)) {
    const YMD date(std::stoi(ys), std::stoi(ms), ds.empty() ? 0 : std::stoi(ds));

    bool found;
    std::optional<YMD> prev, next;
    {
      std::lock_guard<std::mutex> lock(index_lock_);
      index_->Refresh();
      found = index_->Lookup(date, &prev, &next);
    }

    if (!found) {
      if (date.day != 0)
        FormatError(resp, 404, "no logs for date: %04d-%02d-%02d", date.year, date.month, date.day);
      else
        FormatError(resp, 404, "no logs for month: %04d-%02d", date.year, date.month);
      return 404;
    }

    std::unique_ptr<LogFormatter> fmt;
    if (format == ".html")
      fmt = LogFormatter::CreateHTML(resp);
    else if (format == ".txt")
      fmt = LogFormatter::CreateText(resp);
    else
      fmt = LogFormatter::CreateRaw(resp);

    LogEvent event;
    fmt->FormatHeader(date, prev, next);

    int d_min = date.day ? date.day : 1;
    int d_max = date.day ? date.day : 31;
    for (int d = d_min; d <= d_max; ++d) {
      fs::path logfile = root_;
      logfile /= std::to_string(date.year);
      logfile /= std::to_string(date.month);
      logfile /= std::to_string(d);
      logfile += ".pb";

      std::unique_ptr<proto::DelimReader> reader;

      if (fs::is_regular_file(logfile)) {
        reader = std::make_unique<proto::DelimReader>(logfile.c_str());
      } else {
        logfile += ".br";
        if (fs::is_regular_file(logfile))
          reader = std::make_unique<proto::DelimReader>(proto::BrotliInputStream::FromFile(logfile.c_str()));
        else
          continue; // shouldn't happen
      }

      fmt->FormatDay(d_min != d_max, date.year, date.month, d);
      while (reader->Read(&event))
        fmt->FormatEvent(event);
    }
    fmt->FormatFooter(date, prev, next);

    return 200;
  }

  FormatError(resp, 404, "unknown path: %s", uri);
  return 404;
}

} // namespace esologs

int main(int argc, char* argv[]) {
  if (argc != 2) {
    LOG(ERROR) << "usage: " << argv[0] << " <esologs.config>";
    return 1;
  }

  setenv("TZ", "UTC", 1);
  esologs::Server server(argv[1]);
  LOG(INFO) << "server started";

  while (true)  // TODO: event loop for background tasks
    std::this_thread::sleep_for(std::chrono::hours(1));
}
