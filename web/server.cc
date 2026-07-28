#include <algorithm>

#include <strings.h> // strcasecmp

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_split.h"

#include "base/exc.h"
#include "base/log.h"
#include "web/response.h"
#include "web/server.h"

namespace web {

class Server::CivetConnection : public Request, public Response {
 public:
  explicit CivetConnection(struct mg_connection* conn);
  Method method() const override { return method_; }
  const char* uri() const override { return info_->local_uri; }
  std::string_view query() const override { return info_->query_string ? info_->query_string : std::string_view{}; }
  const char* header(const char* key) const override;
  const std::string& body(const std::size_t = kDefaultMaxRequestSize) override;
  const absl::flat_hash_map<std::string_view, std::string_view>& form_data() override;
  std::string_view param(std::string_view key) override;

  void Write(const void* data, std::size_t size) override;

 private:
  std::string_view DecodeAndCache(std::string_view raw);

  struct mg_connection* const conn_;
  const struct mg_request_info* const info_;
  Method const method_;
  bool body_read_ = false;
  std::string body_data_ = {};
  bool form_parsed_ = false;
  absl::flat_hash_map<std::string_view, std::string_view> form_data_ = {};
  std::vector<std::unique_ptr<std::string>> decoded_data_ = {};
};

static Method MethodByName(std::string_view name) {
  if (name == "HEAD") return Method::kHead;
  if (name == "GET") return Method::kGet;
  if (name == "POST") return Method::kPost;
  return Method::kUnknown;
}

Server::CivetConnection::CivetConnection(struct mg_connection* conn)
    : conn_(conn), info_(mg_get_request_info(conn)), method_(MethodByName(info_->request_method))
{}

const char* Server::CivetConnection::header(const char* key) const {
  for (int i = 0; i < info_->num_headers; i++) {
    if (strcasecmp(info_->http_headers[i].name, key) == 0)
      return info_->http_headers[i].value;
  }
  return NULL;
}

const std::string& Server::CivetConnection::body(std::size_t max_size) {
  if (body_read_)
    return body_data_;

  constexpr std::size_t kChunkSize = 65536;
  while (body_data_.size() < max_size) {
    std::size_t body_size = body_data_.size(), buf_size = std::min(body_size + kChunkSize, max_size);
    body_data_.resize(buf_size);
    int got = mg_read(conn_, body_data_.data() + body_size, buf_size - body_size);
    body_data_.resize(body_size + (size_t) std::max(0, got));
    if (got <= 0)
      break;
  }
  return body_data_;
}

const absl::flat_hash_map<std::string_view, std::string_view>& Server::CivetConnection::form_data() {
  if (form_parsed_)
    return form_data_;

  std::string_view data;

  if (auto ct = header("Content-Type"); ct && std::string_view{ct} == "application/x-www-form-urlencoded")
    data = body();
  else
    data = info_->query_string;

  for (auto item : absl::StrSplit(data, '&')) {
    std::string_view key{}, value{};
    if (std::size_t sep = item.find('='); sep != item.npos)
      key = item.substr(0, sep), value = item.substr(sep+1);
    else
      key = item;
    form_data_.emplace(DecodeAndCache(key), DecodeAndCache(value));
  }
  return form_data_;
}

std::string_view Server::CivetConnection::param(std::string_view key) {
  const auto& data = form_data();
  if (auto it = data.find(key); it != data.end())
    return it->second;
  return {};
}

void Server::CivetConnection::Write(const void* data, std::size_t size) {
  mg_write(conn_, data, size);
}

static std::pair<unsigned char, bool> DecodeHexDigit(char ch) {
  if (ch >= '0' && ch <= '9')
    return {ch - '0', true};
  else if (ch >= 'A' && ch <= 'F')
    return {10 + (ch - 'A'), true};
  else if (ch >= 'a' && ch <= 'f')
    return {10 + (ch - 'a'), true};
  return {0, false};
}

static std::pair<unsigned char, bool> DecodeHex(std::string_view text) {
  if (text.size() < 2)
    return {0, false};
  auto [d1, ok1] = DecodeHexDigit(text[0]);
  auto [d2, ok2] = DecodeHexDigit(text[1]);
  if (!ok1 || !ok2)
    return {0, false};
  return {d1 << 4 | d2, true};
}

std::string_view Server::CivetConnection::DecodeAndCache(std::string_view raw) {
  if (raw.find('+') == raw.npos && raw.find('%') == raw.npos)
    return raw; // usable as-is
  auto decoded = std::make_unique<std::string>();
  for (std::size_t i = 0; i < raw.size(); i++) {
    switch (raw[i]) {
    case '+':
      decoded->push_back(' ');
      break;
    case '%':
      if (auto [ch, ok] = DecodeHex(raw.substr(i+1)); ok)
        decoded->push_back(ch), i += 2;
      else
        decoded->push_back('%');
    default:
      decoded->push_back(raw[i]);
      break;
    }
  }
  std::string_view ret = *decoded;
  decoded_data_.emplace_back(std::move(decoded));
  return ret;
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

void Server::Start(const std::string& port) {
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

int Server::port() const {
  struct mg_server_port port_info;
  if (mg_get_server_ports(civet_ctx_, 1, &port_info) != 1)
    throw base::Exception("civetweb listen port unknown");
  return port_info.port;
}

void Server::AddHandler(const char* path, RequestHandler* handler, Method allowed_methods) {
  auto* record = request_handlers_.emplace_back(std::make_unique<RequestHandlerRecord>(handler, allowed_methods)).get();
  mg_set_request_handler(civet_ctx_, path, CivetRequestHandler, record);

}

void Server::AddWebsocketHandler(const char* path, const char* proto, WebsocketHandler* handler) {
  auto* record = websocket_handlers_.emplace_back(std::make_unique<WebsocketHandlerRecord>(this, handler, proto)).get();
  mg_set_websocket_handler_with_subprotocols(
      civet_ctx_, path, &record->proto->proto_list,
      CivetWebsocketConnectHandler,
      CivetWebsocketReadyHandler,
      CivetWebsocketDataHandler,
      CivetWebsocketCloseHandler,
      record);
}

int Server::CivetRequestHandler(struct mg_connection* conn, void* cb) {
  auto* record = static_cast<RequestHandlerRecord*>(cb);
  CivetConnection c(conn);
  if (record->allowed_methods & c.method())
    return record->handler->HandleRequest(&c, &c);
  return 0; // TODO: write 405 Method Not Allowed response?
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
