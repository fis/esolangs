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
  Server(const std::string& port);
  ~Server();
  DISALLOW_COPY(Server);

  int port() const;

  void AddHandler(const char* path, RequestHandler* handler);
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

  struct WebsocketHandlerRecord {
    Server* server;
    WebsocketHandler* handler;
    std::unique_ptr<WebsocketHandlerProto> proto;
    WebsocketHandlerRecord(Server* s, WebsocketHandler* h, const char* p) : server(s), handler(h), proto(std::make_unique<WebsocketHandlerProto>(p)) {}
  };

  static constexpr std::size_t kMaxWebsocketClients = 256;

  struct mg_context* civet_ctx_;
  std::vector<WebsocketHandlerRecord> websocket_handlers_;
  base::unique_set<CivetWebsocket> websocket_clients_;

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
