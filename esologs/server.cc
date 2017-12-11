#include <cstdio>
#include <cstring>
#include <experimental/filesystem>
#include <memory>
#include <string>

#include "CivetServer.h"
#include "civetweb.h"
#include "re2/re2.h"

#include "esologs/format.h"
#include "esologs/index.h"
#include "proto/delim.h"

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
    : root_(root), index_(root), re_logfile_("/(\\d+)-(\\d+)-(\\d+)\\.(html|txt)")
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

    if (!fs::is_regular_file(logfile)) {
      FormatError(conn, 404, "no logs for date: %04d-%02d-%02d", y, m, d);
      return true;
    }

    proto::DelimReader reader(logfile.c_str());
    std::unique_ptr<LogFormatter> fmt =
        format == "html" ? LogFormatter::CreateHTML(conn) : LogFormatter::CreateText(conn);

    LogEvent event;
    while (reader.Read(&event))
      fmt->FormatEvent(event);

    return true;
  }

  FormatError(conn, 404, "unknown path: %s", info->local_uri);
  return true;
}

} // namespace esologs

int main(void) {
  esologs::Server server("tmp");
  std::getchar();
}
