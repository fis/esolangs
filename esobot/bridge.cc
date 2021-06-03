#include "esobot/bridge.h"
#include "esobot/config.pb.h"
#include "event/loop.h"
#include <memory>
#include <utility>

namespace esobot {

Bridge::Bridge(const BridgeConfig& config, irc::bot::ModuleHost* host) : host_(host) {
  for (const auto& route : config.routes())
    routes_.emplace_back(route);
  for (const auto& ignored : config.ignores())
    ignores_.emplace(ignored.net(), ignored.nick());
  for (const auto& filtered : config.filters()) {
    auto re = std::make_unique<RE2>(filtered);
    if (re->ok())
      filters_.try_emplace(filtered, std::move(re));
  }
}

namespace {

constexpr char kHelpText[] = "brctl: see \"brctl: help ignore\" (filter by nick) and \"brctl: help filter\" (filter by text content) for the two available commands";
constexpr char kIgnoreHelpText[] = "brctl: usage: \"brctl: ignored\" (to list; only in a query), \"brctl: ignore [net/]nick\" (to add) or \"brctl: unignore [net/]nick\" (to remove); network defaults to your own; nick = * matches any message";
constexpr char kFilterHelpText[] = "brctl: usage: \"brctl: filtered\" (to list), \"brctl: filter regex\" (to add) or \"brctl: unfilter regex\" (to remove)";

constexpr char kNickErrorText[] = "brctl: invalid [net/]nick specification";
constexpr char kIgnoredText[] = "brctl: ignoring";
constexpr char kNotIgnoredText[] = "brctl: already ignored";
constexpr char kUnignoredText[] = "brctl: unignoring";
constexpr char kNotUnignoredText[] = "brctl: not ignored in the first place";

constexpr char kRegexSpaceErrorText[] = "brctl: no spaces in regexen, it would be too confusing";
constexpr char kRegexParseErrorText[] = "brctl: bad expression: ";
constexpr char kFilteredText[] = "brctl: filtering";
constexpr char kNotFilteredText[] = "brctl: already filtered";
constexpr char kUnfilteredText[] = "brctl: unfiltering";
constexpr char kNotUnfilteredText[] = "brctl: not filtered in the first place";

constexpr std::size_t kMaxNickLen = 32;

} // unnamed namespace

void Bridge::MessageReceived(Connection* conn, const irc::Message& msg) {
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
      for (const auto& ignored : ignores_) {
        out.push_back(' ');
        out.append(ignored.first);
        out.push_back('/');
        out.append(ignored.second);
      }
      if (reply_target.size() > 0 && reply_target[0] == '#')
        out = "brctl: Try doing that in a query, otherwise it pings everyone.";
      conn->Send({"PRIVMSG", reply_target, out});
    } else if (body.rfind("brctl: ignore ", 0) == 0) {
      std::string_view ignore_net, ignore_nick;
      if (!ParseSpec(conn, body.substr(14), &ignore_net, &ignore_nick)) {
        conn->Send({"PRIVMSG", reply_target, kNickErrorText});
        return;
      }
      if (ignores_.emplace(ignore_net, ignore_nick).second)
        conn->Send({"PRIVMSG", reply_target, kIgnoredText});
      else
        conn->Send({"PRIVMSG", reply_target, kNotIgnoredText});
    } else if (body.rfind("brctl: unignore ", 0) == 0) {
      std::string_view ignore_net, ignore_nick;
      if (!ParseSpec(conn, body.substr(16), &ignore_net, &ignore_nick)) {
        conn->Send({"PRIVMSG", reply_target, kNickErrorText});
        return;
      }
      std::pair<std::string, std::string> ignored{ignore_net, ignore_nick};
      if (ignores_.erase(ignored) > 0)
        conn->Send({"PRIVMSG", reply_target, kUnignoredText});
      else
        conn->Send({"PRIVMSG", reply_target, kNotUnignoredText});
    } else if (body == "brctl: filtered") {
      std::string out{"brctl: Filter expressions:"};
      for (const auto& filtered : filters_) {
        out.push_back(' ');
        out.append(filtered.first);
      }
      conn->Send({"PRIVMSG", reply_target, out});
    } else if (body.rfind("brctl: filter ", 0) == 0) {
      std::string_view expr = body.substr(14);
      if (expr.find(' ') != expr.npos) {
        conn->Send({"PRIVMSG", reply_target, kRegexSpaceErrorText});
        return;
      }
      auto re = std::make_unique<RE2>(re2::StringPiece(expr.data(), expr.size())); // TODO: latest RE2 should support string_view, here and in match
      if (!re->ok()) {
        std::string out{kRegexParseErrorText};
        out.append(re->error());
        conn->Send({"PRIVMSG", reply_target, out});
        return;
      }
      if (filters_.try_emplace(std::string(expr), std::move(re)).second)
        conn->Send({"PRIVMSG", reply_target, kFilteredText});
      else
        conn->Send({"PRIVMSG", reply_target, kNotFilteredText});
    } else if (body.rfind("brctl: unfilter ", 0) == 0) {
      std::string_view expr = body.substr(16);
      if (filters_.erase(std::string(expr)) > 0)
        conn->Send({"PRIVMSG", reply_target, kUnfilteredText});
      else
        conn->Send({"PRIVMSG", reply_target, kNotUnfilteredText});
    } else if (body == "brctl: help ignore") {
      conn->Send({"PRIVMSG", reply_target, kIgnoreHelpText});
    } else if (body == "brctl: help filter") {
      conn->Send({"PRIVMSG", reply_target, kFilterHelpText});
    } else {
      conn->Send({"PRIVMSG", reply_target, kHelpText});
    }
    return;
  }

  {
    std::pair<std::string, std::string> source{conn->net(), msg.prefix_nick()};
    if (ignores_.count(source) > 0)
      return;
    source.second = "*";
    if (ignores_.count(source) > 0)
      return;
  }
  for (const auto& filtered : filters_)
    if (RE2::PartialMatch(re2::StringPiece(body.data(), body.size()), *filtered.second))
      return;

  for (const auto& route : routes_) {
    if (route.from_net() != conn->net() || route.from_chan() != chan)
      continue;

    auto to_conn = host_->conn(route.to_net());
    if (!to_conn)
      continue;

    std::string out;
    out.push_back('<');
    out.append(msg.prefix_nick());
    out.push_back('>');
    out.push_back(' ');
    out.append(body);

    to_conn->Send({ "PRIVMSG", route.to_chan(), out });
  }
}

bool Bridge::ParseSpec(Connection* conn, std::string_view spec, std::string_view *ignore_net, std::string_view *ignore_nick) {
  std::size_t sep = spec.find('/');

  if (sep != spec.npos) {
    *ignore_net = spec.substr(0, sep);
    if (!host_->conn(*ignore_net))
      return false;
    spec.remove_prefix(sep + 1);
  } else
    *ignore_net = conn->net();

  if (spec.size() > kMaxNickLen || spec.find(' ') != spec.npos)
    return false;

  *ignore_nick = spec;
  return true;
}

} // namespace esobot
