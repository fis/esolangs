#include <cerrno>
#include <cstring>
#include <memory>

#include "esobot/rcfeed.h"
#include "event/loop.h"

extern "C" {
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
}

namespace esobot {

constexpr int kMaxLen = 400;

RcFeed::RcFeed(event::Loop* loop, const RcFeedConfig& config) : loop_(loop), socket_ready_callback_(this) {
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
    int ret = getaddrinfo("localhost", config.port().c_str(), &hints, &a);
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

  addrs.reset();

  loop_->ReadFd(socket_, base::borrow(&socket_ready_callback_));
}

RcFeed::~RcFeed() {
  loop_->ReadFd(socket_);
}

std::unique_ptr<irc::bot::Plugin> RcFeed::AddHost(const RcFeedPlugin& config, irc::bot::PluginHost* host) {
  hosts_.emplace_back(host, config.channel());
  return std::make_unique<irc::bot::Plugin>();
}

void RcFeed::SocketReady(int) {
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

  LOG(INFO) << "wiki message: " << msg;
  for (const auto& host : hosts_) {
    host.host->Send({ "PRIVMSG", host.channel, msg });
  }
}

} // namespace esobot
