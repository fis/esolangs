#ifndef ESOLOGS_INDEX_H_
#define ESOLOGS_INDEX_H_

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace esologs {

class LogIndex {
 public:
  LogIndex(const std::string& root) : root_(root) {
    Scan(/* full: */ true);
  }

  void Refresh();

  template <typename F>
  void For(int y, F f) {
    for (auto it = dates_.rbegin(); it != dates_.rend(); ++it) {
      if (it->year > y)
        continue;
      else if (it->year < y)
        break;
      f(it->year, it->month, it->day);
    }
  }

  int default_year() {
    if (dates_.empty())
      return 2002;  // arbitrary
    else
      return dates_.back().year;
  }

  std::pair<int, int> bounds() {
    if (dates_.empty())
      return std::pair(2002, 2002);
    else
      return std::pair(dates_.front().year, dates_.back().year);
  }

 private:
  struct YMD {
    int year;
    int month;
    int day;
    YMD(int y, int m, int d) : year(y), month(m), day(d) {}
  };

  const std::string root_;
  std::vector<YMD> dates_;
  std::chrono::steady_clock::time_point last_scan_;

  void Scan(bool full = false);
};

} // namespace esologs

#endif // ESOLOGS_INDEX_H_

// Local Variables:
// mode: c++
// End:
