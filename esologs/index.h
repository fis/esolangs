#ifndef ESOLOGS_INDEX_H_
#define ESOLOGS_INDEX_H_

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <date/date.h>

#include "proto/delim.h"

namespace esologs {

struct YMD {
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

  void Scan(bool full = false);
};

} // namespace esologs

#endif // ESOLOGS_INDEX_H_

// Local Variables:
// mode: c++
// End:
