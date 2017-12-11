#ifndef ESOLOGS_FORMAT_H_
#define ESOLOGS_FORMAT_H_

#include "esologs/index.h"
#include "esologs/log.pb.h"

#include "civetweb.h"

namespace esologs {

void FormatIndex(struct mg_connection* conn, const LogIndex& index);

void FormatError(struct mg_connection* conn, int code, const char* fmt, ...);

struct LogFormatter {
  static std::unique_ptr<LogFormatter> CreateHTML(struct mg_connection* conn);
  static std::unique_ptr<LogFormatter> CreateText(struct mg_connection* conn);

  virtual void FormatHeader() = 0;
  virtual void FormatEvent(const LogEvent& event) = 0;
  virtual void FormatFooter() = 0;

  virtual ~LogFormatter() = default;
};

} // namespace esologs

#endif // ESOLOGS_FORMAT_H_

// Local Variables:
// mode: c++
// End:
