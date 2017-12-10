#ifndef ESOLOGS_WRITER_H_
#define ESOLOGS_WRITER_H_

#include <chrono>
#include <string>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "esologs/log.pb.h"
#include "proto/delim.h"

namespace esologs {

class Writer {
 public:
  Writer(const std::string& dir);
  void Write(LogEvent* event);

 private:
  using us = std::chrono::microseconds;

  const std::string dir_;

  us current_day_us_;
  std::unique_ptr<proto::DelimWriter> current_log_;

  std::pair<us, us> Now() {
    using namespace std::chrono_literals;
    us now_us = std::chrono::duration_cast<us>(std::chrono::system_clock::now().time_since_epoch());
    us time_us = now_us % 86400000000us;
    return std::pair(now_us - time_us, time_us);
  }

  void OpenLog(us day_us);
};

} // namespace esologs

#endif // ESOLOGS_WRITER_H_

// Local Variables:
// mode: c++
// End:
