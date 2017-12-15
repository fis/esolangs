#include "base/exc.h"
#include "web/response.h"
#include "web/server.h"

namespace web {

namespace internal {

class WrappedCivetConnection : public Request, public Response {
 public:
  WrappedCivetConnection(struct mg_connection* conn);
  const char* uri() const override;
  void Write(const void* data, std::size_t size) override;

 private:
  struct mg_connection* conn_;
  const struct mg_request_info* info_;
};

WrappedCivetConnection::WrappedCivetConnection(struct mg_connection* conn) : conn_(conn) {
  info_ = mg_get_request_info(conn);
}

const char* WrappedCivetConnection::uri() const {
  return info_->local_uri;
}

void WrappedCivetConnection::Write(const void* data, std::size_t size) {
  mg_write(conn_, data, size);
}

} // namespace internal

Server::Server(const std::string& port) {
  const char* options[] = {
    "listening_ports", port.c_str(),
    "num_threads", "2",
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

int Server::CivetRequestHandler(struct mg_connection* conn, void* cb) {
  RequestHandler* handler = static_cast<RequestHandler*>(cb);
  // TODO: check HTTP method
  internal::WrappedCivetConnection c(conn);
  return handler->HandleGet(c, &c);
}

} // namespace web
