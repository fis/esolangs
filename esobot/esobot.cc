#include "base/log.h"
#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "irc/bot/bot.h"
#include "proto/util.h"

namespace esobot {

class EsoBot : public irc::bot::ConfiguredBot<Config> {
 public:
  explicit EsoBot(const char* config_file);

 private:
  const char* config_file_;
};

EsoBot::EsoBot(const char* config_file) : ConfiguredBot(config_file) {
  RegisterPlugin<LoggerConfig, Logger>();
  RegisterPlugin<RcFeedConfig, RcFeed>();
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
