#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>

#include <date/date.h>

#include "base/log.h"
#include "esologs/format.h"
#include "web/writer.h"

namespace esologs {

void WebWrite(web::Writer* web, const esologs::YMD& ymd) {
  char buf[11];
  if (ymd.month == 0)
    std::snprintf(buf, sizeof buf, "%04d", ymd.year);
  else if (ymd.day == 0)
    std::snprintf(buf, sizeof buf, "%04d-%02d", ymd.year, ymd.month);
  else
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", ymd.year, ymd.month, ymd.day);
  web->Write(buf);
}

namespace {

template <typename... TitleArgs>
void WriteHtmlHeader(web::Writer* web, const char* css, const char* js, TitleArgs&&... title) {
  web->Write(
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>"
      "<title>", std::forward<TitleArgs>(title)..., "</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"", css, "\">");

  if (js)
    web->Write("<script defer=\"true\" type=\"text/javascript\" src=\"", js, "\"></script>");

  web->Write(
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "</head>\n"
      "<body>\n");
}

void WriteHtmlFooter(web::Writer* web) {
  web->Write("</body>\n</html>\n");
}

constexpr char kContentTypeText[] = "text/plain; charset=utf-8";
constexpr char kContentTypeHtml[] = "text/html; charset=utf-8";

constexpr char kCssIndex[] = "../index.css";
constexpr char kCssLog[] = "../log.css";

} // unnamed namespace

void FormatIndex(web::Response* resp, const TargetConfig& cfg, const LogIndex& index, int y) {
  web::Writer web(resp, kContentTypeHtml);

  bool all = y < 0;
  if (all)
    WriteHtmlHeader(&web, kCssIndex, nullptr, cfg.title());
  else
    WriteHtmlHeader(&web, kCssIndex, nullptr, YMD(y), " - ", cfg.title());

  if (!cfg.announce().empty())
    web.Write(cfg.announce());

  auto [y_min, y_max] = index.bounds();
  int y_last = y_max;

  if (!all) {
    web.Write("<h1>");
    if (y > y_min)
      web.Write("<a href=\"", YMD(y-1), ".html\">←", YMD(y-1), "</a>");
    else
      web.Write("<span class=\"d\">←", YMD(y-1), "</span>");
    web.Write("  ", YMD(y), "  ");
    if (y < y_max)
      web.Write("<a href=\"", YMD(y+1), ".html\">", YMD(y+1), "→</a>");
    else
      web.Write("<span class=\"d\">", YMD(y+1), "→</span>");
    web.Write(
        "  <a href=\"all.html\">↑all</a>"
        "  <a href=\"#about\">↓about</a>"
        "</h1>\n");
    y_min = y_max = y;
  }

  for (y = y_max; y >= y_min; --y) {
    if (all)
      web.Write("<h1><a href=\"", YMD(y), ".html\">", YMD(y), "</a></h1>\n");

    if (y == y_last)
      web.Write("<ul class=\"s\"><li class=\"m\"><a href=\"stalker.html#eof\">stalker mode</a></li></ul>\n");

    web.Write("<div class=\"b\">\n");

    int mh = 0;
    index.For(y, [&web, &mh](int y, int m, int d) {
        if (m != mh) {
          YMD ym(y, m);
          web.Write(
              mh ? "</ul>\n</div>\n" : "",
              "<div class=\"m\">"
              "<h2>", ym, "</h2>\n"
              "<ul>\n"
              "<li class=\"m\">"
              "<a href=\"", ym, ".html\">full month</a>"
              " - <a href=\"", ym, ".txt\">text</a>"
              " - <a href=\"", ym, "-raw.txt\">raw</a>");
          mh = m;
        }

        YMD ymd(y, m, d);
        web.Write(
            "<li>"
            "<a href=\"", ymd, ".html\">", ymd, "</a>"
            " - <a href=\"", ymd, ".txt\">text</a>"
            " - <a href=\"", ymd, "-raw.txt\">raw</a>"
            "</li>\n");
      });
    if (mh)
      web.Write("</ul>\n</div>\n");

    web.Write("</div>\n");
  }

  if (!cfg.about().empty())
    web.Write(cfg.about());
  WriteHtmlFooter(&web);
}

void FormatError(web::Response* resp, int code, const char* fmt, ...) {
  char text[512];
  {
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
  }

  web::Writer web(resp, kContentTypeText, code);
  web.Write(text);
}

namespace internal {

struct LogLine {
  enum Type {
    MESSAGE,
    ACTION,
    JOIN,
    PART,
    QUIT,
    NICK,
    KICK,
    MODE,
    TOPIC,
    ERROR,
    IGNORED,
  };
  Type type;
  std::int64_t day;
  std::uint64_t line;
  std::string tstamp;
  std::string nick;
  std::string body;
};

constexpr const char* kLineDescriptions[] = {
  /* MESSAGE: */ "?",
  /* ACTION: */ "?",
  /* JOIN: */ "joined",
  /* PART: */ "left",
  /* QUIT: */ "quit",
  /* NICK: */ "changed nick to",
  /* KICK: */ "kicked",
  /* MODE: */ "set channel mode",
  /* TOPIC: */ "set topic",
  /* ERROR: */ "?",
  /* IGNORED: */ "",
};

class LogLineFormatter : public LogFormatter {
 public:
  void FormatEvent(const LogEvent& event, const TargetConfig& cfg) override;
  virtual void FormatLine(const LogLine& line) = 0;
 private:
  std::uint64_t line_counter_ = 0;
};

void LogLineFormatter::FormatEvent(const LogEvent& event, const TargetConfig& cfg) {
  static constexpr struct {
    const char* cmd;
    LogLine::Type type;
  } kLineTypes[] = {
    {"PRIVMSG", LogLine::MESSAGE},
    {"NOTICE", LogLine::MESSAGE},
    {"JOIN", LogLine::JOIN},
    {"PART", LogLine::PART},
    {"QUIT", LogLine::QUIT},
    {"NICK", LogLine::NICK},
    {"KICK", LogLine::KICK},
    {"MODE", LogLine::MODE},
    {"TOPIC", LogLine::TOPIC},
    {"NAMES", LogLine::IGNORED},
  };
  static constexpr std::size_t kLineTypeCount = sizeof kLineTypes / sizeof *kLineTypes;

  LogLine line;

  const std::string& cmd = event.command();
  std::size_t line_type = 0;
  for (; line_type < kLineTypeCount; ++line_type) {
    if (cmd == kLineTypes[line_type].cmd) {
      line.type = kLineTypes[line_type].type;
      break;
    }
  }
  if (line_type == kLineTypeCount)
    line.type = LogLine::ERROR;
  else if (line.type == LogLine::IGNORED)
    return;

  auto time_us = event.time_us() % 86400000000u;

  line.tstamp.resize(8);
  std::snprintf(
      line.tstamp.data(), 9,
      "%02d:%02d:%02d",
      (int)(time_us / 3600000000u),
      (int)(time_us / 60000000u % 60u),
      (int)(time_us / 1000000u % 60u));

  if (event.has_event_id()) {
    line.day = event.event_id().day();
    line.line = event.event_id().line();
  } else {
    line.day = 0;
    line.line = line_counter_++;
  }

  if (event.direction() == LogEvent::SENT) {
    line.nick = cfg.nick();
  } else {
    const std::string& prefix = event.prefix();
    std::size_t sep = prefix.find('!');
    if (sep > 0 && sep != std::string::npos)
      line.nick = prefix.substr(0, sep);
    else
      line.nick = "?unknown?";
  }

  int body_arg = (line.type == LogLine::QUIT || line.type == LogLine::NICK) ? 0 : 1;
  for (int i = body_arg; i < event.args_size(); ++i) {
    if (i > body_arg)
      line.body += ' ';
    for (const auto& c : event.args(i)) {
      unsigned char v = c;
      if (v < 1 || (v > 3 && v < 15) || (v > 15 && v < 29))
        continue;
      line.body += c;
    }
  }

  if (line.type == LogLine::MESSAGE) {
    std::string_view body = line.body;
    if (body.size() >= 9 && body.substr(0, 8) == "\x01""ACTION " && body[body.size()-1] == '\x01') {
      line.type = LogLine::ACTION;
      line.body = line.body.substr(8, line.body.size()-9);
    }
  }

  FormatLine(line);
}

class HtmlLineFormatter : public LogLineFormatter {
 public:
  HtmlLineFormatter(web::Response* resp) : web_(resp, kContentTypeHtml) {}
  HtmlLineFormatter(std::string* buffer) : web_(buffer) {}
  void FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next, const std::string& title) override;
  void FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) override;
  void FormatStalkerHeader(int year, const std::string& title) override;
  void FormatStalkerFooter() override;
  void FormatDay(bool multiday, int year, int month, int day) override;
  void FormatElision() override;
  void FormatLine(const LogLine& line) override;
 private:
  web::Writer web_;
  std::int64_t last_day_ = 0;
  std::uint64_t last_line_ = 0;
  void FormatNav(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next);
};

void HtmlLineFormatter::FormatNav(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  bool month = date.day == 0;

  web_.Write("<div class=\"n\">");

  if (prev)
    web_.Write("<a href=\"", *prev, ".html\">←", *prev, "</a>");
  else
    web_.Write("        ", month ? "" : "   ");

  web_.Write("  <span class=\"nc\">", date, "</span>  ");

  if (next)
    web_.Write("<a href=\"", *next, ".html\">", *next, "→</a>");
  else
    web_.Write("        ", month ? "" : "   ");

  web_.Write(
      "  <a href=\"", YMD(date.year), ".html\">↑", YMD(date.year), "</a>"
      "  <a href=\"all.html\">↑all</a>"
      "</div>\n");
}

void HtmlLineFormatter::FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next, const std::string& title) {
  WriteHtmlHeader(&web_, kCssLog, nullptr, date, " - ", title);
  FormatNav(date, prev, next);
}

void HtmlLineFormatter::FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  FormatNav(date, prev, next);
  WriteHtmlFooter(&web_);
}

void HtmlLineFormatter::FormatStalkerHeader(int year, const std::string& title) {
  WriteHtmlHeader(&web_, kCssLog, "stalker.js", title, " - stalker mode");
  web_.Write(
      "<div class=\"n\">"
      "<span class=\"nc\">stalker mode</span>"
      "  <a href=\"", year, ".html\">↑", year, "</a>"
      "  <a href=\"all.html\">↑all</a>"
      "</div>\n");
}

void HtmlLineFormatter::FormatStalkerFooter() {
  web_.Write(
      "<div id=\"s\" data-stalker-day=\"", last_day_, "\" data-stalker-line=\"", last_line_, "\"></div>\n"
      "<div id=\"eof\" class=\"n\">"
      "<span id=\"smsg\">"
      "To update automatically, stalker mode requires a reasonably modern browser with scripts enabled. "
      "If this message does not disappear, it's either because of that or a bug. Feel free to get in "
      "touch on channel for debugging. Or just work around the issue by manually reloading."
      "</span>"
      "</div>\n");
  WriteHtmlFooter(&web_);
}

void HtmlLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    web_.Write(
        "<div class=\"dh\">"
        "<a href=\"", YMD(year, month, day), ".html\">",
        YMD(year, month, day),
        "</a>"
        "</div>\n");
}

void HtmlLineFormatter::FormatElision() {
  web_.Write(
      "<div class=\"r\">"
      "<span class=\"t\">        </span>"
      "<span class=\"s\"> </span>"
      "<span class=\"x\">   </span>"
      "<span class=\"s\"> </span>"
      "<span class=\"ed\">[...]</span>"
      "</div>\n");
}

void RowId(std::uint64_t id, std::string* str) {
  static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static constexpr std::size_t kAlphabetSize = sizeof alphabet - 1;

  while (id > 0) {
    *str += alphabet[id % kAlphabetSize];
    id /= kAlphabetSize;
  }
}

unsigned NickHash(const std::string& nick) {
  unsigned nick_hash = 0;
  for (const auto& c : nick)
    nick_hash = 31 * nick_hash + (unsigned char)c;
  return nick_hash % 10;
}

struct IrcFormat {
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool strikethrough = false;
  int fg = -1, bg = -1;

  void AppendClass(std::string* out) {
    const char* sep = "";
    if (bold) { *out += sep; *out += "fb"; sep = " "; }
    if (italic) { *out += sep; *out += "fi"; sep = " "; }
    if (underline) { *out += sep; *out += "fu"; sep = " "; }
    if (strikethrough) { *out += sep; *out += "fs"; sep = " "; }
    if (fg != -1) {
      *out += sep; *out += "fc"; *out += std::to_string(fg); sep = " ";
    }
    if (bg != -1) {
      *out += sep; *out += "fg"; *out += std::to_string(bg); sep = " ";
    }
  }
  std::size_t ParseColor(const std::string& raw, std::size_t i) {
    std::size_t len = raw.size();

    if (i+1 >= len || (raw[i+1] < '0' || raw[i+1] > '9')) {
      fg = bg = -1;
      return i;
    }

    ++i;
    fg = raw[i] - '0';
    if (i+1 < len && (raw[i+1] >= '0' && raw[i+1] <= '9')) {
      ++i;
      fg = 10*fg + (raw[i] - '0');
    }

    if (i+2 >= len || raw[i+1] != ',' || (raw[i+2] < '0' || raw[i+2] > '9'))
      return i;

    i += 2;
    bg = raw[i] - '0';
    if (i+1 < len && (raw[i+1] >= '0' && raw[i+1] <= '9')) {
      ++i;
      bg = 10*bg + (raw[i] - '0');
    }

    return i;
  }
  void Reset() {
    bold = italic = underline = strikethrough = false;
    fg = bg = -1;
  }
  bool is_default() {
    return !bold && !italic && !underline && !strikethrough && fg == -1 && bg == -1;
  }
};

std::string HtmlEscape(const std::string& raw) {
  std::string cooked;

  IrcFormat format;

  for (std::size_t i = 0; i < raw.size(); ++i) {
    char c = raw[i];
    if (c == 1) {
      cooked += "&lt;CTCP&gt;";
    } else if ((unsigned char)c < 32) {
      if (!format.is_default())
        cooked += "</span>";

      if (c == 2)
        format.bold = !format.bold;
      else if (c == 3)
        i = format.ParseColor(raw, i);
      else if (c == 15)
        format.Reset();
      else if (c == 29)
        format.italic = !format.italic;
      else if (c == 30)
        format.strikethrough = !format.strikethrough;
      else if (c == 31)
        format.underline = !format.underline;

      if (!format.is_default()) {
        cooked += "<span class=\"";
        format.AppendClass(&cooked);
        cooked += "\">";
      }
    } else if (c == '<') {
      cooked += "&lt;";
    } else if (c == '>') {
      cooked += "&gt;";
    } else if (c == '&') {
      cooked += "&amp;";
    } else {
      cooked += c;
    }
  }
  if (!format.is_default())
    cooked += "</span>";

  return cooked;
}

void HtmlLineFormatter::FormatLine(const LogLine& line) {
  std::string id;
  std::string link;

  if (line.day) {
    YMD ymd{date::sys_days{date::days{line.day}}};
    id = "";
    // TODO: wrap this sort of thing
    link.resize(10);
    std::snprintf(link.data(), 11, "%04u-%02u-%02u", (unsigned) ymd.year % 10000, (unsigned) ymd.month % 100, (unsigned) ymd.day % 100);
    link += ".html#l"; RowId(line.line, &link);

    last_day_ = line.day;
    last_line_ = line.line;
  } else {
    id = " id=\"l"; RowId(line.line, &id); id += '"';
    link = "#l"; RowId(line.line, &link);
  }

  web_.Write(
      "<div", id, " class=\"r\">"
      "<span class=\"t\">", line.tstamp, "</span>"
      "<span class=\"s\"> </span>");

  std::string body = HtmlEscape(line.body);

  if (line.type == LogLine::MESSAGE || line.type == LogLine::ACTION) {
    bool act = line.type == LogLine::ACTION;
    web_.Write(
        "<span class=\"ma h", NickHash(line.nick), "\">"
        "<a href=\"", link, "\">", act ? "* " : "&lt;", line.nick, act ? "" : "&gt;", "</a>"
        "</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"mb\">", body, "</span>");
  } else if (line.type != LogLine::ERROR) {
    web_.Write(
        "<span class=\"x\"><a href=\"", link, "\">-!-</a></span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\">"
        "<span class=\"ea h", NickHash(line.nick), "\">", line.nick, "</span>"
        " has ", kLineDescriptions[line.type]);
    if (!body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        web_.Write(" (<span class=\"eb\">", body, "</span>)");
      else if (line.type == LogLine::NICK || line.type == LogLine::KICK)
        web_.Write(" <span class=\"ea h", NickHash(body), "\">", body, "</span>");
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        web_.Write(": <span class=\"eb\">", body, "</span>");
    }
    web_.Write(".</span>");
  } else {
    web_.Write("<span class=\"e\">unexpected log event :(</span>");
  }

  web_.Write("</div>\n");
}

class TextLineFormatter : public LogLineFormatter {
 public:
  TextLineFormatter(web::Response* resp) : web_(resp, kContentTypeText) {}
  void FormatHeader(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&, const std::string&) override {}
  void FormatFooter(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
  void FormatStalkerHeader(int, const std::string&) override {}
  void FormatStalkerFooter() override {}
  void FormatDay(bool multiday, int year, int month, int day) override;
  void FormatElision() override;
  void FormatLine(const LogLine& line) override;
 private:
  web::Writer web_;
};

void TextLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    web_.Write("\n", YMD(year, month, day), ":\n\n");
}

void TextLineFormatter::FormatElision() {
  web_.Write("[...]\n");
}

std::string UnFormat(const std::string& raw) {
  std::string cooked;

  IrcFormat format;

  for (std::size_t i = 0; i < raw.size(); ++i) {
    char c = raw[i];
    if ((unsigned char)c < 32) {
      if (c == 1)
        cooked += "<CTCP>";
      else if (c == 3)
        i = format.ParseColor(raw, i);
    } else {
      cooked += c;
    }
  }

  return cooked;
}

void TextLineFormatter::FormatLine(const LogLine& line) {
  web_.Write(line.tstamp, " ");

  std::string body = UnFormat(line.body);
  if (line.type == LogLine::MESSAGE) {
    web_.Write("<", line.nick, "> ", body, "\n");
  } else if (line.type == LogLine::ACTION) {
    web_.Write("* ", line.nick, " ", body, "\n");
  } else if (line.type != LogLine::ERROR) {
    web_.Write("-!- ", line.nick, " has ", kLineDescriptions[line.type]);
    if (!line.body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        web_.Write(" (", body, ")");
      else if (line.type == LogLine::NICK || line.type == LogLine::KICK)
        web_.Write(" ", body);
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        web_.Write(": ", body);
    }
    web_.Write(".\n");
  } else {
    web_.Write("[unexpected log event :(]\n");
  }
}

class RawFormatter : public LogFormatter {
 public:
  RawFormatter(web::Response* resp) : web_(resp, kContentTypeText), offset_s_(0) {}
  void FormatHeader(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&, const std::string&) override {}
  void FormatFooter(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
  void FormatStalkerHeader(int, const std::string&) override {}
  void FormatStalkerFooter() override {}
  void FormatDay(bool, int year, int month, int day) override;
  void FormatElision() override {}
  void FormatEvent(const LogEvent& event, const TargetConfig&) override;
 private:
  web::Writer web_;
  unsigned long offset_s_;
};

void RawFormatter::FormatDay(bool, int year, int month, int day) {
  date::sys_days d = date::year{year}/month/day;
  offset_s_ = date::sys_seconds(d).time_since_epoch().count();
}

void RawFormatter::FormatEvent(const LogEvent& event, const TargetConfig&) {
  auto time_us = event.time_us();
  web_.Write(
      event.direction() == LogEvent::SENT ? ">" : "<",
      " ",
      offset_s_ + (unsigned long)(time_us / 1000000u),
      " ",
      (unsigned long)(time_us % 1000000u),
      " ");
  if (!event.prefix().empty())
    web_.Write(":", event.prefix(), " ");
  web_.Write(event.command());
  for (int i = 0, n = event.args_size(); i < n; ++i)
    web_.Write(i == n - 1 ? " :" : " ", event.args(i));
  web_.Write("\n");
}

} // namespace internal

std::unique_ptr<LogFormatter> LogFormatter::CreateHTML(web::Response* resp) {
  return std::make_unique<internal::HtmlLineFormatter>(resp);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateHTML(std::string* buffer) {
  return std::make_unique<internal::HtmlLineFormatter>(buffer);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateText(web::Response* resp) {
  return std::make_unique<internal::TextLineFormatter>(resp);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateRaw(web::Response* resp) {
  return std::make_unique<internal::RawFormatter>(resp);
}

} // namespace esologs
