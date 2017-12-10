#ifndef ESOBOT_LOGGER_H_
#define ESOBOT_LOGGER_H_

#include <chrono>
#include <string>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "irc/connection.h"
#include "irc/message.h"
#include "proto/delim.h"

namespace esobot {

class Logger {
 public:
  Logger(const std::string& dir, const std::string& channel);
  void Log(const irc::Message& msg, bool sent = false);
  void Send(irc::Connection* conn, const irc::Message& msg) {
    Log(msg, /* sent: */ true);
    conn->Send(msg);
  }

 private:
  using us = std::chrono::microseconds;

  const std::string dir_;
  const std::string channel_;

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

} // namespace esobot

#endif // ESOBOT_LOGGER_H_

// Local Variables:
// mode: c++
// End:
