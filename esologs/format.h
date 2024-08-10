#ifndef ESOLOGS_FORMAT_H_
#define ESOLOGS_FORMAT_H_

#include <memory>
#include <optional>
#include <string>

#include "esologs/config.pb.h"
#include "esologs/index.h"
#include "esologs/log.pb.h"
#include "web/request.h"
#include "web/response.h"

namespace esologs {

void FormatIndex(const web::Request& req, web::Response* resp, const TargetConfig& cfg, const LogIndex& index, int y);

void FormatError(web::Response* resp, int code, const char* fmt, ...);
void FormatErrorWithHeaders(web::Response* resp, int code, std::string_view extra_headers, const char* fmt, ...);

struct LogFormatter {
  static std::unique_ptr<LogFormatter> CreateHTML(web::Response* resp, std::string_view extra_headers = std::string_view());
  static std::unique_ptr<LogFormatter> CreateHTML(std::string* buffer);
  static std::unique_ptr<LogFormatter> CreateText(web::Response* resp, std::string_view extra_headers = std::string_view());
  static std::unique_ptr<LogFormatter> CreateRaw(web::Response* resp, std::string_view extra_headers = std::string_view());

  virtual void FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next, const std::string& title) = 0;
  virtual void FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) = 0;
  virtual void FormatStalkerHeader(int year, const std::string& title) = 0;
  virtual void FormatStalkerFooter() = 0;
  virtual void FormatDay(bool multiday, int year, int month, int day) = 0;
  virtual void FormatElision() = 0;
  virtual void FormatEvent(const LogEvent& event, const TargetConfig& cfg) = 0;

  virtual ~LogFormatter() = default;
};

} // namespace esologs

#endif // ESOLOGS_FORMAT_H_

// Local Variables:
// mode: c++
// End:
