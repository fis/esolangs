#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "esobot/logger.h"
#include "esobot/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "irc/bot/plugin.h"

namespace esobot {

Logger::Logger(const LoggerConfig& config, irc::bot::PluginHost* host)
    : channel_(config.channel()), log_(config.config_file(), host->loop())
{}

void Logger::Log(const irc::Message& msg, bool sent) {
  bool logged =
      ((msg.command_is("PRIVMSG")
        || msg.command_is("NOTICE")
        || msg.command_is("JOIN")
        || msg.command_is("PART")
        || msg.command_is("KICK")
        || msg.command_is("MODE")
        || msg.command_is("TOPIC"))
       && msg.arg_is(0, channel_.c_str()))
      || msg.command_is("QUIT")
      || msg.command_is("NICK");
  if (!logged)
    return;

  esologs::LogEvent log_event;
  if (!msg.prefix().empty())
    log_event.set_prefix(msg.prefix());
  log_event.set_command(msg.command());
  for (const auto& arg : msg.args())
    log_event.add_args(arg);
  if (sent)
    log_event.set_direction(esologs::LogEvent::SENT);
  log_.Write(&log_event);
};

} // namespace esobot
