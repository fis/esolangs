#include "esobot/bridge.h"
#include "esobot/config.pb.h"
#include "event/loop.h"
#include <utility>

namespace esobot {

Bridge::Bridge(event::Loop* loop, const BridgeConfig& config) : loop_(loop) {
  for (const auto& route : config.route())
    routes_.emplace_back(route);
}

std::unique_ptr<irc::bot::Plugin> Bridge::AddHost(const BridgePlugin& config, irc::bot::PluginHost* host) {
  hosts_.try_emplace(config.net(), host);
  return std::make_unique<Plugin>(this, host, config.net());
}

Bridge::Plugin::~Plugin() {
  bridge->hosts_.erase(net);
}

namespace {

constexpr char kHelpText[] = "brctl: usage: \"brctl: ignored\" (to list), \"brctl: ignore [net/]nick\" (to add) or \"brctl: unignore [net/]nick\" (to remove); network defaults to your own; nick = * matches any message";
constexpr char kErrorText[] = "brctl: invalid [net/]nick specification";
constexpr char kIgnoredText[] = "brctl: ignoring";
constexpr char kNotIgnoredText[] = "brctl: already ignored";
constexpr char kUnignoredText[] = "brctl: unignoring";
constexpr char kNotUnignoredText[] = "brctl: not ignored in the first place";

constexpr std::size_t kMaxNickLen = 32;

} // unnamed namespace

void Bridge::Plugin::MessageReceived(const irc::Message& msg) {
  if (!msg.command_is("PRIVMSG") || msg.nargs() != 2 || msg.prefix_nick().empty())
    return;
  std::string_view chan = msg.arg(0);
  std::string_view body = msg.arg(1);

  if (body.rfind("brctl: ", 0) == 0) {
    auto reply_target = msg.reply_target();
    if (reply_target.empty())
      return;
    if (body == "brctl: ignored") {
      std::string out{"brctl: Ignore list:"};
      for (const auto& ignored : bridge->ignore_) {
        out.push_back(' ');
        out.append(ignored.first);
        out.push_back('/');
        out.append(ignored.second);
      }
      host->Send({"PRIVMSG", reply_target, out});
    } else if (body.rfind("brctl: ignore ", 0) == 0) {
      std::string_view ignore_net, ignore_nick;
      if (!ParseSpec(body.substr(14), &ignore_net, &ignore_nick)) {
        host->Send({"PRIVMSG", reply_target, kErrorText});
        return;
      }
      if (bridge->ignore_.emplace(ignore_net, ignore_nick).second)
        host->Send({"PRIVMSG", reply_target, kIgnoredText});
      else
        host->Send({"PRIVMSG", reply_target, kNotIgnoredText});
    } else if (body.rfind("brctl: unignore ", 0) == 0) {
      std::string_view ignore_net, ignore_nick;
      if (!ParseSpec(body.substr(16), &ignore_net, &ignore_nick)) {
        host->Send({"PRIVMSG", reply_target, kErrorText});
        return;
      }
      std::pair<std::string, std::string> ignored{ignore_net, ignore_nick};
      if (bridge->ignore_.erase(ignored) > 0)
        host->Send({"PRIVMSG", reply_target, kUnignoredText});
      else
        host->Send({"PRIVMSG", reply_target, kNotUnignoredText});
    } else {
      host->Send({"PRIVMSG", reply_target, kHelpText});
    }
    return;
  }

  {
    std::pair<std::string, std::string> source{net, msg.prefix_nick()};
    if (bridge->ignore_.count(source) > 0)
      return;
    source.second = "*";
    if (bridge->ignore_.count(source) > 0)
      return;
  }

  for (const auto& route : bridge->routes_) {
    if (route.from_net() != net || route.from_chan() != chan)
      continue;

    auto host = bridge->hosts_.find(route.to_net());
    if (host == bridge->hosts_.end())
      continue;

    std::string out;
    out.push_back('<');
    out.append(msg.prefix_nick());
    out.push_back('>');
    out.push_back(' ');
    out.append(body);

    host->second->Send({ "PRIVMSG", route.to_chan(), out });
  }
}

bool Bridge::Plugin::ParseSpec(std::string_view spec, std::string_view *ignore_net, std::string_view *ignore_nick) {
  std::size_t sep = spec.find('/');

  if (sep != spec.npos) {
    *ignore_net = spec.substr(0, sep);
    if (bridge->hosts_.count(std::string(*ignore_net)) == 0)
      return false;
    spec.remove_prefix(sep + 1);
  } else
    *ignore_net = net;

  if (spec.size() > kMaxNickLen || spec.find(' ') != spec.npos)
    return false;

  *ignore_nick = spec;
  return true;
}

} // namespace esobot
