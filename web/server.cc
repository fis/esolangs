#include "base/exc.h"
#include "base/log.h"
#include "web/response.h"
#include "web/server.h"

namespace web {

class Server::CivetConnection : public Request, public Response {
 public:
  explicit CivetConnection(struct mg_connection* conn);
  const char* uri() const override;
  void Write(const void* data, std::size_t size) override;

 private:
  struct mg_connection* const conn_;
  const struct mg_request_info* const info_;
};

Server::CivetConnection::CivetConnection(struct mg_connection* conn)
    : conn_(conn), info_(mg_get_request_info(conn))
{}

const char* Server::CivetConnection::uri() const {
  return info_->local_uri;
}

void Server::CivetConnection::Write(const void* data, std::size_t size) {
  mg_write(conn_, data, size);
}

class Server::CivetWebsocket : public Websocket {
 public:
  CivetWebsocket(struct mg_connection* conn, WebsocketClientHandler* handler)
      : conn_(conn), handler_(handler)
  {}

  DISALLOW_COPY(CivetWebsocket);

  std::optional<std::size_t> Write(Type type, const void* buf, std::size_t size) override;
  void Close(Status status) override;

  WebsocketClientHandler* handler() const noexcept { return handler_; }

 private:
  struct Lock {
    Lock(struct mg_connection* conn) : conn_(conn) { mg_lock_connection(conn_); }
    ~Lock() { mg_unlock_connection(conn_); }
   private:
    struct mg_connection* const conn_;
  };

  struct mg_connection* const conn_;
  WebsocketClientHandler* const handler_;
};

std::optional<std::size_t> Server::CivetWebsocket::Write(Type type, const void* buf, std::size_t size) {
  int opcode = 0x80 | (type == Type::kBinary ? 2 : 1);
  int wrote;

  {
    Lock lock(conn_);
    wrote = mg_websocket_write(conn_, opcode, static_cast<const char*>(buf), size);
  }

  if (wrote < 0)
    return std::optional<std::size_t>();
  else
    return std::optional<std::size_t>(wrote);
}

void Server::CivetWebsocket::Close(Status status) {
  unsigned char code[2];
  code[0] = static_cast<unsigned>(status) >> 8;
  code[1] = static_cast<unsigned>(status) >> 8;

  Lock lock(conn_);
  mg_websocket_write(conn_, 0x88, reinterpret_cast<char*>(code), sizeof code);
  // TODO: verify this bit
}

Server::Server(const std::string& port) {
  const char* options[] = {
    "listening_ports", port.c_str(),
    "num_threads", "2",
    "websocket_timeout_ms", "300000",
    // TODO: enable_websocket_ping_pong? needs newer civetweb
    nullptr
  };

  struct mg_callbacks cb = { nullptr };

  civet_ctx_ = mg_start(&cb, this, options);
  if (!civet_ctx_)
    throw base::Exception("civetweb server failed to start");
}

Server::~Server() {
  mg_stop(civet_ctx_);
}

void Server::AddHandler(const char* path, RequestHandler* handler) {
  mg_set_request_handler(civet_ctx_, path, CivetRequestHandler, handler);
}

void Server::AddWebsocketHandler(const char* path, WebsocketHandler* handler) {
  auto* record = &websocket_handlers_.emplace_back(this, handler);
  mg_set_websocket_handler(
      civet_ctx_, path,
      CivetWebsocketConnectHandler,
      CivetWebsocketReadyHandler,
      CivetWebsocketDataHandler,
      CivetWebsocketCloseHandler,
      record);
}

int Server::CivetRequestHandler(struct mg_connection* conn, void* cb) {
  auto* handler = static_cast<RequestHandler*>(cb);
  // TODO: check HTTP method
  CivetConnection c(conn);
  return handler->HandleGet(c, &c);
}

int Server::CivetWebsocketConnectHandler(const struct mg_connection* const_conn, void* cb) {
  enum ReturnCode { kAccept = 0, kClose = 1 };

  auto* record = static_cast<WebsocketHandlerRecord*>(cb);
  auto* conn = const_cast<struct mg_connection*>(const_conn);  // this API is odd

  if (record->server->websocket_clients_.size() >= kMaxWebsocketClients)
    return kClose;

  const struct mg_request_info* info = mg_get_request_info(conn);
  WebsocketClientHandler* client_handler =
      record->handler->HandleWebsocketClient(info->request_uri, info->acceptedWebSocketSubprotocol);

  if (!client_handler)
    return kClose;

  CivetWebsocket* websocket = record->server->websocket_clients_.emplace(conn, client_handler);
  mg_set_user_connection_data(conn, websocket);
  return kAccept;
}

void Server::CivetWebsocketReadyHandler(struct mg_connection* conn, void*) {
  auto* websocket = static_cast<CivetWebsocket*>(mg_get_user_connection_data(conn));
  websocket->handler()->WebsocketReady(websocket);
}

int Server::CivetWebsocketDataHandler(struct mg_connection* conn, int raw_opcode, char* raw_buf, std::size_t size, void*) {
  auto* buf = reinterpret_cast<unsigned char*>(raw_buf);
  auto* websocket = static_cast<CivetWebsocket*>(mg_get_user_connection_data(conn));
  auto* handler = websocket->handler();

  bool fin = raw_opcode & 0x80;
  int opcode = raw_opcode & 0x0f;
  Websocket::Result result;

  if (opcode == 0) {
    // continuation of a previously started fragmented message
    if (handler->fragment_opcode_ == 0) {
      // no data message currently under reassembly
      LOG(WARNING) << "websocket: unexpected continuation frame";
      result = Websocket::Result::kClose;
    } else if (handler->fragment_buffer_.size() + size > handler->fragment_reassembly_limit_) {
      // new message would become too big
      LOG(WARNING)
          << "websocket: fragment reassembly limit exceeded: "
          << handler->fragment_buffer_.size() << " + " << size << " > " << handler->fragment_reassembly_limit_;
      handler->fragment_opcode_ = 0;
      handler->fragment_buffer_.clear();
      result = Websocket::Result::kClose;
    } else {
      // append to buffer, pass to handler if complete
      handler->fragment_buffer_.insert(
          handler->fragment_buffer_.end(),
          buf, buf + size);
      if (fin) {
        result = handler->WebsocketData(
            websocket,
            handler->fragment_opcode_ == 2 ? Websocket::Type::kBinary : Websocket::Type::kText,
            &handler->fragment_buffer_[0], handler->fragment_buffer_.size());
        handler->fragment_opcode_ = 0;
        handler->fragment_buffer_.clear();
      } else {
        result = Websocket::Result::kKeepOpen;
      }
    }
  } else if (!fin) {
    // first frame of a fragmented message
    if (handler->fragment_opcode_ != 0) {
      // existing message under reassembly already
      LOG(WARNING) << "websocket: incomplete fragmented message";
      handler->fragment_opcode_ = 0;
      handler->fragment_buffer_.clear();
      result = Websocket::Result::kClose;
    } else if (opcode != 1 && opcode != 2) {
      // a fragmented non-data message
      LOG(WARNING) << "websocket: unexpected opcode (fragmented): " << opcode;
      result = Websocket::Result::kClose;
    } else {
      // start a new reassembly
      handler->fragment_opcode_ = opcode;
      handler->fragment_buffer_.assign(buf, buf + size);
      result = Websocket::Result::kKeepOpen;
    }
  } else {
    // unfragmented message delivered in a single frame
    if (opcode == 1 || opcode == 2) {
      // regular data frame, just pass to actual handler
      result = handler->WebsocketData(
          websocket,
          opcode == 2 ? Websocket::Type::kBinary : Websocket::Type::kText,
          buf, size);
    } else if (opcode == 8) {
      // websocket close control frame: no action
      result = Websocket::Result::kClose;
    } else if (opcode == 9) {
      // websocket ping control frame, try to reply
      mg_websocket_write(conn, 0x89, raw_buf, size);
      result = Websocket::Result::kKeepOpen;
    } else if (opcode == 10) {
      // websocket pong control frame, regular or unsolicited; ignore
      result = Websocket::Result::kKeepOpen;
    } else {
      // unexpected opcode type
      LOG(WARNING) << "websocket: unexpected opcode: " << opcode;
      websocket->Close(Websocket::Status::kProtocolError);
      result = Websocket::Result::kClose;
    }
  }

  return static_cast<int>(result);
}

void Server::CivetWebsocketCloseHandler(const struct mg_connection* conn, void* cb) {
  auto* record = static_cast<WebsocketHandlerRecord*>(cb);
  auto* websocket = static_cast<CivetWebsocket*>(mg_get_user_connection_data(conn));
  websocket->handler()->WebsocketClose(websocket);
  record->server->websocket_clients_.erase(websocket);
}

} // namespace web
