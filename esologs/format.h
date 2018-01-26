#ifndef ESOLOGS_FORMAT_H_
#define ESOLOGS_FORMAT_H_

#include <memory>
#include <optional>

#include "esologs/index.h"
#include "esologs/log.pb.h"
#include "web/response.h"

namespace esologs {

void FormatIndex(web::Response* resp, const LogIndex& index, int y);

void FormatError(web::Response* resp, int code, const char* fmt, ...);

struct LogFormatter {
  static std::unique_ptr<LogFormatter> CreateHTML(web::Response* resp);
  static std::unique_ptr<LogFormatter> CreateText(web::Response* resp);
  static std::unique_ptr<LogFormatter> CreateRaw(web::Response* resp);

  virtual void FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) = 0;
  virtual void FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) = 0;
  virtual void FormatStalkerHeader(int year) = 0;
  virtual void FormatStalkerFooter() = 0;
  virtual void FormatDay(bool multiday, int year, int month, int day) = 0;
  virtual void FormatElision() = 0;
  virtual void FormatEvent(const LogEvent& event) = 0;

  virtual ~LogFormatter() = default;
};

} // namespace esologs

#endif // ESOLOGS_FORMAT_H_

// Local Variables:
// mode: c++
// End:
