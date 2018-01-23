#include "base/log.h"
#include "esobot/config.pb.h"
#include "irc/bot/bot.h"
#include "proto/util.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    LOG(ERROR) << "usage: " << argv[0] << " <esobot.config>";
    return 1;
  }

  char *config_file = argv[1];
  irc::bot::Bot bot(
      [&config_file]() {
        auto config = std::make_unique<esobot::Config>();
        proto::ReadText(config_file, config.get());
        return config;
      });
  bot.Run();
  return 0;
}
