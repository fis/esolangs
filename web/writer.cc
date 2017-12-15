#include "web/writer.h"

namespace web {

Writer::Writer(struct mg_connection* conn, const char* content_type, int response_code) : conn_(conn) {
  Write(
      "HTTP/1.1 ", response_code, " OK\r\n"
      "Content-Type: ", content_type, "\r\n\r\n");
}

void Writer::Flush(bool force) {
  if (force ? !buffer_.empty() : buffer_.size() >= kFlushAt) {
    mg_write(conn_, buffer_.data(), buffer_.size());
    buffer_.clear();
  }
}

} // namespace web
