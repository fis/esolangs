#ifndef ESOBOT_RCFEED_H_
#define ESOBOT_RCFEED_H_

#include "event/loop.h"

namespace esobot {

struct RcFeedListener {
  virtual void RecentChange(const char* message) = 0;
  virtual ~RcFeedListener() = default;
};

class RcFeedReader {
 public:
  RcFeedReader(event::Loop* loop, RcFeedListener* listener);

 private:
  RcFeedListener* listener_;
  int socket_;

  void SocketReady(int);

  event::FdReaderM<RcFeedReader, &RcFeedReader::SocketReady> socket_ready_callback_;
};

} // namespace esobot

#endif // ESOBOT_RCFEED_H_

// Local Variables:
// mode: c++
// End:
