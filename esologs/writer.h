#ifndef ESOLOGS_WRITER_H_
#define ESOLOGS_WRITER_H_

#include <chrono>
#include <cstdint>
#include <string>

#include <date/date.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <prometheus/registry.h>

#include "base/buffer.h"
#include "base/common.h"
#include "esologs/config.pb.h"
#include "esologs/log.pb.h"
#include "event/loop.h"
#include "event/socket.h"
#include "proto/delim.h"

namespace esologs {

class PipeServer;

/**
 * IRC log writer.
 *
 * Handles appending log events to a set of daily logfiles representing a single channel.
 * An IRC bot is expected to create one of these for every target it's interested in logging.
 */
class Writer {
 public:
  Writer(const Config& config, const std::string& target_name, event::Loop* loop, prometheus::Registry* metric_registry = nullptr);
  ~Writer();

  /**
   * Writes an event to the relevant logfile, and optionally a pipe too.
   *
   * Note that if tee'ing the event to a pipe is requested by passing in a non-null pipe server, the
   * event will be mutated to contain the correct event ID that the event received when it was
   * written to the logfile.
   */
  void Write(LogEvent* event, PipeServer* pipe = nullptr);

  DISALLOW_COPY(Writer);

 private:
  event::Loop* const loop_;

  std::string target_;
  std::string dir_;

  date::sys_days current_day_;
  std::unique_ptr<proto::DelimWriter> current_log_;
  std::uint64_t current_line_;  // index of next line to be written

  prometheus::Gauge* metric_last_written_;

  std::pair<date::sys_days, std::uint64_t> Now() {
    auto now = std::chrono::system_clock::now();
    auto day = date::floor<date::days>(now);
    auto time = date::floor<std::chrono::microseconds>(now - day);
    return std::pair(day, time.count());
  }

  void OpenLog(date::sys_days day);
};

/**
 * Socket server for a real-time tee of log events.
 *
 * An IRC bot is expected to create exactly one of these, which the web frontend will then connect to.
 * Lines written to any target's logs should be passed to this class, though they will have to
 * go through a Writer in order to receive their valid immutable event ID.
 */
class PipeServer : public event::ServerSocket::Watcher, public event::Socket::Watcher {
 public:
  PipeServer(event::Loop* loop, const std::string& path);

  void Write(LogEvent* event);

  // event::ServerSocket::Watcher
  void Accepted(std::unique_ptr<event::Socket> socket) override;
  void AcceptError(std::unique_ptr<base::error> error) override;
  // event::Socket::Watcher
  void CanRead() override;
  void CanWrite() override;
  void ConnectionOpen() override {}
  void ConnectionFailed(std::unique_ptr<base::error> error) override {}

 private:
  void ResetReader();

  std::unique_ptr<event::ServerSocket> server_;
  std::unique_ptr<event::Socket> reader_;

  static constexpr std::size_t kWriteBufferSize = 65536;
  base::ring_buffer write_buffer_{kWriteBufferSize};
  std::deque<std::size_t> write_sizes_;
};

} // namespace esologs

#endif // ESOLOGS_WRITER_H_

// Local Variables:
// mode: c++
// End:
