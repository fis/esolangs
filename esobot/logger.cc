#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>

#include "esobot/logger.h"
#include "esobot/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "irc/bot/module.h"
#include "proto/util.h"

namespace esobot {

namespace fs = std::filesystem;

static constexpr std::uint64_t kRawLogChunkSize = 2*1024*1024; // 2M uncompressed is probably more than a month's worth these days
static constexpr char kRawLogExtension[] = ".pb";

static void FillEvent(esologs::LogEvent* event, const irc::Message& msg, bool sent) {
  if (!msg.prefix().empty())
    event->set_prefix(msg.prefix());
  event->set_command(msg.command());
  for (const auto& arg : msg.args())
    event->add_args(arg);
  if (sent)
    event->set_direction(esologs::LogEvent::SENT);
}

Logger::Logger(const LoggerConfig& config, irc::bot::ModuleHost* host) : raw_path_(config.raw_path()) {
  esologs::Config log_config;
  proto::ReadText(config.config_file(), &log_config);

  for (const auto& target_config : config.targets())
    targets_.emplace_back(std::make_unique<Target>(target_config, log_config, host));

  if (!log_config.pipe_socket().empty())
    pipe_ = std::make_unique<esologs::PipeServer>(host->loop(), log_config.pipe_socket());

  if (!raw_path_.empty())
    raw_files_ = std::make_unique<std::unordered_map<std::string, esologs::FileWriter>>();
}

void Logger::ConnectionConfigured(Connection* conn) {
  conn->EnableCap("chghost");
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
    if (msg.command_is("PRIVMSG") || msg.command_is("NOTICE"))
      logged = msg.arg_is(0, target->chan); // log both received and sent (not echoed)
    else if (msg.command_is("JOIN")
        || msg.command_is("PART")
        || msg.command_is("KICK")
        || msg.command_is("MODE")
        || msg.command_is("TOPIC"))
      logged = !sent && msg.arg_is(0, target->chan); // log only received (echoed)
    else if (msg.command_is("QUIT") || msg.command_is("NICK") || msg.command_is("CHGHOST"))
      logged = !sent && conn->on_channel(msg.prefix_nick(), target->chan); // log only received (echoed), if target on channel
    if (!logged)
      continue;

    esologs::LogEvent log_event;
    FillEvent(&log_event, msg, sent);
    target->log.Write(&log_event, pipe_.get());
  }

  if (!raw_path_.empty()) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_us = std::chrono::floor<std::chrono::microseconds>(now);

    esologs::LogEvent log_event;
    log_event.set_time_us(now_us.count());
    FillEvent(&log_event, msg, sent);

    auto file = RawFile(conn->net());
    file->Write(log_event);
    if (file->bytes() >= kRawLogChunkSize)
      CloseRawFile(conn->net(), now_us.count());
  }
};

esologs::FileWriter* Logger::RawFile(const std::string& net) {
  auto old = raw_files_->find(net);
  if (old != raw_files_->end())
    return &old->second;

  auto path = RawFilePath(net);
  auto it = raw_files_->try_emplace(net, path);
  return &it.first->second;
}

void Logger::CloseRawFile(const std::string& net, std::uint64_t time) {
  raw_files_->erase(net);

  char suffix[18];
  std::snprintf(suffix, sizeof suffix, "-%016" PRIx64, time);

  auto old_path = RawFilePath(net);
  fs::path new_path{raw_path_};
  new_path /= net;
  new_path += suffix;
  new_path += kRawLogExtension;

  fs::rename(old_path, new_path);
}

fs::path Logger::RawFilePath(const std::string& net) {
  fs::path path{raw_path_};
  path /= net;
  path += kRawLogExtension;
  return path;
}

} // namespace esobot
