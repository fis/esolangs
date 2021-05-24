#ifndef ESOLOGS_STALKER_H_
#define ESOLOGS_STALKER_H_

#include <atomic>
#include <deque>
#include <mutex>

#include <date/date.h>
#include <prometheus/registry.h>

#include "base/exc.h"
#include "base/unique_set.h"
#include "esologs/config.pb.h"
#include "esologs/format.h"
#include "esologs/index.h"
#include "esologs/log.pb.h"
#include "event/loop.h"
#include "event/socket.h"
#include "web/websocket.h"

namespace esologs {

struct IndexMapper {
  virtual LogIndex* index(const std::string& target) = 0;
  virtual ~IndexMapper() = default;
};

class Stalker : public event::Socket::Watcher, public event::Timed {
 public:
  Stalker(const Config& config, event::Loop* loop, IndexMapper* indices, prometheus::Registry* metric_registry = nullptr);
  ~Stalker();

  void Format(const TargetConfig& cfg, LogFormatter* fmt);

  web::WebsocketClientHandler* AddClient(const TargetConfig& config);

  bool loaded() { return events_loaded_; }

  // event::Socket::Watcher
  void ConnectionOpen() override;
  void ConnectionFailed(std::unique_ptr<base::error> error) override;
  void CanRead() override;
  void CanWrite() override {}
  // event::Timed
  void TimerExpired(bool) override { ConnectPipe(); }

 private:
  enum State {
    kConnecting,
    kConnected,
    kWaiting,
  };
  struct Target {
    Target(const std::string& n) : name(n) {}
    const std::string name; // TODO: maybe put TargetConfig copy here instead of client; less passing around
    std::int64_t last_day = 0;
    std::uint64_t last_line = 0;
    std::deque<LogEvent> events;
    std::mutex events_lock;
  };
  class Client;

  void ConnectPipe();
  void ResetPipe();

  void UpdateClients();
  void Backfill();

  Target* target(const std::string& name);

  static constexpr date::days kBackfillDays{3};
  static constexpr std::size_t kQueueSize = 1000;
  static constexpr auto kReconnectDelay = std::chrono::seconds(30);

  event::Loop* const loop_;
  IndexMapper* const indices_;
  std::vector<std::unique_ptr<Target>> targets_;
  std::string pipe_path_;

  State state_ = kWaiting;
  std::unique_ptr<event::Socket> pipe_;
  std::array<char, 4096> read_buffer_;

  std::atomic<bool> events_loaded_ = false;

  base::unique_set<Client> clients_;
  std::mutex clients_lock_;
  std::atomic<bool> clients_active_ = false;

  prometheus::Gauge* metric_clients_ = nullptr;
  prometheus::Gauge* metric_last_received_ = nullptr;
};

} // namespace esologs

#endif // ESOLOGS_STALKER_H_

// Local Variables:
// mode: c++
// End:
