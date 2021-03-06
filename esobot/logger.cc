#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "esobot/logger.h"
#include "esobot/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "irc/bot/module.h"
#include "proto/util.h"

namespace esobot {

Logger::Logger(const LoggerConfig& config, irc::bot::ModuleHost* host) {
  esologs::Config log_config;
  proto::ReadText(config.config_file(), &log_config);

  for (const auto& target_config : config.targets())
    targets_.emplace_back(std::make_unique<Target>(target_config, log_config, host));

  if (!log_config.pipe_socket().empty())
    pipe_ = std::make_unique<esologs::PipeServer>(host->loop(), log_config.pipe_socket());
}

Logger::Target::Target(const LoggerTarget& target, const esologs::Config& log_config, irc::bot::ModuleHost* host)
    : net(target.net()), chan(target.chan()),
      log(log_config, target.config(), host->loop(), host->metric_registry())
{}

void Logger::Log(Connection* conn, const irc::Message& msg, bool sent) {
  for (auto& target : targets_) {
    if (conn->net() != target->net)
      continue;

    bool logged = false;
    if (msg.command_is("PRIVMSG")
        || msg.command_is("NOTICE")
        || msg.command_is("JOIN")
        || msg.command_is("PART")
        || msg.command_is("KICK")
        || msg.command_is("MODE")
        || msg.command_is("TOPIC"))
      logged = msg.arg_is(0, target->chan);
    else if (msg.command_is("QUIT") || msg.command_is("NICK"))
      logged = conn->on_channel(msg.prefix_nick(), target->chan);
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
    target->log.Write(&log_event, pipe_.get());
  }
};

} // namespace esobot
