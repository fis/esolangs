#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "civetweb.h"

#include "base/common.h"
#include "base/unique_set.h"
#include "web/request.h"
#include "web/response.h"
#include "web/websocket.h"

namespace web {

class Server {
 public:
  Server(const std::string& port) { Start(port); }
  Server(std::string_view port) { std::string p{port}; Start(p); }
  ~Server();
  DISALLOW_COPY(Server);

  int port() const;

  void AddHandler(const char* path, RequestHandler* handler, Method allowed_methods = Method::kHead | Method::kGet);
  void AddWebsocketHandler(const char* path, const char* proto, WebsocketHandler* handler);

 private:
  class CivetConnection;
  class CivetWebsocket;

  struct WebsocketHandlerProto {
    WebsocketHandlerProto(const char *p) : proto(p) {
      proto_list.nb_subprotocols = 1;
      proto_list.subprotocols = &proto;
    }
    const char *proto;
    struct mg_websocket_subprotocols proto_list;
  };

  struct RequestHandlerRecord {
    RequestHandler* handler;
    Method allowed_methods;
    RequestHandlerRecord(RequestHandler* h, Method ms) : handler(h), allowed_methods(ms) {}
  };

  struct WebsocketHandlerRecord {
    Server* server;
    WebsocketHandler* handler;
    std::unique_ptr<WebsocketHandlerProto> proto;
    WebsocketHandlerRecord(Server* s, WebsocketHandler* h, const char* p) : server(s), handler(h), proto(std::make_unique<WebsocketHandlerProto>(p)) {}
  };

  void Start(const std::string& port);

  static constexpr std::size_t kMaxWebsocketClients = 256;

  struct mg_context* civet_ctx_;
  std::vector<std::unique_ptr<RequestHandlerRecord>> request_handlers_;
  std::vector<std::unique_ptr<WebsocketHandlerRecord>> websocket_handlers_;
  base::unique_set<CivetWebsocket> websocket_clients_; // TODO: this may need to see the full definition of CivetWebsocket for proper destroying

  static int CivetRequestHandler(struct mg_connection* conn, void* cb);
  static int CivetWebsocketConnectHandler(const struct mg_connection* conn, void* cb);
  static void CivetWebsocketReadyHandler(struct mg_connection* conn, void*);
  static int CivetWebsocketDataHandler(struct mg_connection* conn, int opcode, char* buf, std::size_t size, void*);
  static void CivetWebsocketCloseHandler(const struct mg_connection* conn, void* cb);
};

} // namespace web

#endif // WEB_SERVER_H_

// Local Variables:
// mode: c++
// End:
