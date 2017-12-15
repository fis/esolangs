#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include <string>
#include <memory>

#include "civetweb.h"

#include "web/response.h"

namespace web {

struct Request {
  virtual const char* uri() const = 0;
  virtual ~Request() = default;
};

struct RequestHandler {
  virtual int HandleGet(const Request& request, Response* response) = 0;
  virtual ~RequestHandler() = default;
};

class Server {
 public:
  Server(const std::string& port);
  ~Server();
  void AddHandler(const char* path, RequestHandler* handler);

 private:
  struct mg_context* civet_ctx_;

  static int CivetRequestHandler(struct mg_connection* conn, void* cb);
};

} // namespace web

#endif // WEB_SERVER_H_

// Local Variables:
// mode: c++
// End:
