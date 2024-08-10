#include <algorithm>
#include <filesystem>

#include "re2/re2.h"

#include "base/log.h"
#include "esologs/index.h"
#include "proto/brotli.h"

namespace esologs {

namespace fs = std::filesystem;

namespace {

constexpr std::chrono::steady_clock::duration kRescanInterval = std::chrono::seconds(30);

} // unnamed namespace

void LogIndex::Refresh() {
  if (std::chrono::steady_clock::now() - last_scan_ >= kRescanInterval)
    Scan();
}

void LogIndex::Scan(bool full) {
  last_scan_ = std::chrono::steady_clock::now();

  if (full)
    dates_.clear();

  YMD lower_bound(0, 0, 0);
  if (!dates_.empty())
    lower_bound = dates_.back();

  RE2 re_ym("\\d+");
  RE2 re_d("(\\d+)\\.pb(?:\\.br)?");

  fs::path root = root_;

  std::vector<int> years;
  for (const auto& entry : fs::directory_iterator(root_)) {
    std::string file = entry.path().filename();
    if (RE2::FullMatch(file, re_ym)) {
      int year = std::stoi(file);
      if (year >= lower_bound.year)
        years.push_back(year);
    }
  }
  if (years.empty())
    return;
  std::sort(years.begin(), years.end());

  for (int year : years) {
    fs::path y_dir = root / std::to_string(year);

    std::vector<int> months;
    for (const auto& entry : fs::directory_iterator(y_dir)) {
      std::string file = entry.path().filename();
      if (RE2::FullMatch(file, re_ym)) {
        int month = std::stoi(file);
        if (year > lower_bound.year || month >= lower_bound.month)
          months.push_back(month);
      }
    }
    if (months.empty())
      continue;
    std::sort(months.begin(), months.end());

    for (int month : months) {
      fs::path m_dir = y_dir / std::to_string(month);

      std::vector<int> days;
      for (const auto& entry : fs::directory_iterator(m_dir)) {
        std::string file = entry.path().filename();
        std::string file_day;
        if (RE2::FullMatch(file, re_d, &file_day)) {
          int day = std::stoi(file);
          if (year > lower_bound.year || month > lower_bound.month || day > lower_bound.day)
            days.push_back(day);
        }
      }
      if (days.empty())
        continue;
      std::sort(days.begin(), days.end());

      for (int day : days)
        dates_.emplace_back(year, month, day);
    }
  }
}

bool LogIndex::Lookup(const YMD& date, std::optional<YMD>* prev, std::optional<YMD>* next) const noexcept {
  bool monthly = date.day == 0;

  auto pos = std::lower_bound(dates_.begin(), dates_.end(), date);
  if (pos == dates_.end()
      || pos->year != date.year
      || pos->month != date.month
      || (!monthly && pos->day != date.day))
    return false;

  if (prev) {
    if (pos != dates_.begin()) {
      *prev = std::optional(*(pos-1));
      if (monthly)
        (*prev)->day = 0;
    } else {
      *prev = std::optional<YMD>();
    }
  }

  if (next) {
    ++pos;
    if (monthly)
      while (pos != dates_.end() && pos->year == date.year && pos->month == date.month)
        ++pos;

    if (pos != dates_.end()) {
      *next = std::optional(*pos);
      if (monthly)
        (*next)->day = 0;
    } else {
      *next = std::optional<YMD>();
    }
  }

  return true;
}

bool LogIndex::Stat(const YMD& date, FileInfo* info) {
  // Intended behavior:
  // - If the requested range (day or month) is fully in the past, return a "frozen" record.
  // - If requesting a month (that's still current):
  //   - If there's an active file being written, return the single-day data of that file.
  //   - Otherwise, return a "just-past-the-end" record of the last complete day (an imaginary 0-sized file of the next day).
  // - Otherwise, return the actual last modification date, day and size of the requested single file.

  if (dates_.empty())
    return false;

  bool monthly = date.day == 0;
  auto now = std::chrono::system_clock::now();

  auto last_date = monthly ? date.last_of_month() : date;
  auto frozen_time = last_date.time() + std::chrono::days{1} + std::chrono::minutes{5};
  if (frozen_time <= now) {
    *info = FileInfo::of_frozen(frozen_time);
    return true;
  }

  YMD logdate = date;
  fs::path logfile{};

  if (monthly) {
    for (int d = 31; d >= 1; --d) {
      auto end_time = YMD{date.year, date.month, d}.time() + std::chrono::days{1};
      if (end_time <= now) {
        *info = FileInfo::of_liquid(end_time, d+1, 0);
        return true;
      }
      logfile = file(date.year, date.month, d);
      if (fs::is_regular_file(logfile)) {
        logdate.day = d;
        break;
      }
    }
    if (!logdate.day)
      return false; // should be impossible
  } else {
    logfile = file(logdate.year, logdate.month, logdate.day);
  }

  std::error_code ec;
  auto last_write = fs::last_write_time(logfile, ec);
  if (ec)
    return false;
  std::size_t size = fs::file_size(logfile, ec);
  if (ec)
    return false;
  // N.B. this would be more portable with `std::chrono::clock_cast<std::chrono::system_clock>`,
  // but GCC 12 on Debian stable does not support it as of this writing.
  *info = FileInfo::of_liquid(std::chrono::file_clock::to_sys(last_write), logdate.day, size);
  return true;
}

std::unique_ptr<proto::DelimReader> LogIndex::Open(int y, int m, int d) {
  fs::path logfile = file(y, m, d);
  if (fs::is_regular_file(logfile)) {
    return std::make_unique<proto::DelimReader>(logfile.c_str());
  } else {
    logfile += ".br";
    if (fs::is_regular_file(logfile))
      return std::make_unique<proto::DelimReader>(base::own(proto::BrotliInputStream::FromFile(logfile.c_str())));
  }

  return nullptr;
}

fs::path LogIndex::file(int y, int m, int d) const noexcept {
  fs::path logfile = root_;
  logfile /= std::to_string(y);
  logfile /= std::to_string(m);
  logfile /= std::to_string(d);
  logfile += ".pb";
  return logfile;
}

} // namespace esologs
