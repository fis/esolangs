#ifndef ESOBOT_RCFEED_H_
#define ESOBOT_RCFEED_H_

#include "esobot/config.pb.h"
#include "event/loop.h"
#include "irc/bot/plugin.h"

namespace esobot {

class RcFeed : public irc::bot::Plugin {
 public:
  RcFeed(const RcFeedConfig& config, irc::bot::PluginHost* host);
  ~RcFeed();

 private:
  const std::string channel_;
  irc::bot::PluginHost* host_;
  int socket_;

  void SocketReady(int);

  event::FdReaderM<RcFeed, &RcFeed::SocketReady> socket_ready_callback_;
};

} // namespace esobot

#endif // ESOBOT_RCFEED_H_

// Local Variables:
// mode: c++
// End:
