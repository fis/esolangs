#include "base/log.h"
#include "esobot/bridge.h"
#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "irc/bot/bot.h"
#include "proto/util.h"
#include <memory>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    LOG(ERROR) << "usage: " << argv[0] << " <bot.config>";
    return 1;
  }

  event::Loop loop;

  esobot::Config config;
  proto::ReadText(argv[1], &config);

  std::unique_ptr<esobot::RcFeed> rc_feed;
  if (config.has_rc_feed())
    rc_feed = std::make_unique<esobot::RcFeed>(&loop, config.rc_feed());
  std::unique_ptr<esobot::Bridge> bridge;
  if (config.has_bridge())
    bridge = std::make_unique<esobot::Bridge>(&loop, config.bridge());

  std::vector<std::unique_ptr<irc::bot::Bot>> bots;
  for (const esobot::BotConfig& bot_config : config.bot()) {
    auto bot = std::make_unique<irc::bot::Bot>(&loop);
    bot->RegisterPlugin<esobot::LoggerPlugin, esobot::Logger>();
    if (rc_feed)
      bot->RegisterPlugin<esobot::RcFeedPlugin>([&rc_feed](const esobot::RcFeedPlugin& config, irc::bot::PluginHost* host) {
        return rc_feed->AddHost(config, host);
      });
    if (bridge)
      bot->RegisterPlugin<esobot::BridgePlugin>([&bridge](const esobot::BridgePlugin& config, irc::bot::PluginHost* host) {
        return bridge->AddHost(config, host);
      });
    bot->Start(bot_config);
    bots.emplace_back(std::move(bot));
  }

  loop.Run();
  return 0;
}
