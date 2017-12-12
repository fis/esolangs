#include <cstdio>
#include <cstring>
#include <experimental/filesystem>
#include <memory>
#include <string>

#include "CivetServer.h"
#include "civetweb.h"
#include "re2/re2.h"

#include "base/log.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "proto/brotli.h"
#include "proto/delim.h"

extern "C" {
#include <stdlib.h>
}

namespace esologs {

namespace fs = std::experimental::filesystem;

class Server : public CivetHandler {
 public:
  Server(const std::string& root);

  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  const fs::path root_;
  LogIndex index_;

  const RE2 re_logfile_;

  std::unique_ptr<CivetServer> web_server_;
};

Server::Server(const std::string& root)
    : root_(root), index_(root), re_logfile_("/(\\d+)-(\\d+)-(\\d+)(\\.html|\\.txt|-raw\\.txt)")
{
  const char* options[] = {
    "listening_ports", "13280",
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

  if (std::strcmp(info->local_uri, "/") == 0) {
    FormatIndex(conn, index_);
    return true;
  }

  std::string ys, ms, ds, format;
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

int main(void) {
  setenv("TZ", "UTC", 1);
  esologs::Server server("logs");
  LOG(INFO) << "server started";
  std::getchar();
}
