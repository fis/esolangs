#ifndef ESOLOGS_FORMAT_H_
#define ESOLOGS_FORMAT_H_

#include "esologs/index.h"
#include "esologs/log.pb.h"

#include "civetweb.h"

namespace esologs {

void FormatIndex(struct mg_connection* conn, const LogIndex& index);

void FormatError(struct mg_connection* conn, int code, const char* fmt, ...);

class LogFormatter {
 public:
  LogFormatter(struct mg_connection* conn);
  ~LogFormatter();

  void FormatEvent(const LogEvent& event);

 private:
  struct mg_connection* conn_;
};

} // namespace esologs

#endif // ESOLOGS_FORMAT_H_

// Local Variables:
// mode: c++
// End:
