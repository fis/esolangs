#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <experimental/filesystem>
#include <string>

//#include "base/log.h"
#include "esobot/log.pb.h"
#include "esobot/logger.h"
#include "irc/config.pb.h"
#include "irc/connection.h"
#include "irc/message.h"

namespace esobot {

namespace fs = std::experimental::filesystem;

Logger::Logger(const char* dir, const char* channel) : dir_(dir), channel_(channel) {
  current_day_us_ = Now().first;
  OpenLog(current_day_us_);
}

void Logger::Log(const irc::Message& msg, bool sent) {
  auto [day_us, time_us] = Now();

  bool logged =
      ((msg.command_is("PRIVMSG")
        || msg.command_is("NOTICE")
        || msg.command_is("JOIN")
        || msg.command_is("PART")
        || msg.command_is("KICK")
        || msg.command_is("MODE")
        || msg.command_is("TOPIC"))
       && msg.arg_is(0, channel_))
      || msg.command_is("QUIT")
      || msg.command_is("NICK");
  if (!logged)
    return;

  if (day_us != current_day_us_) {
    current_day_us_ = day_us;
    OpenLog(day_us);
  }

  LogEvent log_event;
  log_event.set_time_us(time_us.count());
  if (!msg.prefix().empty())
    log_event.set_prefix(msg.prefix());
  log_event.set_command(msg.command());
  for (const auto& arg : msg.args())
    log_event.add_args(arg);

  current_log_->Write(log_event);
};

void Logger::OpenLog(us day_us) {
  if (current_log_)
    current_log_.reset();

  using clock = std::chrono::system_clock;
  std::time_t day = clock::to_time_t(clock::time_point(std::chrono::duration_cast<clock::duration>(day_us)));
  std::tm* day_tm = std::gmtime(&day);

  char buf[16];

  fs::path log_file = dir_;

  std::snprintf(buf, sizeof buf, "%d", 1900 + day_tm->tm_year);
  log_file /= buf;
  std::snprintf(buf, sizeof buf, "%d", day_tm->tm_mon + 1);
  log_file /= buf;

  fs::create_directories(log_file);

  std::snprintf(buf, sizeof buf, "%d.pb", day_tm->tm_mday);
  log_file /= buf;

  current_log_ = std::make_unique<proto::DelimWriter>(log_file.c_str());
}

} // namespace esobot
