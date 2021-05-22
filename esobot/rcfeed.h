#ifndef ESOBOT_RCFEED_H_
#define ESOBOT_RCFEED_H_

#include <vector>

#include "esobot/config.pb.h"
#include "event/loop.h"
#include "irc/bot/plugin.h"

namespace esobot {

class RcFeed {
 public:
  RcFeed(event::Loop* loop, const RcFeedConfig& config);
  ~RcFeed();

  std::unique_ptr<irc::bot::Plugin> AddHost(const RcFeedPlugin& config, irc::bot::PluginHost* host);

 private:
  event::Loop* loop_;
  int socket_;

  struct Host {
    irc::bot::PluginHost* host;
    const std::string channel;
    Host(irc::bot::PluginHost* h, const std::string& ch) : host(h), channel(ch) {}
  };
  std::vector<Host> hosts_; // TODO: on plugin desctruction, remove it from list

  void SocketReady(int);

  event::FdReaderM<RcFeed, &RcFeed::SocketReady> socket_ready_callback_;
};

} // namespace esobot

#endif // ESOBOT_RCFEED_H_

// Local Variables:
// mode: c++
// End:
