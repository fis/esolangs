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
  current_day_ = Now().first;
  OpenLog(current_day_);
}

void Writer::Write(LogEvent* event) {
  auto [day, time_us] = Now();

  if (day != current_day_) {
    current_day_ = day;
    OpenLog(day);
  }

  event->set_time_us(time_us);
  current_log_->Write(*event);
};

void Writer::OpenLog(date::sys_days day) {
  if (current_log_)
    current_log_.reset();

  auto ymd = date::year_month_day{day};

  char buf[16];

  fs::path log_file = dir_;

  std::snprintf(buf, sizeof buf, "%d", (int)ymd.year());
  log_file /= buf;
  std::snprintf(buf, sizeof buf, "%u", (unsigned)ymd.month());
  log_file /= buf;

  fs::create_directories(log_file);

  std::snprintf(buf, sizeof buf, "%u.pb", (unsigned)ymd.day());
  log_file /= buf;

  current_log_ = std::make_unique<proto::DelimWriter>(log_file.c_str());
}

} // namespace esologs
