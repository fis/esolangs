#ifndef ESOLOGS_INDEX_H_
#define ESOLOGS_INDEX_H_

#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <date/date.h>

#include "proto/delim.h"

namespace esologs {

struct YMD {
  struct day_number_tag { explicit day_number_tag() = default; };
  static inline constexpr day_number_tag day_number = day_number_tag();

  int year;
  int month;
  int day;

  explicit YMD(int y, int m = 0, int d = 0) : year(y), month(m), day(d) {}
  explicit YMD(date::sys_days d) {
    auto date = date::year_month_day{d};
    year = (int) date.year();
    month = (unsigned) date.month();
    day = (unsigned) date.day();
  }
  YMD(day_number_tag, std::int64_t day) : YMD(date::sys_days{date::days{day}}) {}

  YMD last_of_month() const noexcept {
    auto d = date::year{year}/date::month{static_cast<unsigned>(month)}/date::last;
    return YMD{year, month, static_cast<int>(static_cast<unsigned>(d.day()))};
  }

  std::chrono::sys_seconds time() const noexcept {
    auto d = date::year{year}/date::month{static_cast<unsigned>(month)}/date::day{static_cast<unsigned>(day)};
    return static_cast<date::sys_days>(d);
  }

  friend bool operator==(const YMD& a, const YMD& b);
  friend bool operator!=(const YMD& a, const YMD& b);
  friend bool operator< (const YMD& a, const YMD& b);
  friend bool operator<=(const YMD& a, const YMD& b);
  friend bool operator>=(const YMD& a, const YMD& b);
  friend bool operator> (const YMD& a, const YMD& b);
};

inline bool operator==(const YMD& a, const YMD& b) { return a.year == b.year && a.month == b.month && a.day == b.day; }
inline bool operator!=(const YMD& a, const YMD& b) { return !(a == b); }

inline bool operator<(const YMD& a, const YMD& b) {
  return a.year < b.year || (a.year == b.year && (a.month < b.month || (a.month == b.month && a.day < b.day)));
}
inline bool operator<=(const YMD& a, const YMD& b) {
  return a.year < b.year || (a.year == b.year && (a.month < b.month || (a.month == b.month && a.day <= b.day)));
}
inline bool operator>=(const YMD& a, const YMD& b) { return b <= a; }
inline bool operator>(const YMD& a, const YMD& b) { return b < a; }

struct FileInfo {
  using time_type = std::chrono::time_point<std::chrono::system_clock>;

  bool frozen;          // `true` if file is guaranteed to no longer change
  time_type last_write; // synthesized past-the-end-of-day timestamp for frozen files
  int size_day;         // always set to 0 if frozen; otherwise the day number that size refers to
  std::size_t size;     // always set to 0 if frozen; otherwise size of the most recent referred-to file

  explicit constexpr FileInfo() : frozen(false), last_write(), size(0) {}
  explicit constexpr FileInfo(bool frozen, time_type last_write, int size_day, std::size_t size) : frozen(frozen), last_write(last_write), size_day(size_day), size(size) {}
  static inline constexpr FileInfo of_frozen(time_type last_write) { return FileInfo(true, last_write, 0, 0); }
  static inline constexpr FileInfo of_liquid(time_type last_write, int size_day, std::size_t size) { return FileInfo(false, last_write, size_day, size); }
};

class LogIndex {
 public:

  LogIndex(const std::string& root) : root_(root) {
    Scan(/* full: */ true);
  }

  void Refresh();

  template <typename F>
  void For(int y, F f) const {
    for (auto it = dates_.rbegin(); it != dates_.rend(); ++it) {
      if (it->year > y)
        continue;
      else if (it->year < y)
        break;
      f(it->year, it->month, it->day);
    }
  }

  bool Lookup(const YMD& date, std::optional<YMD>* prev = nullptr, std::optional<YMD>* next = nullptr) const noexcept;

  bool Stat(const YMD& date, FileInfo* info);

  std::unique_ptr<proto::DelimReader> Open(int y, int m, int d);

  std::mutex* lock() { return &lock_; }

  int default_year() const noexcept {
    if (dates_.empty())
      return 2002;  // arbitrary
    else
      return dates_.back().year;
  }

  std::pair<int, int> bounds() const noexcept {
    if (dates_.empty())
      return std::pair(2002, 2002);
    else
      return std::pair(dates_.front().year, dates_.back().year);
  }

 private:
  const std::string root_;
  std::vector<YMD> dates_;
  std::chrono::steady_clock::time_point last_scan_;

  std::mutex lock_;

  std::filesystem::path file(int y, int m, int d) const noexcept;
  void Scan(bool full = false);
};

} // namespace esologs

#endif // ESOLOGS_INDEX_H_

// Local Variables:
// mode: c++
// End:
