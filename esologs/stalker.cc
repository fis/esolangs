#include <date/date.h>
#include <prometheus/gauge.h>

#include "base/buffer.h"
#include "esologs/config.pb.h"
#include "esologs/stalker.h"
#include "web/server.h"

namespace esologs {

class Stalker::Client : public web::WebsocketClientHandler {
 public:
  Client(Stalker* stalker, Stalker::Target* target, TargetConfig config) : stalker_(stalker), target_(target), config_(config) {}

  bool Send(const LogEvent& event);
  void Close();

  bool registered() const noexcept { return socket_ && sent_day_; }

  bool has_event(const LogEvent& event) const noexcept {
    const LogEventId& id = event.event_id();
    return id.day() < sent_day_ || (id.day() == sent_day_ && id.line() <= sent_line_);
  }

  void WebsocketReady(web::Websocket* socket) override;
  web::Websocket::Result WebsocketData(web::Websocket* socket, web::Websocket::Type type, const unsigned char* buf, std::size_t size) override;
  void WebsocketClose(web::Websocket* socket) override;

 private:
  Stalker* const stalker_;
  Stalker::Target* const target_;
  TargetConfig const config_;
  web::Websocket* socket_ = nullptr;

  std::int64_t sent_day_ = 0;
  std::uint64_t sent_line_ = 0;

  std::string buffer_;
  std::unique_ptr<LogFormatter> fmt_ = LogFormatter::CreateHTML(&buffer_);

  friend class Stalker;
};

bool Stalker::Client::Send(const LogEvent& event) {
  const LogEventId& id = event.event_id();
  std::int64_t day = id.day();
  std::uint64_t line = id.line();

  std::optional<std::size_t> wrote;

  {
    base::byte_array<8> buf;
    base::write_i32(day, &buf[0]);
    base::write_u32(line, &buf[4]);
    wrote = socket_->Write(web::Websocket::Type::kBinary, buf.data(), buf.size());
    if (!wrote || *wrote != buf.size()) {
      LOG(WARNING) << "stalker websocket: header write failed";
      return false;
    }
  }

  if (day > sent_day_) {
    YMD ymd(YMD::day_number, day);
    fmt_->FormatDay(true, ymd.year, ymd.month, ymd.day);
  }
  fmt_->FormatEvent(event, config_);

  std::size_t body_size = buffer_.size();
  wrote = socket_->Write(web::Websocket::Type::kText, buffer_.data(), body_size);
  buffer_.clear();

  if (!wrote || *wrote != body_size) {
    LOG(WARNING) << "stalker websocket: body write failed";
    return false;
  }

  sent_day_ = day;
  sent_line_ = line;
  return true;
}

void Stalker::Client::Close() {
  if (socket_)
    socket_->Close();
}

void Stalker::Client::WebsocketReady(web::Websocket* socket) {
  std::lock_guard<std::mutex> lock(stalker_->clients_lock_);
  socket_ = socket;
}

web::Websocket::Result Stalker::Client::WebsocketData(web::Websocket* socket, web::Websocket::Type type, const unsigned char* buf, std::size_t size) {
  if (type != web::Websocket::Type::kBinary) {
    LOG(WARNING) << "stalker websocket: unexpected non-binary message";
    socket->Close(web::Websocket::Status::kInvalidData);
    return web::Websocket::Result::kClose;
  }

  if (size != 8) {
    LOG(WARNING) << "stalker websocket: unexpected size, size = " << size;
    socket->Close(web::Websocket::Status::kProtocolError);
    return web::Websocket::Result::kClose;
  }

  auto msg_day = base::read_i32(&buf[0]);
  auto msg_line = base::read_u32(&buf[4]);

  {
    std::lock_guard<std::mutex> lock(stalker_->clients_lock_);
    if (!sent_day_)
      LOG(INFO) << "stalker: new websocket (" << msg_day << ", " << msg_line << ")";
    sent_day_ = msg_day;
    sent_line_ = msg_line;
    stalker_->clients_active_ = true;
  }

  stalker_->UpdateClients();
  return web::Websocket::Result::kKeepOpen;
}

void Stalker::Client::WebsocketClose(web::Websocket* socket) {
  std::lock_guard<std::mutex> lock(stalker_->clients_lock_);

  stalker_->clients_.erase(this);  // self-destruct
  if (stalker_->metric_clients_)
    stalker_->metric_clients_->Set(stalker_->clients_.size());

  bool active = false;
  for (Client* client : stalker_->clients_) {
    if (client->registered()) {
      active = true;
      break;
    }
  }
  stalker_->clients_active_ = active;
}

Stalker::Stalker(const Config& config, event::Loop* loop, IndexMapper* indices, prometheus::Registry* metric_registry)
    : loop_(loop), indices_(indices), pipe_path_(config.pipe_socket())
{
  for (const auto& target_config : config.target())
    targets_.emplace_back(std::make_unique<Target>(target_config.name()));

  if (metric_registry) {
    metric_clients_ = &prometheus::BuildGauge()
        .Name("esologs_stalker_active_clients")
        .Help("How many websocket stalker clients are currently active?")
        .Register(*metric_registry)
        .Add({});
    metric_last_received_ = &prometheus::BuildGauge()
        .Name("esologs_stalker_last_message_time")
        .Help("When was the last log message received from the stalker feed?")
        .Register(*metric_registry)
        .Add({});
  }

  ConnectPipe();
}

Stalker::~Stalker() {}

void Stalker::Format(const TargetConfig& cfg, LogFormatter* fmt) {
  std::int64_t day = 0;

  int link_year;
  {
    LogIndex* index = indices_->index(cfg.name());
    std::lock_guard<std::mutex> lock(*index->lock());
    link_year = index->default_year();
  }
  fmt->FormatStalkerHeader(link_year, cfg.title());

  Target* tgt = target(cfg.name());
  std::lock_guard<std::mutex> lock(tgt->events_lock);
  for (const LogEvent& event : tgt->events) {
    std::int64_t event_day = event.event_id().day();
    std::uint64_t event_line = event.event_id().line();

    if (event_day > day) {
      day = event_day;
      YMD ymd(YMD::day_number, day);
      fmt->FormatDay(true, ymd.year, ymd.month, ymd.day);

      if (event_line > 0)
        fmt->FormatElision();
    }

    fmt->FormatEvent(event, cfg);
  }

  fmt->FormatStalkerFooter();
}

web::WebsocketClientHandler* Stalker::AddClient(const TargetConfig& config) {
  std::lock_guard<std::mutex> lock(clients_lock_);
  auto* handler = clients_.emplace(this, target(config.name()), config);
  if (metric_clients_)
    metric_clients_->Set(clients_.size());
  return handler;
}

void Stalker::ConnectionOpen() {
  CHECK(state_ == kConnecting);

  LOG(INFO) << "stalker: pipe connected";
  state_ = kConnected;
  pipe_->WantRead(true);

  if (!events_loaded_) {
    Backfill();
    events_loaded_ = true;
  }
}

void Stalker::ConnectionFailed(std::unique_ptr<base::error> error) {
  CHECK(state_ == kConnecting);

  LOG(WARNING) << "stalker: pipe connection failed: " << pipe_path_ << ": " << *error;
  ResetPipe();
}

void Stalker::CanRead() {
  CHECK(pipe_);

  std::size_t len;
  {
    auto ret = pipe_->Read(read_buffer_.data(), read_buffer_.size());
    if (!ret.ok()) {
      LOG(WARNING) << "stalker: pipe read failed: " << *ret.error();
      ResetPipe();
      return;
    }
    len = ret.size();
  }
  if (len == 0)
    return;

  LogEvent event;
  if (!event.ParseFromArray(read_buffer_.data(), len)) {
    LOG(WARNING) << "stalker: pipe event parse error, len = " << len;
    return;
  }

  Target* tgt = target(event.event_id().target());
  if (!tgt) {
    LOG(WARNING) << "stalker: pipe event for unknown target: " << event.event_id().target();
    return;
  }

  {
    std::lock_guard<std::mutex> lock(tgt->events_lock);

    std::int64_t event_day = event.event_id().day();
    std::uint64_t event_line = event.event_id().line();
    if (event_day < tgt->last_day || (event_day == tgt->last_day && event_line <= tgt->last_line))
      return; // already seen
    tgt->last_day = event_day;
    tgt->last_line = event_line;

    tgt->events.emplace_back(std::move(event));
    if (tgt->events.size() > kQueueSize)
      tgt->events.pop_front();
  }

  if (metric_last_received_)
    metric_last_received_->SetToCurrentTime();

  if (clients_active_)
    UpdateClients();
}

void Stalker::ConnectPipe() {
  CHECK(state_ == kWaiting);
  CHECK(!pipe_);

  state_ = kConnecting;
  auto sock = event::Socket::Builder()
      .loop(loop_)
      .watcher(this)
      .unix(pipe_path_)
      .kind(event::Socket::SEQPACKET)
      .Build();
  if (sock.ok()) {
    pipe_ = sock.ptr();
    pipe_->Start();
  } else {
    ConnectionFailed(sock.error());
  }
}

void Stalker::ResetPipe() {
  state_ = kWaiting;
  pipe_.reset();
  loop_->Delay(kReconnectDelay, base::borrow(this));
}

void Stalker::UpdateClients() {
  std::lock_guard<std::mutex> lock_clients(clients_lock_);

  for (Client* client : clients_) {
    if (!client->registered())
      continue;

    std::lock_guard<std::mutex> lock_events(client->target_->events_lock);
    auto& events = client->target_->events;
    auto event = events.end();
    while (event != events.begin() && !client->has_event(*(event-1)))
      --event;
    for (; event != events.end(); ++event) {
      if (!client->Send(*event)) {
        client->Close();
        break;
      }
    }
  }
}

// TODO: consider just doing an eager backfill
void Stalker::Backfill() {
  auto today = date::floor<date::days>(std::chrono::system_clock::now());

  for (const auto& tgt : targets_) {
    std::lock_guard<std::mutex> lock_events(tgt->events_lock);

    LogIndex* index = indices_->index(tgt->name);
    std::lock_guard<std::mutex> lock(*index->lock());

    for (auto backfill_day = today - kBackfillDays; backfill_day <= today; backfill_day += date::days{1}) {
      YMD ymd{backfill_day};
      if (!index->Lookup(ymd))
        continue;

      auto reader = index->Open(ymd.year, ymd.month, ymd.day);
      std::uint64_t line = 0;
      while (true) {
        LogEvent& event = tgt->events.emplace_back();

        if (!reader->Read(&event)) {
          tgt->events.pop_back();
          break;
        }

        LogEventId* event_id = event.mutable_event_id();
        event_id->set_day(backfill_day.time_since_epoch().count());
        event_id->set_line(line);
        ++line;

        if (tgt->events.size() > kQueueSize)
          tgt->events.pop_front();
      }

      if (line > 0) {
        tgt->last_day = backfill_day.time_since_epoch().count();
        tgt->last_line = line - 1;
      }
    }
  }
}

Stalker::Target* Stalker::target(const std::string& name) {
  for (auto& t : targets_)
    if (t->name == name)
      return t.get();
  return nullptr;
}

} // namespace esologs
