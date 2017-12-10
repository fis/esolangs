#include <cerrno>
#include <cstring>

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

constexpr char kUdpPort[] = "8147";
constexpr int kMaxLen = 400;

RcFeedReader::RcFeedReader(event::Loop* loop, RcFeedListener* listener)
    : listener_(listener), socket_ready_callback_(this) {

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

  loop->ReadFd(socket_, &socket_ready_callback_);
}

void RcFeedReader::SocketReady(int) {
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

  listener_->RecentChange(msg);
}

} // namespace esobot
