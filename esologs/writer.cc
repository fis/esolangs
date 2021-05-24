#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <experimental/filesystem>
#include <string>

#include <prometheus/gauge.h>

#include "base/exc.h"
#include "esologs/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "event/loop.h"
#include "event/socket.h"
#include "proto/util.h"

namespace esologs {

namespace fs = std::experimental::filesystem;

Writer::Writer(const Config& config, const std::string& target_name, event::Loop* loop, prometheus::Registry* metric_registry)
    : loop_(loop)
{
  const TargetConfig* target = nullptr;

  for (const auto& t : config.target()) {
    if (t.name() == target_name) {
      target = &t;
      break;
    }
  }
  if (!target)
    throw base::Exception("log target name not found in config");

  dir_ = target->log_path();
  target_ = target->name();

  current_day_ = Now().first;
  OpenLog(current_day_);

  if (metric_registry) {
    metric_last_written_ = &prometheus::BuildGauge()
        .Name("esologs_writer_last_message_time")
        .Help("When was the last log message written?")
        .Register(*metric_registry)
        .Add({{"target", target_name}});
  }
}

Writer::~Writer() {}

void Writer::Write(LogEvent* event, PipeServer* pipe) {
  auto [day, time_us] = Now();

  if (day != current_day_) {
    current_day_ = day;
    OpenLog(day);
  }

  event->set_time_us(time_us);
  current_log_->Write(*event);
  ++current_line_;

  if (pipe) {
    LogEventId* event_id = event->mutable_event_id();
    event_id->set_target(target_);
    event_id->set_day(day.time_since_epoch().count());
    event_id->set_line(current_line_ - 1);
    pipe->Write(event);
  }

  if (metric_last_written_)
    metric_last_written_->SetToCurrentTime();
};

void Writer::OpenLog(date::sys_days day) {
  if (current_log_)
    current_log_.reset();

  auto ymd = date::year_month_day{day};

  char buf[16];

  fs::path log_file = dir_;

  std::snprintf(buf, sizeof buf, "%d", (int)ymd.year());
  log_file /= buf;
  std::snprintf(buf, sizeof buf, "%u", (unsigned)ymd.month());
  log_file /= buf;

  fs::create_directories(log_file);

  std::snprintf(buf, sizeof buf, "%u.pb", (unsigned)ymd.day());
  log_file /= buf;

  current_line_ = 0;
  if (fs::exists(log_file)) {
    proto::DelimReader reader(log_file.c_str());
    while (reader.Skip())
      ++current_line_;
  }

  current_log_ = std::make_unique<proto::DelimWriter>(log_file.c_str());
}

PipeServer::PipeServer(event::Loop* loop, const std::string& path) {
  auto server = event::ListenUnix(loop, this, path, event::Socket::SEQPACKET);
  if (!server.ok())
    throw base::Exception(*server.error());
  server_ = server.ptr();
  LOG(INFO) << "pipeserver: listening at: " << path;
}

void PipeServer::Write(LogEvent* event) {
  if (!reader_)
    return;

  bool start_write = write_buffer_.empty();

  std::size_t event_size = event->ByteSizeLong();
  if (write_buffer_.size() + event_size > kWriteBufferSize) {
    LOG(WARNING) << "pipeserver: send queue full, killing the client";
    ResetReader();
    return;
  }

  {
    proto::RingBufferOutputStream stream(base::borrow(&write_buffer_));
    google::protobuf::io::CodedOutputStream coded(&stream);
    event->SerializeWithCachedSizes(&coded);
  }
  write_sizes_.push_back(event_size);

  if (start_write)
    reader_->WantWrite(true);
}

void PipeServer::Accepted(std::unique_ptr<event::Socket> socket) {
  ResetReader();
  reader_ = std::move(socket);
  reader_->SetWatcher(this);
  reader_->WantRead(true);
  reader_->WantWrite(false);
  LOG(INFO) << "pipeserver: accepted connection";
}

void PipeServer::AcceptError(std::unique_ptr<base::error> error) {
  LOG(WARNING) << "pipeserver: accept failed: " << *error;
}

void PipeServer::CanRead() {
  // A pipe reader is never expected to write anything back.
  // If the socket is ready to read, it means something must have gone wrong.

  LOG(WARNING) << "pipeserver: connection broken (unexpected input)";
  ResetReader();
}

void PipeServer::CanWrite() {
  std::size_t len = write_sizes_.front();
  write_sizes_.pop_front();

  auto chunks = write_buffer_.front(len);
  base::io_result wrote = base::io_result::ok(0);
  if (chunks.second.valid()) {
    // TODO: rethink this to avoid the rare copy when straddling the boundary
    char copy[len];
    std::memcpy(copy, chunks.first.data(), chunks.first.size());
    std::memcpy(copy + chunks.first.size(), chunks.second.data(), chunks.second.size());
    wrote = reader_->Write(copy, len);
  } else {
    wrote = reader_->Write(chunks.first.data(), chunks.first.size());
  }

  if (wrote.failed()) {
    LOG(WARNING) << "pipeserver: write failed: " << *wrote.error();
    ResetReader();
    return;
  }

  if (wrote.size() < len)
    LOG(WARNING) << "pipeserver: write truncated: " << wrote.size() << " < " << len;

  write_buffer_.pop(len);
  if (write_buffer_.empty())
    reader_->WantWrite(false);
}

void PipeServer::ResetReader() {
  write_buffer_.clear();
  write_sizes_.clear();
  reader_.reset();
}

#if 0 // TODO REMOVE
void Writer::Stalker::ConnectionOpen() {
  LOG(INFO) << "stalker connected: " << socket_path_;

  CHECK(state_ == kConnecting);
  state_ = kConnected;
  socket_->WantRead(true);  // monitor readability for detecting disconnect early
}

void Writer::Stalker::ConnectionFailed(std::unique_ptr<base::error> error) {
  LOG(WARNING) << "stalker connect failed: " << socket_path_ << ": " << *error;

  CHECK(state_ == kConnecting);
  state_ = kWaiting;
  socket_.reset();
  loop_->Delay(kReconnectDelay, base::borrow(this));
}

void Writer::Stalker::CanRead() {
  // The stalker server is never expected to write anything back.
  // The socket being ready to read must mean something went wrong.

  LOG(WARNING) << "stalker connection broken";
  state_ = kWaiting;
  socket_.reset();
  loop_->Delay(kReconnectDelay, base::borrow(this));
}

void Writer::Stalker::Write(const LogEvent& event) {
  if (state_ != kConnected)
    return;  // not critical, just drop

  CHECK(socket_);

  if (!event.SerializeToString(&write_buffer_)) {
    LOG(WARNING) << "stalker event serialization failed";
    return;
  }

  auto wrote = socket_->Write(write_buffer_.data(), write_buffer_.length());

  if (wrote.failed()) {
    LOG(WARNING) << "stalker write failed: " << socket_path_ << ": " << *wrote.error();
    state_ = kWaiting;
    socket_.reset();
    loop_->Delay(kReconnectDelay, base::borrow(this));
    return;
  }

  if (wrote.size() < write_buffer_.length()) {
    LOG(WARNING) << "stalker write truncated: " << wrote.size() << " < " << write_buffer_.length();
    return;
  }
}

void Writer::Stalker::Connect() {
  CHECK(state_ == kWaiting);
  CHECK(!socket_);

  state_ = kConnecting;
  auto maybe_socket = event::Socket::Builder()
      .loop(loop_)
      .watcher(this)
      .unix(socket_path_)
      .kind(event::Socket::SEQPACKET)
      .Build();
  if (maybe_socket.ok()) {
    socket_ = maybe_socket.ptr();
    socket_->Start();
  } else {
    ConnectionFailed(maybe_socket.error());
  }
}
#endif

} // namespace esologs
