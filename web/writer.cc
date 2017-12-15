#include "web/writer.h"

namespace web {

Writer::Writer(Response* resp, const char* content_type, int response_code) : resp_(resp) {
  Write(
      "HTTP/1.1 ", response_code, " OK\r\n"
      "Content-Type: ", content_type, "\r\n\r\n");
}

void Writer::Flush(bool force) {
  if (force ? !buffer_.empty() : buffer_.size() >= kFlushAt) {
    resp_->Write(buffer_.data(), buffer_.size());
    buffer_.clear();
  }
}

} // namespace web
