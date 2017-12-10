#ifndef ESOBOT_LOGGER_H_
#define ESOBOT_LOGGER_H_

#include <string>

#include "esologs/writer.h"
#include "irc/connection.h"
#include "irc/message.h"

namespace esobot {

class Logger {
 public:
  Logger(const std::string& channel, const std::string& dir);
  void Log(const irc::Message& msg, bool sent = false);
  void Send(irc::Connection* conn, const irc::Message& msg) {
    Log(msg, /* sent: */ true);
    conn->Send(msg);
  }

 private:
  const std::string channel_;
  esologs::Writer log_;
};

} // namespace esobot

#endif // ESOBOT_LOGGER_H_

// Local Variables:
// mode: c++
// End:
