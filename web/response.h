#ifndef WEB_RESPONSE_H_
#define WEB_RESPONSE_H_

#include <cstdlib>

namespace web {

struct Response {
  virtual void Write(const void* data, std::size_t size) = 0;
};

} // namespace web

#endif // WEB_RESPONSE_H_

// Local Variables:
// mode: c++
// End:
