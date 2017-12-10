#include "base/log.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "event/loop.h"
#include "irc/config.pb.h"
#include "irc/connection.h"
#include "irc/message.h"

namespace esobot {

constexpr char kNick[] = "esowiki";
constexpr char kChannel[] = "#esoteric";
#if 0
constexpr char kIrcServer[] = "chat.freenode.net";
constexpr char kIrcPort[] = "6697";
constexpr char kLogRoot[] = "/home/esowiki/logs";
#else
constexpr char kIrcServer[] = "localhost";
constexpr char kIrcPort[] = "6697";
constexpr char kLogRoot[] = "/home/fis/src/esowiki/tmp";
#endif

class EsoBot : public irc::Connection::Reader, public RcFeedListener {
 public:
  EsoBot();
  void Run();

  void MessageReceived(const irc::Message& msg) override;
  void RecentChange(const char* msg) override;

 private:
  event::Loop loop_;
  Logger log_;
  std::unique_ptr<irc::Connection> irc_;
  std::unique_ptr<RcFeedReader> rc_feed_reader_;
};

EsoBot::EsoBot() : log_(kLogRoot, kChannel) {
  irc::Config config;
  config.set_nick(kNick);
  config.set_user(kNick);
  config.set_realname(kNick);
  irc::Config::Server* server = config.add_servers();
  server->set_host(kIrcServer);
  server->set_port(kIrcPort);
  server->mutable_tls();
  config.add_channels(kChannel);

  irc_ = std::make_unique<irc::Connection>(config, &loop_);
  irc_->AddReader(this);

  rc_feed_reader_ = std::make_unique<RcFeedReader>(&loop_, this);
}

void EsoBot::Run() {
  irc_->Start();
  while (true)
    loop_.Poll();
}

void EsoBot::MessageReceived(const irc::Message& msg) {
  log_.Log(msg);

  std::string debug(msg.command());
  for (const std::string& arg : msg.args()) {
    debug += ' ';
    debug += arg;
  }
  LOG(DEBUG) << debug;
}

void EsoBot::RecentChange(const char* msg) {
  LOG(INFO) << "Wiki message: " << msg;
  log_.Send(irc_.get(), { "PRIVMSG", kChannel, msg });
}

} // namespace esobot

int main(int argc, char *argv[]) {
  esobot::EsoBot bot;
  bot.Run();
  return 0;
}
