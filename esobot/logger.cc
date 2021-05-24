#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "esobot/logger.h"
#include "esobot/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "irc/bot/module.h"

namespace esobot {

Logger::Logger(const LoggerConfig& config, irc::bot::ModuleHost* host) {
  for (const auto& target_config : config.targets())
    targets_.emplace_back(std::make_unique<Target>(config, target_config, host));
}

Logger::Target::Target(const LoggerConfig& config, const LoggerTarget& target, irc::bot::ModuleHost* host)
    : net(target.net()), chan(target.chan()),
      log(config.config_file(), target.config(), host->loop(), host->metric_registry())
{}

void Logger::Log(Connection* conn, const irc::Message& msg, bool sent) {
  for (auto& target : targets_) {
    if (conn->net() != target->net)
      continue;

    bool logged =
        ((msg.command_is("PRIVMSG")
          || msg.command_is("NOTICE")
          || msg.command_is("JOIN")
          || msg.command_is("PART")
          || msg.command_is("KICK")
          || msg.command_is("MODE")
          || msg.command_is("TOPIC"))
         && msg.arg_is(0, target->chan))
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
    target->log.Write(&log_event);
  }
};

} // namespace esobot
