#ifndef ESOLOGS_WRITER_H_
#define ESOLOGS_WRITER_H_

#include <chrono>
#include <cstdint>
#include <string>

#include <date/date.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "esologs/log.pb.h"
#include "proto/delim.h"

namespace esologs {

class Writer {
 public:
  Writer(const std::string& dir);
  void Write(LogEvent* event);

 private:
  const std::string dir_;

  date::sys_days current_day_;
  std::unique_ptr<proto::DelimWriter> current_log_;

  std::pair<date::sys_days, std::uint64_t> Now() {
    auto now = std::chrono::system_clock::now();
    auto day = date::floor<date::days>(now);
    auto time = date::floor<std::chrono::microseconds>(now - day);
    return std::pair(day, time.count());
  }

  void OpenLog(date::sys_days day);
};

} // namespace esologs

#endif // ESOLOGS_WRITER_H_

// Local Variables:
// mode: c++
// End:
