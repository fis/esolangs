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
void WriteHtmlHeader(web::Writer* web, const char* css, TitleArgs&&... title) {
  web->Write(
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>"
      "<title>", std::forward<TitleArgs>(title)..., "</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"", css, "\">"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "</head>\n"
      "<body>\n");
}

void WriteHtmlFooter(web::Writer* web) {
  web->Write("</body>\n</html>\n");
}

constexpr char kContentTypeText[] = "text/plain; charset=utf-8";
constexpr char kContentTypeHtml[] = "text/html; charset=utf-8";

constexpr char kAboutText[] =
    "<h1 id=\"about\">about</h1>\n"
    "<p>"
    "These logs are for the <code>#esoteric</code> IRC channel on Freenode. "
    "See <a href=\"https://esolangs.org/wiki/Esolang:Community_portal\">"
    "Esolang:Community portal</a> for more information about the channel. "
    "The code for collecting these logs can be found in the "
    //"<a href=\"https://github.com/fis/esowiki\">github.com/fis/esowiki</a> "
    "[COMING REAL SOON] "
    "repository."
    "</p>\n"
    "<p>"
    "Other public collections of logs of the channel can be found at "
    "<a href=\"http://codu.org/logs/_esoteric/\">codu.org</a> and "
    "<a href=\"http://tunes.org/~nef/logs/esoteric/?C=M;O=D\">tunes.org</a>."
    "</p>\n";

} // unnamed namespace

void FormatIndex(web::Response* resp, const LogIndex& index, int y) {
  web::Writer web(resp, kContentTypeHtml);

  bool all = y < 0;
  if (all)
    WriteHtmlHeader(&web, "index.css", "#esoteric logs");
  else
    WriteHtmlHeader(&web, "index.css", YMD(y), " - #esoteric logs");

  auto [y_min, y_max] = index.bounds();

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
    web.Write("<div id=\"b\">\n");

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

  web.Write(kAboutText);
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
  };
  Type type;
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
};

struct LogLineFormatter : public LogFormatter {
  void FormatEvent(const LogEvent& event) override;
  virtual void FormatLine(const LogLine& line) = 0;
};

void LogLineFormatter::FormatEvent(const LogEvent& event) {
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
  };
  static constexpr std::size_t kLineTypeCount = sizeof kLineTypes / sizeof *kLineTypes;

  LogLine line;

  auto time_us = event.time_us();
  line.tstamp.resize(8);
  std::snprintf(
      line.tstamp.data(), 9,
      "%02d:%02d:%02d",
      (int)(time_us / 3600000000),
      (int)(time_us / 60000000 % 60),
      (int)(time_us / 1000000 % 60));

  if (event.direction() == LogEvent::SENT) {
    line.nick = "esowiki";  // TODO: configure?
  } else {
    const std::string& prefix = event.prefix();
    std::size_t sep = prefix.find('!');
    if (sep > 0 && sep != std::string::npos)
      line.nick = prefix.substr(0, sep);
    else
      line.nick = "?unknown?";
  }

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

struct HtmlLineFormatter : public LogLineFormatter {
  web::Writer web_;
  HtmlLineFormatter(web::Response* resp) : web_(resp, kContentTypeHtml) {}
  void FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) override;
  void FormatDay(bool multiday, int year, int month, int day) override;
  void FormatLine(const LogLine& line) override;
  void FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) override;
 private:
  int row_id_ = 0;
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
    web_.Write("<a href=\"", *next, "\">", *next, "→</a>");
  else
    web_.Write("        ", month ? "" : "   ");

  web_.Write(
      "  <a href=\"", YMD(date.year), ".html\">↑", YMD(date.year), "</a>"
      "  <a href=\"all.html\">↑all</a>"
      "</div>\n");
}

void HtmlLineFormatter::FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  WriteHtmlHeader(&web_, "log.css", date, " - #esoteric logs");
  FormatNav(date, prev, next);
}

void HtmlLineFormatter::FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  FormatNav(date, prev, next);
  WriteHtmlFooter(&web_);
}

void HtmlLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    web_.Write(
        "<div class=\"dh\">"
        "<a href=\"", YMD(year, month, day), ".html\">",
        YMD(year, month, day),
        "</a>"
        "</div\n");
}

void RowId(int id, char* p, std::size_t max_len) {
  static constexpr char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static constexpr std::size_t kAlphabetSize = sizeof alphabet - 1;

  if (!max_len) return;

  while (id > 0 && max_len > 1) {
    *p = alphabet[id % kAlphabetSize];
    id /= kAlphabetSize;
    ++p;
    --max_len;
  }
  *p = 0;
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
    if (bold) {
      *out += sep; *out += "fb"; sep = " ";
    }
    if (italic) {
      *out += sep; *out += "fi"; sep = " ";
    }
    if (underline) {
      *out += sep; *out += "fu"; sep = " ";
    }
    if (strikethrough) {
      *out += sep; *out += "fs"; sep = " ";
    }
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
  char id[16];
  RowId(row_id_++, id, sizeof id);

  web_.Write(
      "<div id=\"l", id, "\" class=\"r\">"
      "<span class=\"t\">", line.tstamp, "</span>"
      "<span class=\"s\"> </span>");

  std::string body = HtmlEscape(line.body);

  if (line.type == LogLine::MESSAGE || line.type == LogLine::ACTION) {
    bool act = line.type == LogLine::ACTION;
    web_.Write(
        "<span class=\"ma h", NickHash(line.nick), "\">"
        "<a href=\"#l", id, "\">", act ? "* " : "&lt;", line.nick, act ? "" : "&gt;", "</a>"
        "</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"mb\">", body, "</span>");
  } else if (line.type != LogLine::ERROR) {
    web_.Write(
        "<span class=\"x\"><a href=\"#l", id, "\">-!-</a></span>"
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

struct TextLineFormatter : public LogLineFormatter {
  web::Writer web_;
  TextLineFormatter(web::Response* resp) : web_(resp, kContentTypeText) {}
  void FormatHeader(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
  void FormatDay(bool multiday, int year, int month, int day) override;
  void FormatLine(const LogLine& line) override;
  void FormatFooter(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
};

void TextLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    web_.Write("\n", YMD(year, month, day), ":\n\n");
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

struct RawFormatter : public LogFormatter {
  web::Writer web_;
  unsigned long offset_s_;

  RawFormatter(web::Response* resp) : web_(resp, kContentTypeText), offset_s_(0) {}
  void FormatHeader(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
  void FormatDay(bool, int year, int month, int day) override;
  void FormatEvent(const LogEvent& event) override;
  void FormatFooter(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
};

void RawFormatter::FormatDay(bool, int year, int month, int day) {
  date::sys_days d = date::year{year}/month/day;
  offset_s_ = date::sys_seconds(d).time_since_epoch().count();
}

void RawFormatter::FormatEvent(const LogEvent& event) {
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

std::unique_ptr<LogFormatter> LogFormatter::CreateText(web::Response* resp) {
  return std::make_unique<internal::TextLineFormatter>(resp);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateRaw(web::Response* resp) {
  return std::make_unique<internal::RawFormatter>(resp);
}

} // namespace esologs
