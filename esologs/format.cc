#include <cstdarg>
#include <cstdio>
#include <functional>

#include "esologs/format.h"

namespace esologs {

namespace {

void HeaderHtml(struct mg_connection* conn, const char* title) {
  mg_printf(
      conn,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>%s</title><link rel=\"stylesheet\" type=\"text/css\" href=\"log.css\"></head>\n"
      "<body>\n",
      title);
}

void FooterHtml(struct mg_connection* conn) {
  mg_printf(conn, "</body>\n</html>\n");
}

} // unnamed namespace

void FormatIndex(struct mg_connection* conn, const LogIndex& index) {
  HeaderHtml(conn, "#esoteric logs");

  index.For([&conn](int y, int m, int d) {
      char ymd[16];
      std::snprintf(ymd, sizeof ymd, "%04d-%02d-%02d", y, m, d);
      mg_printf(
          conn,
          "<li>"
          "<a href=\"%s.html\">%s</a>"
          " (<a href=\"%s.txt\">text</a>)"
          "</li>\n",
          ymd, ymd, ymd);
    });

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
    // TODO: add NICK
    MESSAGE,
    JOIN,
    PART,
    QUIT,
    KICK,
    MODE, // TODO: fix args
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
  /* JOIN: */ "joined",
  /* PART: */ "left",
  /* QUIT: */ "quit",
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

  int body_arg = line.type == LogLine::QUIT ? 0 : 1;
  if (event.args_size() > body_arg) {
    // TODO: better sanitization, don't HTML-escape text format
    for (const auto& c : event.args(body_arg)) {
      if (c < 32)
        continue;
      line.body += c;
    }
  }

  FormatLine(line);
}

struct HtmlLineFormatter : public LogLineFormatter {
  using LogLineFormatter::LogLineFormatter;
  void FormatHeader(int y, int m, int d) override;
  void FormatLine(const LogLine& line) override;
  void FormatFooter() override;
};

void HtmlLineFormatter::FormatHeader(int y, int m, int d) {
  char title[64];
  std::snprintf(title, sizeof title, "%04d-%02d-%02d - #esoteric logs", y, m, d);
  HeaderHtml(conn_, title);
}

void HtmlLineFormatter::FormatFooter() {
  FooterHtml(conn_);
}

void HtmlLineFormatter::FormatLine(const LogLine& line) {
  mg_printf(
      conn_,
      "<div class=\"r\">"
      "<span class=\"t\">%s</span>"
      "<span class=\"s\"> </span>",
      line.tstamp.c_str());

  unsigned nick_hash = 0;
  for (const auto& c : line.nick)
    nick_hash = 31 * nick_hash + (unsigned char)c;
  nick_hash %= 10;

  std::string body;
  for (const auto& c : line.body) {
    if (c == '<')
      body += "&lt;";
    else if (c == '>')
      body += "&gt;";
    else if (c == '&')
      body += "&amp;";
    else
      body += c;
  }

  if (line.type == LogLine::MESSAGE) {
    mg_printf(
        conn_,
        "<span class=\"ma h%d\">&lt;%s&gt;</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"mb\">%s</span>",
        nick_hash, line.nick.c_str(), body.c_str());
  } else if (line.type != LogLine::ERROR) {
    mg_printf(
        conn_,
        "<span class=\"x\">-!-</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\"><span class=\"ea h%d\">%s</span> has %s",
        nick_hash,
        line.nick.c_str(),
        kLineDescriptions[line.type]);
    if (!line.body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        mg_printf(conn_, " (<span class=\"eb\">%s</span>)", body.c_str());
      else if (line.type == LogLine::KICK)
        mg_printf(conn_, "<span class=\"ea\">%s</span>", body.c_str()); // TODO: nick hash
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        mg_printf(conn_, ": <span class\"eb\">%s</span>", body.c_str());
    }
    mg_printf(conn_, ".</span>");
  } else {
    mg_printf(conn_, "<span class=\"e\">unexpected log event :(</span>");
  }

  mg_printf(conn_, "</div>\n");
}

struct TextLineFormatter : public LogLineFormatter {
  using LogLineFormatter::LogLineFormatter;
  void FormatHeader(int, int, int) override;
  void FormatLine(const LogLine& line) override;
  void FormatFooter() override;
};

void TextLineFormatter::FormatHeader(int, int, int) {
  mg_printf(
      conn_,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n");
}

void TextLineFormatter::FormatFooter() {}

void TextLineFormatter::FormatLine(const LogLine& line) {
  mg_printf(conn_, "%s ", line.tstamp.c_str());

  if (line.type == LogLine::MESSAGE) {
    mg_printf(conn_, "<%s> %s\n", line.nick.c_str(), line.body.c_str());
  } else if (line.type != LogLine::ERROR) {
    mg_printf(conn_, "-!- %s has %s", line.nick.c_str(), kLineDescriptions[line.type]);
    if (!line.body.empty()) {
      if (line.type == LogLine::PART || line.type == LogLine::QUIT)
        mg_printf(conn_, " (%s)", line.body.c_str());
      else if (line.type == LogLine::KICK)
        mg_printf(conn_, "%s", line.body.c_str());
      else if (line.type == LogLine::MODE || line.type == LogLine::TOPIC)
        mg_printf(conn_, ": %s", line.body.c_str());
    }
    mg_printf(conn_, ".\n");
  } else {
    mg_printf(conn_, "[unexpected log event :(]");
  }
}

} // namespace internal

std::unique_ptr<LogFormatter> LogFormatter::CreateHTML(struct mg_connection* conn) {
  return std::make_unique<internal::HtmlLineFormatter>(conn);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateText(struct mg_connection* conn) {
  return std::make_unique<internal::TextLineFormatter>(conn);
}

} // namespace esologs
