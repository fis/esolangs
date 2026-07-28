#ifndef WEB_REQUEST_H_
#define WEB_REQUEST_H_

#include <cstddef>
#include <string_view>

#include "absl/container/flat_hash_map.h"

#include "base/common.h"
#include "web/response.h"

namespace web {

/** Default maximum size for a request. */
constexpr std::size_t kDefaultMaxRequestSize = 1048576; // 1 MiB

enum class Method : unsigned {
  kUnknown = 0,
  kHead = 1,
  kGet = 2,
  kPost = 4,
};

constexpr Method operator|(Method a, Method b) { return static_cast<Method>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }
constexpr bool operator&(Method a, Method b) { return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0; }

struct Request {
  virtual Method method() const = 0;
  virtual const char* uri() const = 0;
  virtual std::string_view query() const = 0;
  virtual const char* header(const char* key) const = 0;
  /**
   * Returns the request body.
   *
   * First call triggers a read, storing the contents in an internal buffer. Subsequent calls use the same buffer.
   * If the request body is larger than the given maximum size, it's simply truncated.
   */
  virtual const std::string& body(const std::size_t max_size = kDefaultMaxRequestSize) = 0;
  /**
   * Returns the parsed form data.
   *
   * If the request has a content-type of application/x-www-form-urlencoded, the body is read and parsed.
   * Otherwise, the query string is parsed instead.
   */
  virtual const absl::flat_hash_map<std::string_view, std::string_view>& form_data() = 0;
  /** Returns a specific element of the parsed form data, or an empty string if not found. */
  virtual std::string_view param(std::string_view key) = 0;

  Request() = default;
  virtual ~Request() = default;
  DISALLOW_COPY(Request);
};

struct RequestHandler {
  virtual int HandleRequest(Request* request, Response* response) = 0;

  RequestHandler() = default;
  virtual ~RequestHandler() = default;
  DISALLOW_COPY(RequestHandler);
};

} // namespace web

#endif // WEB_REQUEST_H_

// Local Variables:
// mode: c++
// End:
