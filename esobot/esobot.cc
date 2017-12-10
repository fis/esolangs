#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <experimental/filesystem>
#include <iostream>
#include <string>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "base/log.h"
#include "esobot/log.pb.h"
#include "event/loop.h"
#include "irc/config.pb.h"
#include "irc/connection.h"
#include "irc/message.h"

extern "C" {
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace esobot {

namespace fs = std::experimental::filesystem;

constexpr char kNick[] = "esowiki";
constexpr char kChannel[] = "#esoteric";
#if 1
constexpr char kIrcServer[] = "chat.freenode.net";
constexpr char kIrcPort[] = "6697";
constexpr char kLogRoot[] = "/home/esowiki/logs";
#else
constexpr char kIrcServer[] = "localhost";
constexpr char kIrcPort[] = "6697";
constexpr char kLogRoot[] = "/home/fis/src/esowiki/tmp";
#endif

constexpr char kUdpPort[] = "8147";
constexpr int kMaxLen = 400;

class RcFeedReader : public event::FdReader {
 public:
  RcFeedReader(event::Loop* loop, irc::Connection* irc);
  void CanRead(int fd) override;

 private:
  irc::Connection* irc_;
  int socket_;
};

RcFeedReader::RcFeedReader(event::Loop* loop, irc::Connection* irc) : irc_(irc) {
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE;

  struct AddrinfoDeleter {
    void operator()(struct addrinfo* addrs) {
      freeaddrinfo(addrs);
    }
  };
  using AddrinfoPtr = std::unique_ptr<struct addrinfo, AddrinfoDeleter>;

  AddrinfoPtr addrs;
  {
    struct addrinfo *a;
    int ret = getaddrinfo("localhost", kUdpPort, &hints, &a);
    if (ret != 0) {
      std::string error = "getaddrinfo: ";
      error += gai_strerror(ret);
      throw base::Exception(error);
    }
    addrs = AddrinfoPtr(a);
  }

  socket_ = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
  if (socket_ == -1)
    throw base::Exception("socket", errno);

  if (bind(socket_, addrs->ai_addr, addrs->ai_addrlen) == -1)
    throw base::Exception("bind", errno);

  addrs.release();

  loop->ReadFd(socket_, this);
}

void RcFeedReader::CanRead(int fd) {
  char msg[4096];

  ssize_t got = recv(socket_, msg, sizeof msg - 1, 0);
  if (got == -1)
    throw base::Exception("recv", errno);
  if (got == 0)
    return;
  msg[got] = 0;

  {
    char* to = msg;
    int len = 0;

    for (char* from = msg; *from && len < kMaxLen; ++from) {
      char c = *from;
      if (c <= 2 || (c >= 4 && c <= 31) || c == 127)
        continue;
      *to++ = c;
      ++len;
    }

    *to = 0;
  }
  if (!*msg)
    return;

  LOG(INFO) << "Wiki message: " << msg;
  irc_->Send({ "PRIVMSG", kChannel, msg });
}

class Logger {
 public:
  Logger();
  void Log(const irc::Message& msg);

 private:
  using us = std::chrono::microseconds;

  us current_day_us_;
  std::unique_ptr<google::protobuf::io::FileOutputStream> current_log_;

  std::pair<us, us> Now() {
    using namespace std::chrono_literals;
    us now_us = std::chrono::duration_cast<us>(std::chrono::system_clock::now().time_since_epoch());
    us time_us = now_us % 86400000000us;
    return std::pair(now_us - time_us, time_us);
  }

  void OpenLog(us day_us);
};

Logger::Logger() {
  current_day_us_ = Now().first;
  OpenLog(current_day_us_);
}

void Logger::Log(const irc::Message& msg) {
  auto [day_us, time_us] = Now();

  bool logged =
      ((msg.command_is("PRIVMSG")
        || msg.command_is("NOTICE")
        || msg.command_is("JOIN")
        || msg.command_is("PART")
        || msg.command_is("KICK")
        || msg.command_is("MODE")
        || msg.command_is("TOPIC"))
       && msg.arg_is(0, kChannel))
      || msg.command_is("QUIT")
      || msg.command_is("NICK");
  if (!logged)
    return;

  if (day_us != current_day_us_) {
    current_day_us_ = day_us;
    OpenLog(day_us);
  }

  LogEvent log_event;
  log_event.set_time_us(time_us.count());
  if (!msg.prefix().empty())
    log_event.set_prefix(msg.prefix());
  log_event.set_command(msg.command());
  for (const auto& arg : msg.args())
    log_event.add_args(arg);

  {
    const int size = log_event.ByteSize();

    google::protobuf::io::CodedOutputStream out(current_log_.get());

    out.WriteLittleEndian32(size);

    std::uint8_t* buffer = out.GetDirectBufferForNBytesAndAdvance(size);
    if (buffer)
      log_event.SerializeWithCachedSizesToArray(buffer);
    else
      log_event.SerializeWithCachedSizes(&out);
  }

  current_log_->Flush();
};

void Logger::OpenLog(us day_us) {
  if (current_log_)
    current_log_.reset();

  using clock = std::chrono::system_clock;
  std::time_t day = clock::to_time_t(clock::time_point(std::chrono::duration_cast<clock::duration>(day_us)));
  std::tm* day_tm = std::gmtime(&day);

  char buf[16];

  fs::path log_file(kLogRoot);

  std::snprintf(buf, sizeof buf, "%d", 1900 + day_tm->tm_year);
  log_file /= buf;
  std::snprintf(buf, sizeof buf, "%d", day_tm->tm_mon + 1);
  log_file /= buf;

  fs::create_directories(log_file);

  std::snprintf(buf, sizeof buf, "%d.pb", day_tm->tm_mday);
  log_file /= buf;

  int fd = open(log_file.c_str(), O_WRONLY | O_CREAT, 0640);
  if (fd == -1)
    throw base::Exception("log file open failed", errno); // TODO: include path

  current_log_ = std::make_unique<google::protobuf::io::FileOutputStream>(fd);
  current_log_->SetCloseOnDelete(true);
}

class EsoBot : public irc::Connection::Reader {
 public:
  EsoBot();
  void Run();
  void MessageReceived(const irc::Message& msg) override;

 private:
  event::Loop loop_;
  Logger log_;
  std::unique_ptr<irc::Connection> irc_;
  std::unique_ptr<RcFeedReader> rc_feed_reader_;
};

EsoBot::EsoBot() {
  irc::Config config;
  config.set_nick(kNick);
  config.set_user(kNick);
  config.set_realname(kNick);
  irc::Config::Server* server = config.add_servers();
  server->set_host(kIrcServer);
  server->set_port(kIrcPort);
  server->mutable_tls();
  config.add_channels(kChannel);

  irc_ = std::make_unique<irc::Connection>(config, &loop_);
  irc_->AddReader(this);

  rc_feed_reader_ = std::make_unique<RcFeedReader>(&loop_, irc_.get());
}

void EsoBot::Run() {
  irc_->Start();
  while (true)
    loop_.Poll();
}

void EsoBot::MessageReceived(const irc::Message& msg) {
  log_.Log(msg);

  std::string debug(msg.command());
  for (const std::string& arg : msg.args()) {
    debug += ' ';
    debug += arg;
  }
  LOG(DEBUG) << debug;
}

} // namespace esobot

int main(int argc, char *argv[]) {
  esobot::EsoBot bot;
  bot.Run();
  return 0;
}
