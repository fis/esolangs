#ifndef ESOLOGS_STALKER_H_
#define ESOLOGS_STALKER_H_

#include <deque>
#include <mutex>

#include <date/date.h>

#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "esologs/log.pb.h"
#include "event/loop.h"
#include "event/socket.h"

namespace esologs {

class Stalker : public event::ServerSocket::Watcher, public event::Socket::Watcher {
 public:
  Stalker(const Config& config, event::Loop* loop, LogIndex* index);

  void Format(LogFormatter* fmt);

  void Accepted(std::unique_ptr<event::Socket> socket) override;
  void CanRead() override;

  void ConnectionOpen() override {}
  void ConnectionFailed(const std::string& error) override {}
  void CanWrite() override {}

 private:
  static constexpr date::days kBackfillDays{3};
  static constexpr std::size_t kQueueSize = 1000;

  event::Loop* loop_;
  LogIndex* index_;

  std::unique_ptr<event::ServerSocket> server_;
  std::unique_ptr<event::Socket> writer_;
  std::array<char, 4096> read_buffer_;

  std::deque<LogEvent> events_;
  std::mutex events_lock_;

  std::int64_t last_day_;
  std::uint64_t last_line_;

  void Backfill();
};

} // namespace esologs

#endif // ESOLOGS_STALKER_H_

// Local Variables:
// mode: c++
// End:
