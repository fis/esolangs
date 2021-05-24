#ifndef ESOBOT_LOGGER_H_
#define ESOBOT_LOGGER_H_

#include "esobot/config.pb.h"
#include "esologs/writer.h"
#include "irc/bot/module.h"

namespace esobot {

class Logger : public irc::bot::Module {
  using Connection = irc::bot::Connection;

 public:
  Logger(const LoggerConfig& config, irc::bot::ModuleHost* host);
  void MessageReceived(Connection* conn, const irc::Message& msg) override { Log(conn, msg, /* sent: */ false); }
  void MessageSent(Connection* conn, const irc::Message& msg) override { Log(conn, msg, /* sent: */ true); }

 private:
  struct Target {
    const std::string net;
    const std::string chan;
    esologs::Writer log;
    Target(const LoggerConfig& config, const LoggerTarget& target, irc::bot::ModuleHost* host);
  };
  std::vector<std::unique_ptr<Target>> targets_;

  void Log(Connection* conn, const irc::Message& msg, bool sent);
};

} // namespace esobot

#endif // ESOBOT_LOGGER_H_

// Local Variables:
// mode: c++
// End:
