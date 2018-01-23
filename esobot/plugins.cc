#include "esobot/config.pb.h"
#include "esobot/logger.h"
#include "esobot/rcfeed.h"
#include "irc/bot/plugin.h"

namespace esobot {

IRC_BOT_MODULE(
    LoggerConfig, Logger,
    RcFeedConfig, RcFeed);

} // namespace esobot
