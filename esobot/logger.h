#ifndef ESOBOT_LOGGER_H_
#define ESOBOT_LOGGER_H_

#include <filesystem>

#include "esobot/config.pb.h"
#include "esologs/writer.h"
#include "irc/bot/module.h"

namespace esobot {

class Logger : public irc::bot::Module {
  using Connection = irc::bot::Connection;

 public:
  Logger(const LoggerConfig& config, irc::bot::ModuleHost* host);
  void MessageReceived(Connection* conn, const irc::Message& msg) override { Log(conn, msg, /* sent: */ false); }
  void MessageSent(Connection* conn, const irc::Message& msg) override { Log(conn, msg, /* sent: */ true); }

 private:
  struct Target {
    const std::string net;
    const std::string chan;
    esologs::Writer log;
    Target(const LoggerTarget& target, const esologs::Config& log_config, irc::bot::ModuleHost* host);
  };

  void Log(Connection* conn, const irc::Message& msg, bool sent);
  esologs::FileWriter* RawFile(const std::string& net);
  void CloseRawFile(const std::string& net, std::uint64_t time);
  std::filesystem::path RawFilePath(const std::string& net);

  std::vector<std::unique_ptr<Target>> targets_;
  std::unique_ptr<esologs::PipeServer> pipe_;

  std::string raw_path_;
  std::unique_ptr<std::unordered_map<std::string, esologs::FileWriter>> raw_files_;
};

} // namespace esobot

#endif // ESOBOT_LOGGER_H_

// Local Variables:
// mode: c++
// End:
