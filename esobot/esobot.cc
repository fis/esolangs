#include "base/log.h"
#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "event/loop.h"
#include "irc/config.pb.h"
#include "irc/connection.h"
#include "irc/message.h"
#include "proto/util.h"

namespace esobot {

class EsoBot : public irc::Connection::Reader, public RcFeedListener {
 public:
  EsoBot(const char* config_file);
  void Run();

  void MessageReceived(const irc::Message& msg) override;
  void RecentChange(const char* msg) override;

 private:
  std::string channel_;

  event::Loop loop_;
  std::unique_ptr<Logger> log_;
  std::unique_ptr<irc::Connection> irc_;
  std::unique_ptr<RcFeedReader> rc_feed_reader_;
};

EsoBot::EsoBot(const char* config_file) {
  Config config;
  proto::ReadText(config_file, &config);

  // TODO: deal with multiple channels
  CHECK(config.irc().channels_size() == 1);
  channel_ = config.irc().channels(0);

  log_ = std::make_unique<Logger>(channel_, config.log_path());

  irc_ = std::make_unique<irc::Connection>(config.irc(), &loop_);
  irc_->AddReader(this);

  rc_feed_reader_ = std::make_unique<RcFeedReader>(&loop_, this);
}

void EsoBot::Run() {
  irc_->Start();
  while (true)
    loop_.Poll();
}

void EsoBot::MessageReceived(const irc::Message& msg) {
  log_->Log(msg);

  std::string debug(msg.command());
  for (const std::string& arg : msg.args()) {
    debug += ' ';
    debug += arg;
  }
  LOG(DEBUG) << debug;
}

void EsoBot::RecentChange(const char* msg) {
  LOG(INFO) << "Wiki message: " << msg;
  log_->Send(irc_.get(), { "PRIVMSG", channel_.c_str(), msg });
}

} // namespace esobot

int main(int argc, char *argv[]) {
  if (argc != 2) {
    LOG(ERROR) << "usage: " << argv[0] << " <esobot.config>";
    return 1;
  }

  esobot::EsoBot bot(argv[1]);
  bot.Run();
  return 0;
}
