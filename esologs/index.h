#ifndef ESOLOGS_INDEX_H_
#define ESOLOGS_INDEX_H_

#include <string>
#include <vector>

namespace esologs {

class LogIndex {
 public:
  LogIndex(const std::string& root) : root_(root) {
    Scan(/* full: */ true);
  }

  template <typename F>
  void For(F f) const {
    for (const auto& date : dates_)
      f(date.year, date.month, date.day);
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

  void Scan(bool full = false);
};

} // namespace esologs

#endif // ESOLOGS_INDEX_H_

// Local Variables:
// mode: c++
// End:
