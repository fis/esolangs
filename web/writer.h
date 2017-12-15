#ifndef WEB_WRITER_H_
#define WEB_WRITER_H_

#include <cstdlib>
#include <string>
#include <utility>

#include "web/response.h"

namespace web {

class Writer {
 public:
  Writer(Response* resp, const char* content_type, int response_code=200);
  ~Writer() { Flush(true); }

  template <typename... Args>
  void Write(Args&&... args) {
    DoWrite(std::forward<Args>(args)...);
    Flush();
  }

 private:
  static constexpr std::size_t kFlushAt = 8192;

  std::string buffer_;
  Response* resp_;

  template <typename Arg1, typename... Args>
  void DoWrite(Arg1&& arg1, Args&&... args) {
    Append(arg1);
    DoWrite(std::forward<Args>(args)...);
  }
  void DoWrite() {}

  void Append(const char* s) { buffer_ += s; }
  void Append(const std::string& s) { buffer_ += s; }

  template <typename Number>
  auto Append(Number n) -> decltype((void)std::to_string(n), void()) {
    buffer_ += std::to_string(n);
  }

  template <typename T>
  auto Append(const T& t) -> decltype(WebWrite(this, t)) {
    WebWrite(this, t);
  }

  void Flush(bool force=false);
};

} // namespace web

#endif // WEB_WRITER_H_

// Local Variables:
// mode: c++
// End:
