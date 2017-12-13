#include <chrono>
#include <cstdio>
#include <cstring>
#include <experimental/filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "CivetServer.h"
#include "civetweb.h"
#include "re2/re2.h"

#include "base/log.h"
#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "proto/brotli.h"
#include "proto/delim.h"
#include "proto/util.h"

extern "C" {
#include <stdlib.h>
}

namespace esologs {

namespace fs = std::experimental::filesystem;

class Server : public CivetHandler {
 public:
  Server(const char* config_file);

  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  fs::path root_;
  std::string nick_;

  std::unique_ptr<LogIndex> index_;
  std::mutex index_lock_;

  const RE2 re_index_ = RE2("/(?:(\\d+|all)\\.html)?");
  const RE2 re_logfile_ = RE2("/(\\d+)-(\\d+)-(\\d+)(\\.html|\\.txt|-raw\\.txt)");

  std::unique_ptr<CivetServer> web_server_;
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

  const char* options[] = {
    "listening_ports", config.listen_port().c_str(),
    "num_threads", "2",
    nullptr
  };
  web_server_ = std::make_unique<CivetServer>(options);
  web_server_->addHandler("/", this);
}

bool Server::handleGet(CivetServer* server, struct mg_connection* conn) {
  const struct mg_request_info* info = mg_get_request_info(conn);

  if (!info->local_uri) {
    FormatError(conn, 500, "local_uri missing");
    return true;
  }

  std::string ys, ms, ds, format;

  if (RE2::FullMatch(info->local_uri, re_index_, &ys)) {
    int y;

    std::lock_guard<std::mutex> lock(index_lock_);
    index_->Refresh();

    if (ys.empty())
      y = index_->default_year();
    else if (ys == "all")
      y = -1;
    else
      y = std::stoi(ys);

    FormatIndex(conn, index_.get(), y);
    return true;
  }

  if (RE2::FullMatch(info->local_uri, re_logfile_, &ys, &ms, &ds, &format)) {
    int y = std::stoi(ys), m = std::stoi(ms), d = std::stoi(ds);

    fs::path logfile = root_;
    logfile /= std::to_string(y);
    logfile /= std::to_string(m);
    logfile /= std::to_string(d);
    logfile += ".pb";

    std::unique_ptr<proto::DelimReader> reader;

    if (fs::is_regular_file(logfile)) {
      reader = std::make_unique<proto::DelimReader>(logfile.c_str());
    } else {
      logfile += ".br";
      if (fs::is_regular_file(logfile)) {
        reader = std::make_unique<proto::DelimReader>(proto::BrotliInputStream::FromFile(logfile.c_str()));
      } else {
        FormatError(conn, 404, "no logs for date: %04d-%02d-%02d", y, m, d);
        return true;
      }
    }

    std::unique_ptr<LogFormatter> fmt;
    if (format == ".html")
      fmt = LogFormatter::CreateHTML(conn);
    else if (format == ".txt")
      fmt = LogFormatter::CreateText(conn);
    else
      fmt = LogFormatter::CreateRaw(conn);

    LogEvent event;
    fmt->FormatHeader(y, m, d);
    while (reader->Read(&event))
      fmt->FormatEvent(event);
    fmt->FormatFooter();

    return true;
  }

  FormatError(conn, 404, "unknown path: %s", info->local_uri);
  return true;
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
