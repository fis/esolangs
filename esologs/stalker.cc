#include <date/date.h>

#include "base/buffer.h"
#include "esologs/stalker.h"
#include "web/server.h"

namespace esologs {

class Stalker::Client : public web::WebsocketClientHandler {
 public:
  Client(Stalker* stalker) : stalker_(stalker) {}

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
  web::Websocket* socket_ = nullptr;

  std::int64_t sent_day_ = 0;
  std::uint64_t sent_line_ = 0;

  std::string buffer_;
  std::unique_ptr<LogFormatter> fmt_ = LogFormatter::CreateHTML(&buffer_);
};

bool Stalker::Client::Send(const LogEvent& event) {
  const LogEventId& id = event.event_id();
  std::int64_t day = id.day();
  std::uint64_t line = id.line();

  std::optional<std::size_t> wrote;

  {
    base::byte_array<8> buf;
    base::write_i32(day, buf, 0);
    base::write_u32(day, buf, 4);
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
  fmt_->FormatEvent(event);

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

  auto msg_day = base::read_i32(buf, 0);
  auto msg_line = base::read_u32(buf, 4);

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

Stalker::Stalker(const Config& config, event::Loop* loop, LogIndex* index, prometheus::Registry* metric_registry)
    : loop_(loop), index_(index) {
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

  server_ = event::ListenUnix(loop, this, config.stalker_socket(), event::Socket::SEQPACKET);
  LOG(INFO) << "stalker server listening at: " << config.stalker_socket();
}

Stalker::~Stalker() {}

void Stalker::Format(LogFormatter* fmt) {
  std::int64_t day = 0;

  int link_year;
  {
    std::lock_guard<std::mutex> lock(*index_->lock());
    link_year = index_->default_year();
  }
  fmt->FormatStalkerHeader(link_year);

  std::lock_guard<std::mutex> lock(events_lock_);
  for (const LogEvent& event : events_) {
    std::int64_t event_day = event.event_id().day();
    std::uint64_t event_line = event.event_id().line();

    if (event_day > day) {
      day = event_day;
      YMD ymd(YMD::day_number, day);
      fmt->FormatDay(true, ymd.year, ymd.month, ymd.day);

      if (event_line > 0)
        fmt->FormatElision();
    }

    fmt->FormatEvent(event);
  }

  fmt->FormatStalkerFooter();
}

void Stalker::UpdateClients() {
  std::scoped_lock<std::mutex, std::mutex> lock(clients_lock_, events_lock_);

  for (Client* client : clients_) {
    if (!client->registered())
      continue;

    auto event = events_.end();
    while (event != events_.begin() && !client->has_event(*(event-1)))
      --event;
    for (; event != events_.end(); ++event) {
      if (!client->Send(*event)) {
        client->Close();
        break;
      }
    }
  }
}

web::WebsocketClientHandler* Stalker::AddClient() {
  std::lock_guard<std::mutex> lock(clients_lock_);
  auto* handler = clients_.emplace(this);
  if (metric_clients_)
    metric_clients_->Set(clients_.size());
  return handler;
}

void Stalker::Accepted(std::unique_ptr<event::Socket> socket) {
  {
    std::lock_guard<std::mutex> lock(events_lock_);
    if (events_.empty())
      Backfill();
    events_loaded_ = true;
  }

  writer_ = std::move(socket);
  writer_->SetWatcher(this);
  writer_->StartRead();
  LOG(INFO) << "stalker client connected";
}

void Stalker::CanRead() {
  CHECK(writer_);

  std::size_t size;
  try {
    size = writer_->Read(read_buffer_.data(), read_buffer_.size());
    if (size == 0)
      return;
  } catch (const event::Socket::Exception& err) {
    LOG(WARNING) << "stalker event read failed: " << err.what();
    writer_.reset();
    return;
  }

  writer_->StartRead();

  {
    std::lock_guard<std::mutex> lock(events_lock_);

    LogEvent& event = events_.emplace_back();
    if (!event.ParseFromArray(read_buffer_.data(), size)) {
      LOG(WARNING) << "stalker event parse error, len = " << size;
      events_.pop_back();
      return;
    }

    std::int64_t event_day = event.event_id().day();
    std::uint64_t event_line = event.event_id().line();
    if (event_day < last_day_ || (event_day == last_day_ && event_line <= last_line_)) {
      events_.pop_back();
      return;
    }
    last_day_ = event_day;
    last_line_ = event_line;

    if (events_.size() > kQueueSize)
      events_.pop_front();
  }

  if (metric_last_received_)
    metric_last_received_->SetToCurrentTime();

  if (clients_active_)
    UpdateClients();
}

void Stalker::Backfill() {
  auto today = date::floor<date::days>(std::chrono::system_clock::now());

  std::lock_guard<std::mutex>(*index_->lock());
  for (auto backfill_day = today - kBackfillDays; backfill_day <= today; backfill_day += date::days{1}) {
    YMD ymd{backfill_day};
    if (!index_->Lookup(ymd))
      continue;

    auto reader = index_->Open(ymd.year, ymd.month, ymd.day);
    std::uint64_t line = 0;
    while (true) {
      LogEvent& event = events_.emplace_back();

      if (!reader->Read(&event)) {
        events_.pop_back();
        break;
      }

      LogEventId* event_id = event.mutable_event_id();
      event_id->set_day(backfill_day.time_since_epoch().count());
      event_id->set_line(line);
      ++line;

      if (events_.size() > kQueueSize)
        events_.pop_front();
    }

    if (line > 0) {
      last_day_ = backfill_day.time_since_epoch().count();
      last_line_ = line - 1;
    }
  }
}

} // namespace esologs
