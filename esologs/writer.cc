#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <experimental/filesystem>
#include <string>

#include "esologs/config.pb.h"
#include "esologs/log.pb.h"
#include "esologs/writer.h"
#include "event/loop.h"
#include "event/socket.h"
#include "proto/util.h"

namespace esologs {

namespace fs = std::experimental::filesystem;

class Writer::Stalker : public event::Socket::Watcher, public event::Timed {
 public:
  Stalker(const std::string& socket_path, event::Loop* loop)
      : socket_path_(socket_path), loop_(loop), state_(kWaiting)
  { Connect(); }

  void ConnectionOpen() override;
  void ConnectionFailed(const std::string& error) override;
  void CanRead() override;
  void CanWrite() override {}

  void TimerExpired(bool) override { Connect(); }

  void Write(const LogEvent& event);

 private:
  static constexpr auto kReconnectDelay = std::chrono::seconds(30);

  enum State {
    kConnecting,
    kConnected,
    kWaiting,
  };

  const std::string socket_path_;
  event::Loop* loop_;

  State state_;
  std::unique_ptr<event::Socket> socket_;
  std::string write_buffer_;

  void Connect();
};

Writer::Writer(const std::string& config_path, event::Loop* loop) : loop_(loop) {
  Config config;
  proto::ReadText(config_path, &config);

  dir_ = config.log_path();

  current_day_ = Now().first;
  OpenLog(current_day_);

  if (!config.stalker_socket().empty())
    stalker_ = std::make_unique<Stalker>(config.stalker_socket(), loop);
}

Writer::~Writer() {}

void Writer::Write(LogEvent* event) {
  auto [day, time_us] = Now();

  if (day != current_day_) {
    current_day_ = day;
    OpenLog(day);
  }

  event->set_time_us(time_us);
  current_log_->Write(*event);
  ++current_line_;

  if (stalker_) {
    LogEventId* event_id = event->mutable_event_id();
    event_id->set_day(day.time_since_epoch().count());
    event_id->set_line(current_line_ - 1);
    stalker_->Write(*event);
  }
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
    LogEvent event; // TODO: support 'skip' in DelimReader
    proto::DelimReader reader(log_file.c_str());
    while (reader.Read(&event))
      ++current_line_;
  }

  current_log_ = std::make_unique<proto::DelimWriter>(log_file.c_str());
}

void Writer::Stalker::ConnectionOpen() {
  LOG(INFO) << "stalker connected: " << socket_path_;

  CHECK(state_ == kConnecting);
  state_ = kConnected;
  socket_->StartRead();  // monitor readability for detecting disconnect early
}

void Writer::Stalker::ConnectionFailed(const std::string& error) {
  LOG(WARNING) << "stalker connect failed: " << socket_path_ << ": " << error;

  CHECK(state_ == kConnecting);
  state_ = kWaiting;
  socket_.reset();
  loop_->Delay(kReconnectDelay, this);
}

void Writer::Stalker::CanRead() {
  // The stalker server is never expected to write anything back.
  // The socket being ready to read must mean something went wrong.

  LOG(WARNING) << "stalker connection broken";
  state_ = kWaiting;
  socket_.reset();
  loop_->Delay(kReconnectDelay, this);
}

void Writer::Stalker::Write(const LogEvent& event) {
  if (state_ != kConnected)
    return;  // not critical, just drop

  CHECK(socket_);

  try {
    if (event.SerializeToString(&write_buffer_)) {
      auto wrote = socket_->Write(write_buffer_.data(), write_buffer_.length());
      if (wrote < write_buffer_.length())
        LOG(WARNING) << "stalker write truncated: " << wrote << " < " << write_buffer_.length();
    } else {
      LOG(WARNING) << "stalker event serialization failed";
    }
  } catch (const event::Socket::Exception& err) {
    LOG(WARNING) << "stalker write failed: " << socket_path_ << ": " << err.what();
    state_ = kWaiting;
    socket_.reset();
    loop_->Delay(kReconnectDelay, this);
  }
}

void Writer::Stalker::Connect() {
  CHECK(state_ == kWaiting);
  CHECK(!socket_);

  state_ = kConnecting;
  socket_ = event::Socket::Builder()
      .loop(loop_)
      .watcher(this)
      .unix(socket_path_)
      .kind(event::Socket::SEQPACKET)
      .Build();
  socket_->Start();
}

} // namespace esologs
