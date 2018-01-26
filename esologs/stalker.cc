#include <date/date.h>

#include "esologs/stalker.h"

namespace esologs {

Stalker::Stalker(const Config& config, event::Loop* loop, LogIndex* index) : loop_(loop), index_(index) {
  server_ = event::ListenUnix(loop, this, config.stalker_socket(), event::Socket::SEQPACKET);
  LOG(INFO) << "stalker server listening at: " << config.stalker_socket();
}

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
      date::sys_days date{date::days{day}};
      YMD ymd{date};
      fmt->FormatDay(true, ymd.year, ymd.month, ymd.day);

      if (event_line > 0)
        fmt->FormatElision();
    }

    fmt->FormatEvent(event);
  }

  fmt->FormatStalkerFooter();
}

void Stalker::Accepted(std::unique_ptr<event::Socket> socket) {
  {
    std::lock_guard<std::mutex> lock(events_lock_);
    if (events_.empty())
      Backfill();
  }

  writer_ = std::move(socket);
  writer_->SetWatcher(this);
  writer_->StartRead();
  LOG(INFO) << "stalker client connected";
}

void Stalker::CanRead() {
  CHECK(writer_);

  // TODO: handle read errors (and EOF) without bringing down the logs server

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

    // TODO: signal to active stalkers
  }
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
