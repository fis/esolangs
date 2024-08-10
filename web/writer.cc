#include "web/writer.h"

namespace web {

Writer::Writer(Response* resp, const char* content_type, int response_code, std::string_view extra_headers)
    : resp_(resp),
      owned_buffer_(new std::string),
      buffer_(owned_buffer_.get())
{
  Write(
      "HTTP/1.1 ", response_code, " OK\r\n"
      "Content-Type: ", content_type, "\r\n",
      extra_headers, "\r\n");
}

void Writer::Flush(bool force) {
  if (!resp_)
    return;  // nowhere to flush to
  if (force ? !buffer_->empty() : buffer_->size() >= kFlushAt) {
    resp_->Write(buffer_->data(), buffer_->size());
    buffer_->clear();
  }
}

} // namespace web
