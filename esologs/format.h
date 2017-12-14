#ifndef ESOLOGS_FORMAT_H_
#define ESOLOGS_FORMAT_H_

#include <memory>
#include <optional>

#include "esologs/index.h"
#include "esologs/log.pb.h"

#include "civetweb.h"

namespace esologs {

void FormatIndex(struct mg_connection* conn, const LogIndex& index, int y);

void FormatError(struct mg_connection* conn, int code, const char* fmt, ...);

struct LogFormatter {
  static std::unique_ptr<LogFormatter> CreateHTML(struct mg_connection* conn);
  static std::unique_ptr<LogFormatter> CreateText(struct mg_connection* conn);
  static std::unique_ptr<LogFormatter> CreateRaw(struct mg_connection* conn);

  virtual void FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) = 0;
  virtual void FormatDay(bool multiday, int year, int month, int day) = 0;
  virtual void FormatEvent(const LogEvent& event) = 0;
  virtual void FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) = 0;

  virtual ~LogFormatter() = default;
};

} // namespace esologs

#endif // ESOLOGS_FORMAT_H_

// Local Variables:
// mode: c++
// End:
