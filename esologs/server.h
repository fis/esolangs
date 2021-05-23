#ifndef ESOLOGS_SERVER_H_
#define ESOLOGS_SERVER_H_

#include <unordered_map>

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "re2/re2.h"

#include "event/loop.h"
#include "esologs/config.pb.h"
#include "esologs/index.h"
#include "esologs/stalker.h"
#include "web/server.h"

namespace esologs {

class Server : public web::RequestHandler, public web::WebsocketHandler {
 public:
  Server(const Config& config, event::Loop* loop);

  int port() const { return web_server_->port(); }

  int HandleGet(const web::Request& req, web::Response* resp) override;
  web::WebsocketClientHandler* HandleWebsocketClient(const char* uri, const char* protocol) override;

 private:
  struct Target {
    TargetConfig config;
    LogIndex index;
    Target(const TargetConfig& c) : config(c), index(c.log_path()) {}
    int HandleGet(Server* srv, const char *uri, web::Response* resp);
  };

  event::Loop* loop_;

  std::unordered_map<std::string_view, std::unique_ptr<Target>> targets_;
  std::unique_ptr<Stalker> stalker_;

  std::unique_ptr<prometheus::Exposer> metric_exposer_;
  std::shared_ptr<prometheus::Registry> metric_registry_;

  const RE2 re_index_ = RE2("(?:(\\d+|all)\\.html)?");
  const RE2 re_logfile_ = RE2("(\\d+)-(\\d+)(?:-(\\d+))?(\\.html|\\.txt|-raw\\.txt)");
  const RE2 re_stalker_ = RE2("/stalker(\\.html|\\.txt|-raw\\.txt)");

  std::unique_ptr<web::Server> web_server_;

  static std::unique_ptr<LogFormatter> CreateFormatter(const std::string& format, web::Response* resp);
};

} // namespace esologs

#endif // ESOLOGS_SERVER_H_

// Local Variables:
// mode: c++
// End:
