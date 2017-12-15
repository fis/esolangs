#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>

#include <date/date.h>

#include "esologs/format.h"

namespace esologs {

namespace {

void HeaderText(struct mg_connection* conn) {
  mg_printf(
      conn,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n");
}

void HeaderHtml(struct mg_connection* conn, const char* title, const char* style) {
  mg_printf(
      conn,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>"
      "<title>%s</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "</head>\n"
      "<body>\n",
      title, style);
}

void FooterHtml(struct mg_connection* conn) {
  mg_printf(conn, "</body>\n</html>\n");
}

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

void FormatIndex(struct mg_connection* conn, const LogIndex& index, int y) {
  bool all = y < 0;

  char title[64];
  if (all)
    std::strcpy(title, "#esoteric logs");
  else
    std::snprintf(title, sizeof title, "%04d - #esoteric logs", y);
  HeaderHtml(conn, title, "index.css");

  auto [y_min, y_max] = index.bounds();

  if (!all) {
    mg_printf(conn, "<h1>");
    if (y > y_min)
      mg_printf(conn, "<a href=\"%04d.html\">←%04d</a>", y-1, y-1);
    else
      mg_printf(conn, "<span class=\"d\">←%04d</span>", y-1);
    mg_printf(conn, "  %04d  ", y);
    if (y < y_max)
      mg_printf(conn, "<a href=\"%04d.html\">%04d→</a>", y+1, y+1);
    else
      mg_printf(conn, "<span class=\"d\">%04d→</span>", y+1);
    mg_printf(
        conn,
        "  <a href=\"all.html\">↑all</a>"
        "  <a href=\"#about\">↓about</a>"
        "</h1>\n");
    y_min = y_max = y;
  }

  for (y = y_max; y >= y_min; --y) {
    if (all)
      mg_printf(conn, "<h1><a href=\"%04d.html\">%04d</a></h1>", y, y);
    mg_printf(conn, "<div id=\"b\">\n");

    int mh = 0;
    index.For(y, [&conn, &mh](int y, int m, int d) {
        if (m != mh) {
          mg_printf(
              conn,
              "%s"
              "<div class=\"m\">"
              "<h2>%04d-%02d</h2>\n"
              "<ul>\n"
              "<li class=\"m\">"
              "<a href=\"%04d-%02d.html\">full month</a>"
              " - <a href=\"%04d-%02d.txt\">text</a>"
              " - <a href=\"%04d-%02d-raw.txt\">raw</a>",
              mh ? "</ul>\n</div>\n" : "",
              y, m, y, m, y, m, y, m);
          mh = m;
        }

        char ymd[16];
        std::snprintf(ymd, sizeof ymd, "%04d-%02d-%02d", y, m, d);
        mg_printf(
            conn,
            "<li>"
            "<a href=\"%s.html\">%s</a>"
            " - <a href=\"%s.txt\">text</a>"
            " - <a href=\"%s-raw.txt\">raw</a>"
            "</li>\n",
            ymd, ymd, ymd, ymd);
      });
    if (mh)
      mg_printf(conn, "</ul>\n</div>\n");

    mg_printf(conn, "</div>\n");
  }

  mg_printf(conn, "%s", kAboutText);
  FooterHtml(conn);
}

void FormatError(struct mg_connection* conn, int code, const char* fmt, ...) {
  char text[512];
  {
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
  }

  mg_printf(
      conn,
      "HTTP/1.1 %d Error\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "%s\n",
      code, text);
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
  struct mg_connection* conn_;

  LogLineFormatter(struct mg_connection* conn) : conn_(conn) {}

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
  using LogLineFormatter::LogLineFormatter;
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

  mg_printf(conn_, "<div class=\"n\">");

  if (prev && month)
    mg_printf(
        conn_,
        "<a href=\"%04d-%02d.html\">←%04d-%02d</a>",
        prev->year, prev->month,
        prev->year, prev->month);
  else if (prev && !month)
    mg_printf(
        conn_,
        "<a href=\"%04d-%02d-%02d.html\">←%04d-%02d-%02d</a>",
        prev->year, prev->month, prev->day,
        prev->year, prev->month, prev->day);
  else
    mg_printf(conn_, "        %s", month ? "" : "   ");

  if (month)
    mg_printf(conn_, "  <span class=\"nc\">%04d-%02d</span>  ", date.year, date.month);
  else
    mg_printf(conn_, "  <span class=\"nc\">%04d-%02d-%02d</span>  ", date.year, date.month, date.day);

  if (next && month)
    mg_printf(
        conn_,
        "<a href=\"%04d-%02d.html\">%04d-%02d→</a>",
        next->year, next->month,
        next->year, next->month);
  else if (next && !month)
    mg_printf(
        conn_,
        "<a href=\"%04d-%02d-%02d.html\">%04d-%02d-%02d→</a>",
        next->year, next->month, next->day,
        next->year, next->month, next->day);
  else
    mg_printf(conn_, "        %s", month ? "" : "   ");

  mg_printf(
      conn_,
      "  <a href=\"%04d.html\">↑%04d</a>"
      "  <a href=\"all.html\">↑all</a>"
      "</div>\n",
      date.year, date.year);
}

void HtmlLineFormatter::FormatHeader(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  char title[64];
  std::snprintf(title, sizeof title, "%04d-%02d-%02d - #esoteric logs", date.year, date.month, date.day);
  HeaderHtml(conn_, title, "log.css");
  FormatNav(date, prev, next);
}

void HtmlLineFormatter::FormatFooter(const YMD& date, const std::optional<YMD>& prev, const std::optional<YMD>& next) {
  FormatNav(date, prev, next);
  FooterHtml(conn_);
}

void HtmlLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    mg_printf(
        conn_,
        "<div class=\"dh\"><a href=\"%04d-%02d-%02d.html\">%04d-%02d-%02d</a></div>\n",
        year, month, day, year, month, day);
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

  mg_printf(
      conn_,
      "<div id=\"l%s\" class=\"r\">"
      "<span class=\"t\">%s</span>"
      "<span class=\"s\"> </span>",
      id, line.tstamp.c_str());

  std::string body = HtmlEscape(line.body);

  if (line.type == LogLine::MESSAGE || line.type == LogLine::ACTION) {
    bool act = line.type == LogLine::ACTION;
    mg_printf(
        conn_,
        "<span class=\"ma h%u\">"
        "<a href=\"#l%s\">%s%s%s</a>"
        "</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"mb\">%s</span>",
        NickHash(line.nick),
        id, act ? "* " : "&lt;", line.nick.c_str(), act ? "" : "&gt;",
        body.c_str());
  } else if (line.type != LogLine::ERROR) {
    mg_printf(
        conn_,
        "<span class=\"x\"><a href=\"#l%s\">-!-</a></span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\"><span class=\"ea h%u\">%s</span> has %s",
        id,
        NickHash(line.nick), line.nick.c_str(), kLineDescriptions[line.type]);
    if (!body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        mg_printf(conn_, " (<span class=\"eb\">%s</span>)", body.c_str());
      else if (line.type == LogLine::NICK || line.type == LogLine::KICK)
        mg_printf(conn_, " <span class=\"ea h%u\">%s</span>", NickHash(body), body.c_str());
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        mg_printf(conn_, ": <span class=\"eb\">%s</span>", body.c_str());
    }
    mg_printf(conn_, ".</span>");
  } else {
    mg_printf(conn_, "<span class=\"e\">unexpected log event :(</span>");
  }

  mg_printf(conn_, "</div>\n");
}

struct TextLineFormatter : public LogLineFormatter {
  using LogLineFormatter::LogLineFormatter;
  void FormatHeader(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override { HeaderText(conn_); }
  void FormatDay(bool multiday, int year, int month, int day) override;
  void FormatLine(const LogLine& line) override;
  void FormatFooter(const YMD&, const std::optional<YMD>&, const std::optional<YMD>&) override {}
};

void TextLineFormatter::FormatDay(bool multiday, int year, int month, int day) {
  if (multiday)
    mg_printf(conn_, "\n%04d-%02d-%02d:\n\n", year, month, day);
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
  mg_printf(conn_, "%s ", line.tstamp.c_str());

  std::string body = UnFormat(line.body);
  if (line.type == LogLine::MESSAGE) {
    mg_printf(conn_, "<%s> %s\n", line.nick.c_str(), body.c_str());
  } else if (line.type == LogLine::ACTION) {
    mg_printf(conn_, "* %s %s\n", line.nick.c_str(), body.c_str());
  } else if (line.type != LogLine::ERROR) {
    mg_printf(conn_, "-!- %s has %s", line.nick.c_str(), kLineDescriptions[line.type]);
    if (!line.body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        mg_printf(conn_, " (%s)", body.c_str());
      else if (line.type == LogLine::KICK)
        mg_printf(conn_, "%s", body.c_str());
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        mg_printf(conn_, ": %s", body.c_str());
    }
    mg_printf(conn_, ".\n");
  } else {
    mg_printf(conn_, "[unexpected log event :(]");
  }
}

struct RawFormatter : public LogFormatter {
  struct mg_connection* conn_;
  unsigned long offset_s_;
  RawFormatter(struct mg_connection* conn) : conn_(conn), offset_s_(0) {}
  void FormatHeader(const YMD& date, const std::optional<YMD>&, const std::optional<YMD>&) override { HeaderText(conn_); }
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
  mg_printf(
      conn_,
      "%c %lu %lu ",
      event.direction() == LogEvent::SENT ? '>' : '<',
      offset_s_ + (unsigned long)(time_us / 1000000u),
      (unsigned long)(time_us % 1000000u));
  if (!event.prefix().empty())
    mg_printf(conn_, ":%s ", event.prefix().c_str());
  mg_printf(conn_, "%s", event.command().c_str());
  for (int i = 0, n = event.args_size(); i < n; ++i)
    mg_printf(conn_, " %s%s", i == n - 1 ? ":" : "", event.args(i).c_str());
  mg_printf(conn_, "\n");
}

} // namespace internal

std::unique_ptr<LogFormatter> LogFormatter::CreateHTML(struct mg_connection* conn) {
  return std::make_unique<internal::HtmlLineFormatter>(conn);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateText(struct mg_connection* conn) {
  return std::make_unique<internal::TextLineFormatter>(conn);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateRaw(struct mg_connection* conn) {
  return std::make_unique<internal::RawFormatter>(conn);
}

} // namespace esologs
