#ifndef ESOLOGS_WRITER_H_
#define ESOLOGS_WRITER_H_

#include <chrono>
#include <cstdint>
#include <string>

#include <date/date.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "base/common.h"
#include "esologs/log.pb.h"
#include "event/loop.h"
#include "proto/delim.h"

namespace esologs {

class Writer {
 public:
  Writer(const std::string& config_file, event::Loop* loop);
  ~Writer();

  void Write(LogEvent* event);

  DISALLOW_COPY(Writer);

 private:
  class Stalker;

  event::Loop* loop_;

  std::string dir_;

  date::sys_days current_day_;
  std::unique_ptr<proto::DelimWriter> current_log_;
  std::uint64_t current_line_;  // index of next line to be written

  std::unique_ptr<Stalker> stalker_;

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
