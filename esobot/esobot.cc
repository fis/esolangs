#include <cerrno>
#include <iostream>
#include <string>

#include "base/log.h"
#include "event/loop.h"
#include "irc/config.pb.h"
#include "irc/connection.h"
#include "irc/message.h"

extern "C" {
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}

constexpr char kNick[] = "esowiki";
constexpr char kChannel[] = "#esoteric";
constexpr char kIrcServer[] = "chat.freenode.net";
constexpr char kIrcPort[] = "6697";
constexpr char kUdpPort[] = "8147";
constexpr int kMaxLen = 400;

class UdpReader : public event::FdReader {
 public:
  UdpReader(event::Loop* loop, irc::Connection* irc);
  void CanRead(int fd) override;

 private:
  irc::Connection* irc_;
  int socket_;
};

UdpReader::UdpReader(event::Loop* loop, irc::Connection* irc) : irc_(irc) {
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

void UdpReader::CanRead(int fd) {
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

int main(int argc, char *argv[]) {
  irc::Config config;
  config.set_nick(kNick);
  config.set_user(kNick);
  config.set_realname(kNick);
  irc::Config::Server* server = config.add_servers();
  server->set_host(kIrcServer);
  server->set_port(kIrcPort);
  server->mutable_tls();
  config.add_channels(kChannel);

  event::Loop loop;
  irc::Connection connection(config, &loop);
  UdpReader udp_reader(&loop, &connection);

  connection.AddReader([](const irc::Message& msg) {
      std::string debug(msg.command());
      for (const std::string& arg : msg.args()) {
        debug += ' ';
        debug += arg;
      }
      LOG(DEBUG) << debug;
    });

  connection.Start();

  while (true)
    loop.Poll();
}
