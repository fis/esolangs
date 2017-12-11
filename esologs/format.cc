#include <cstdarg>
#include <cstdio>

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
          "<li><a href=\"%s.html\">%s</a> (<a href=\"%s.txt\">text</a>)</li>\n",
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

class HtmlLogFormatter : public LogFormatter {
 public:
  HtmlLogFormatter(struct mg_connection* conn);
  ~HtmlLogFormatter();
  void FormatEvent(const LogEvent& event) override;

 private:
  struct mg_connection* conn_;
};

HtmlLogFormatter::HtmlLogFormatter(struct mg_connection* conn) : conn_(conn) {
  HeaderHtml(conn_, "TODO");
}

HtmlLogFormatter::~HtmlLogFormatter() {
  FooterHtml(conn_);
}

void HtmlLogFormatter::FormatEvent(const LogEvent& event) {
  auto tstamp = event.time_us();

  mg_printf(
      conn_,
      "<div class=\"r\">"
      "<span class=\"t\">%02d:%02d:%02d</span>"
      "<span class=\"s\"> </span>",
      (int)(tstamp / 3600000000),
      (int)(tstamp / 60000000 % 60),
      (int)(tstamp / 1000000 % 60));

  std::string nick;
  if (event.direction() == LogEvent::SENT) {
    nick = "esowiki";
  } else {
    nick = event.prefix();
    std::size_t sep = nick.find('!');
    if (sep != std::string::npos)
      nick.erase(sep);
    else
      nick = "?unknown?";
  }

  unsigned nick_hash = 0;
  for (const auto& c : nick)
    nick_hash = 31 * nick_hash + (unsigned char)c;
  nick_hash %= 10;

  int body_arg = event.command() == "QUIT" ? 0 : 1;
  std::string body;
  if (event.args_size() > body_arg) {
    for (const auto& c : event.args(body_arg)) {
      if (c < 32)
        continue;
      else if (c == '<')
        body += "&lt;";
      else if (c == '>')
        body += "&gt;";
      else if (c == '&')
        body += "&amp;";
      else
        body += c;
    }
  }

  if (event.command() == "PRIVMSG" || event.command() == "NOTICE") {
    mg_printf(
        conn_,
        "<span class=\"ma h%d\">&lt;%s&gt;</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"mb\">%s</span>",
        nick_hash, nick.c_str(), body.c_str());
  } else if (event.command() == "JOIN" || event.command() == "PART" || event.command() == "QUIT") {
    mg_printf(
        conn_,
        "<span class=\"x\">-!-</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\"><span class=\"ea h%d\">%s</span> has %s",
        nick_hash,
        nick.c_str(),
        event.command() == "JOIN" ? "joined" : event.command() == "PART" ? "left" : "quit");
    if (!body.empty())
      mg_printf(conn_, " (<span class=\"eb\">%s</span>)", body.c_str());
    mg_printf(conn_, ".</span>");
  } else if (event.command() == "KICK") {
    mg_printf(
        conn_,
        "<span class=\"x\">-!-</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\"><span class=\"ea h%d\">%s</span> has kicked <span class=\"ea\">%s</span>.</span>",
        nick_hash, nick.c_str(), body.c_str());
  } else if (event.command() == "MODE" || event.command() == "TOPIC") {
    mg_printf(
        conn_,
        "<span class=\"x\">-!-</span>"
        "<span class=\"s\"> </span>"
        "<span class=\"ed\"><span class=\"ea h%d\">%s</span> has set %s: <span class\"eb\">%s</span>.</span>",
        nick_hash,
        nick.c_str(),
        event.command() == "MODE" ? "channel mode" : "topic",
        body.c_str());
  } else {
    mg_printf(conn_, "<span class=\"e\">unexpected log event :(</span>");
  }

  mg_printf(conn_, "</div>\n");
}

class TextLogFormatter : public LogFormatter {
 public:
  TextLogFormatter(struct mg_connection* conn);
  void FormatEvent(const LogEvent& event) override;

 private:
  struct mg_connection* conn_;
};

TextLogFormatter::TextLogFormatter(struct mg_connection* conn) : conn_(conn) {
  mg_printf(
      conn,
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n");
}

void TextLogFormatter::FormatEvent(const LogEvent& event) {
  auto tstamp = event.time_us();

  mg_printf(
      conn_,
      "%02d:%02d:%02d ",
      (int)(tstamp / 3600000000),
      (int)(tstamp / 60000000 % 60),
      (int)(tstamp / 1000000 % 60));

  std::string nick;
  if (event.direction() == LogEvent::SENT) {
    nick = "esowiki";
  } else {
    nick = event.prefix();
    std::size_t sep = nick.find('!');
    if (sep != std::string::npos)
      nick.erase(sep);
    else
      nick = "?unknown?";
  }

  int body_arg = event.command() == "QUIT" ? 0 : 1;
  std::string body;
  if (event.args_size() > body_arg)
    body = event.args(body_arg);

  if (event.command() == "PRIVMSG" || event.command() == "NOTICE") {
    mg_printf(conn_, "<%s> %s", nick.c_str(), body.c_str());
  } else if (event.command() == "JOIN" || event.command() == "PART" || event.command() == "QUIT") {
    mg_printf(
        conn_,
        "-!- %s has %s",
        nick.c_str(),
        event.command() == "JOIN" ? "joined" : event.command() == "PART" ? "left" : "quit");
    if (!body.empty())
      mg_printf(conn_, " (%s)", body.c_str());
    mg_printf(conn_, ".");
  } else if (event.command() == "KICK") {
    mg_printf(conn_, "-!- %s has kicked %s.", nick.c_str(), body.c_str());
  } else if (event.command() == "MODE" || event.command() == "TOPIC") {
    mg_printf(
        conn_,
        "-!- %s has set %s: %s.",
        nick.c_str(),
        event.command() == "MODE" ? "channel mode" : "topic",
        body.c_str());
  } else {
    mg_printf(conn_, "unexpected log event :(");
  }

  mg_printf(conn_, "\n");
}

} // namespace internal

std::unique_ptr<LogFormatter> LogFormatter::CreateHTML(struct mg_connection* conn) {
  return std::make_unique<internal::HtmlLogFormatter>(conn);
}

std::unique_ptr<LogFormatter> LogFormatter::CreateText(struct mg_connection* conn) {
  return std::make_unique<internal::TextLogFormatter>(conn);
}

} // namespace esologs
