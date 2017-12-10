#include <algorithm>
#include <experimental/filesystem>

#include "re2/re2.h"

#include "esologs/index.h"

namespace esologs {

namespace fs = std::experimental::filesystem;

void LogIndex::Scan(bool full) {
  if (full)
    dates_.clear();

  YMD lower_bound(0, 0, 0);
  if (!dates_.empty())
    lower_bound = dates_.back();

  RE2 re_ym("\\d+");
  RE2 re_d("(\\d+)\\.pb");

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

} // namespace esologs
