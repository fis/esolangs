#include "base/log.h"
#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "irc/bot/bot.h"
#include "proto/util.h"

int main(int argc, char *argv[]) {
  irc::bot::Bot bot;
  bot.RegisterPlugin<esobot::LoggerConfig, esobot::Logger>();
  bot.RegisterPlugin<esobot::RcFeedConfig, esobot::RcFeed>();
  return bot.Main<esobot::Config>(argc, argv);
}
