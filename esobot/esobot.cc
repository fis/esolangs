#include "base/log.h"
#include "esobot/bridge.h"
#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "irc/bot/bot.h"
#include "irc/bot/remote.h"
#include "proto/util.h"
#include <memory>

int main(int argc, char *argv[]) {
  irc::bot::Bot bot;
  irc::bot::RegisterRemoteModule(&bot);
  bot.RegisterModule<esobot::BridgeConfig, esobot::Bridge>();
  bot.RegisterModule<esobot::LoggerConfig, esobot::Logger>();
  bot.RegisterModule<esobot::RcFeedConfig, esobot::RcFeed>();
  return bot.Main<esobot::Config>(argc, argv);
}
