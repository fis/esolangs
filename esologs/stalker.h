#ifndef ESOLOGS_STALKER_H_
#define ESOLOGS_STALKER_H_

#include <atomic>
#include <deque>
#include <mutex>

#include <date/date.h>
#include <prometheus/registry.h>

#include "base/owner_set.h"
#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "esologs/log.pb.h"
#include "event/loop.h"
#include "event/socket.h"
#include "web/websocket.h"

namespace esologs {

class Stalker : public event::ServerSocket::Watcher, public event::Socket::Watcher {
 public:
  Stalker(const Config& config, event::Loop* loop, LogIndex* index, prometheus::Registry* metric_registry = nullptr);
  ~Stalker();

  void Format(LogFormatter* fmt);
  void UpdateClients();

  web::WebsocketClientHandler* AddClient();

  bool loaded() { return events_loaded_; }

  void Accepted(std::unique_ptr<event::Socket> socket) override;
  void CanRead() override;

  void ConnectionOpen() override {}
  void ConnectionFailed(const std::string& error) override {}
  void CanWrite() override {}

 private:
  class Client;

  static constexpr date::days kBackfillDays{3};
  static constexpr std::size_t kQueueSize = 1000;

  event::Loop* loop_;
  LogIndex* index_;

  std::unique_ptr<event::ServerSocket> server_;
  std::unique_ptr<event::Socket> writer_;
  std::array<char, 4096> read_buffer_;

  std::deque<LogEvent> events_;
  std::mutex events_lock_;
  std::atomic<bool> events_loaded_ = false;

  base::owner_set<Client> clients_;
  std::mutex clients_lock_;
  std::atomic<bool> clients_active_ = false;

  std::int64_t last_day_;
  std::uint64_t last_line_;

  prometheus::Gauge* metric_clients_ = nullptr;
  prometheus::Gauge* metric_last_received_ = nullptr;

  void Backfill();
};

} // namespace esologs

#endif // ESOLOGS_STALKER_H_

// Local Variables:
// mode: c++
// End:
