#ifndef ESOBOT_LOGGER_H_
#define ESOBOT_LOGGER_H_

#include "esobot/config.pb.h"
#include "esologs/writer.h"
#include "irc/bot/plugin.h"

namespace esobot {

class Logger : public irc::bot::Plugin {
 public:
  Logger(const LoggerConfig& config, irc::bot::PluginHost*);
  void MessageReceived(const irc::Message& msg) override { Log(msg, /* sent: */ false); }
  void MessageSent(const irc::Message& msg) override { Log(msg, /* sent: */ true); }

 private:
  const std::string channel_;
  esologs::Writer log_;

  void Log(const irc::Message& msg, bool sent);
};

} // namespace esobot

#endif // ESOBOT_LOGGER_H_

// Local Variables:
// mode: c++
// End:
