#ifndef WEB_REQUEST_H_
#define WEB_REQUEST_H_

#include "base/common.h"
#include "web/response.h"

namespace web {

struct Request {
  virtual bool is_head() const = 0;
  virtual const char* uri() const = 0;
  virtual const char* header(const char* key) const = 0;

  Request() = default;
  virtual ~Request() = default;
  DISALLOW_COPY(Request);
};

struct RequestHandler {
  virtual int HandleGet(const Request& request, Response* response) = 0;

  RequestHandler() = default;
  virtual ~RequestHandler() = default;
  DISALLOW_COPY(RequestHandler);
};

} // namespace web

#endif // WEB_REQUEST_H_

// Local Variables:
// mode: c++
// End:
