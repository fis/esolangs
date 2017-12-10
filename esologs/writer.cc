#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <experimental/filesystem>
#include <string>

#include "esologs/log.pb.h"
#include "esologs/writer.h"

namespace esologs {

namespace fs = std::experimental::filesystem;

Writer::Writer(const std::string& dir) : dir_(dir) {
  current_day_us_ = Now().first;
  OpenLog(current_day_us_);
}

void Writer::Write(LogEvent* event) {
  auto [day_us, time_us] = Now();

  if (day_us != current_day_us_) {
    current_day_us_ = day_us;
    OpenLog(day_us);
  }

  event->set_time_us(time_us.count());
  current_log_->Write(*event);
};

void Writer::OpenLog(us day_us) {
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

} // namespace esologs
