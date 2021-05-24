#ifndef ESOBOT_RCFEED_H_
#define ESOBOT_RCFEED_H_

#include <vector>

#include "esobot/config.pb.h"
#include "event/loop.h"
#include "irc/bot/module.h"

namespace esobot {

class RcFeed : public irc::bot::Module {
 public:
  RcFeed(const RcFeedConfig& config, irc::bot::ModuleHost* host);
  ~RcFeed();

 private:
  event::Loop* loop_;

  int socket_;

  struct Target {
    irc::bot::Connection* conn;
    const std::string chan;
    Target(irc::bot::Connection* co, const std::string& ch) : conn(co), chan(ch) {}
  };
  std::vector<Target> targets_;

  void SocketReady(int);

  event::FdReaderM<RcFeed, &RcFeed::SocketReady> socket_ready_callback_;
};

} // namespace esobot

#endif // ESOBOT_RCFEED_H_

// Local Variables:
// mode: c++
// End:
