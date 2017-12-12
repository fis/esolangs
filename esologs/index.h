#ifndef ESOLOGS_INDEX_H_
#define ESOLOGS_INDEX_H_

#include <chrono>
#include <string>
#include <vector>

namespace esologs {

class LogIndex {
 public:
  LogIndex(const std::string& root) : root_(root) {
    Scan(/* full: */ true);
  }

  template <typename F>
  void For(F f) {
    Refresh();
    for (auto it = dates_.rbegin(); it != dates_.rend(); ++it)
      f(it->year, it->month, it->day);
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

  void Refresh();
  void Scan(bool full = false);
};

} // namespace esologs

#endif // ESOLOGS_INDEX_H_

// Local Variables:
// mode: c++
// End:
