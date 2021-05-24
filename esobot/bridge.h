#ifndef ESOBOT_BRIDGE_H_
#define ESOBOT_BRIDGE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "re2/re2.h"

#include "esobot/config.pb.h"
#include "event/loop.h"
#include "irc/bot/module.h"

namespace esobot {

class Bridge : public irc::bot::Module {
  using Connection = irc::bot::Connection;

 public:
  Bridge(const BridgeConfig& config, irc::bot::ModuleHost* host);

  void MessageReceived(Connection* conn, const irc::Message& msg) override;

 private:
  bool ParseSpec(Connection* conn, std::string_view spec, std::string_view *net, std::string_view *nick);

  // TODO: look for opportunities to add std::less<> to string-keyed maps
  irc::bot::ModuleHost* host_;
  std::vector<RouteConfig> routes_;
  std::set<std::pair<std::string, std::string>> ignores_;
  std::map<std::string, std::unique_ptr<RE2>> filters_;
};

} // namespace esobot

#endif // ESOBOT_BRIDGE_H_

// Local Variables:
// mode: c++
// End:
