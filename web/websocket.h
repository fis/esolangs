#ifndef WEB_WEBSOCKET_H_
#define WEB_WEBSOCKET_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/common.h"

namespace web {

class Server;

struct Websocket {
  enum class Result { kClose = 0, kKeepOpen = 1 };  // must match civetweb return codes
  enum class Type { kText, kBinary };
  enum class Status : std::uint16_t {
    kOk = 1000,
    kGoingAway = 1001,
    kProtocolError = 1002,
    kInvalidData = 1003,
    kPolicyViolation = 1008,
    kTooBig = 1009,
    kInternalError = 1011,
  };

  virtual std::optional<std::size_t> Write(Type type, const void* buf, std::size_t size) = 0;
  virtual void Close(Status status = Status::kOk) = 0;

  Websocket() = default;
  virtual ~Websocket() = default;
  DISALLOW_COPY(Websocket);
};

struct WebsocketClientHandler {
  explicit WebsocketClientHandler(std::size_t fragment_reassembly_limit = 4096)
      : fragment_reassembly_limit_(fragment_reassembly_limit)
  {}

  virtual void WebsocketReady(Websocket* socket) = 0;
  virtual Websocket::Result WebsocketData(Websocket* socket, Websocket::Type type, const unsigned char* buf, std::size_t size) = 0;

  /**
   * Called to indicate that the client has disconnected.
   *
   * Once this method returns, the \p socket object can no longer be used. If you have other threads
   * that might call Websocket::Write(), you probably want to arrange for some explicit
   * synchronization with them.
   */
  virtual void WebsocketClose(Websocket* socket) = 0;

  virtual ~WebsocketClientHandler() = default;
  DISALLOW_COPY(WebsocketClientHandler);

 private:
  // these fields are used for internal fragment reassembly
  friend class Server;
  const std::size_t fragment_reassembly_limit_;
  int fragment_opcode_ = 0;
  std::vector<unsigned char> fragment_buffer_;
};

struct WebsocketHandler {
  /**
   * Called to indicate a new websocket connection has been requested.
   *
   * The return value can be `null` to refuse the client, or a pointer to a WebsocketClientHandler
   * that will then receive any callbacks related to the connection.
   *
   * The returned WebsocketClientHandler (when not `null`) must stay valid until the
   * WebsocketClientHandler::Close() method has been called by the server.
   *
   * To initiate closing the connection from the server side, call the Websocket::Close() method
   * on the Websocket object received by the handler. This will eventually cause a call to
   * WebsocketClientHandler::Close().
   */
  virtual WebsocketClientHandler* HandleWebsocketClient(const char* uri, const char* protocol) = 0;

  WebsocketHandler() = default;
  virtual ~WebsocketHandler() = default;
  DISALLOW_COPY(WebsocketHandler);
};

} // namespace web

#endif // WEB_WEBSOCKET_H_

// Local Variables:
// mode: c++
// End:
