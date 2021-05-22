#ifndef ESOBOT_BRIDGE_H_
#define ESOBOT_BRIDGE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "re2/re2.h"

#include "esobot/config.pb.h"
#include "event/loop.h"
#include "irc/bot/plugin.h"

namespace esobot {

class Bridge {
 public:
  Bridge(event::Loop* loop, const BridgeConfig& config);

  std::unique_ptr<irc::bot::Plugin> AddHost(const BridgePlugin& config, irc::bot::PluginHost* host);

 private:
  struct Plugin : public irc::bot::Plugin {
    Plugin(Bridge* b, irc::bot::PluginHost* h, const std::string& n) : bridge(b), host(h), net(n) {}
    ~Plugin();

    void MessageReceived(const irc::Message& msg) override;
    bool ParseSpec(std::string_view spec, std::string_view *net, std::string_view *nick);

    Bridge* bridge;
    irc::bot::PluginHost* host;
    std::string net;
  };

  event::Loop* loop_;
  std::vector<RouteConfig> routes_;
  std::unordered_map<std::string, irc::bot::PluginHost*> hosts_;
  std::set<std::pair<std::string, std::string>> ignore_;
  std::map<std::string, std::unique_ptr<RE2>> filter_;
};

} // namespace esobot

#endif // ESOBOT_BRIDGE_H_

// Local Variables:
// mode: c++
// End:
