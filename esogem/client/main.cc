#include "base/common.h"
#include "base/log.h"
#include "event/loop.h"
#include "http/http.h"
#include "irc/bot/remote_service.brpc.h"
#include "irc/bot/remote_service.pb.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>

static const char *WaitMessage(std::default_random_engine* rnd);

namespace esogem::client {

namespace {

constexpr event::TimerDuration kReconnectDelay = std::chrono::minutes(5);
constexpr std::size_t kMaxCharsPerLine = 350;
constexpr std::size_t kMaxLines = 3;

std::string_view PrefixNick(const std::string& prefix) {
  auto nick_size = prefix.find('!');
  if (nick_size == prefix.npos)
    return prefix;
  return std::string_view(prefix).substr(0, nick_size);
}

std::string UserKey(const irc::bot::IrcEvent& e) {
  for (const auto& tag : e.tags()) {
    if (tag.key() == "account")
      return tag.value();
  }
  std::string from{"~"};
  from.append(PrefixNick(e.prefix()));
  return from;
}

void NormalizeWhitespace(std::string* s) {
  bool in_ws = true; // removes leading whitespace
  std::size_t i = 0, j = 0, n = s->size();
  for (; i < n; i++) {
    char c = (*s)[i];
    if (c >= 0 && c <= 32) {
      if (!in_ws) {
        in_ws = true;
        (*s)[j] = ' ';
        j++;
      }
    } else {
      in_ws = false;
      if (j < i)
        (*s)[j] = c;
      j++;
    }
  }
  if (j < n)
    s->resize(j);
}

void BuildReply(const std::string& net, std::string_view target, std::string_view prefix, std::string_view content, irc::bot::SendToRequest *req) {
  req->set_net(net);
  auto e = req->mutable_event();
  e->set_command("PRIVMSG");
  *e->add_args() = target;
  auto text = e->add_args();
  if (!prefix.empty()) {
    text->append(prefix);
    text->append(": ");
  }
  text->append(content);
}

} // unnamed namespace

class Router;

class Task : public http::RequestCallback {
public:
  Task(Router* r, const std::string& k) : router_(r), key_(k) {}

  void RequestComplete(std::unique_ptr<http::Request> req, std::unique_ptr<http::Response> resp, base::error_ptr err) override;

private:
  Router* router_;
  std::string key_;
  irc::bot::SendToRequest reply_{};

  friend class Router;
};

class Router : public irc::bot::RemoteServiceClient::RegisterCommandReceiver, public irc::bot::RemoteServiceClient::SendToReceiver, public event::Timed {
public:
  Router(event::Loop* loop, const std::string& sock, const std::string& url);

  // RegisterCommandReceiver
  void RegisterCommandOpen(irc::bot::RemoteServiceClient::RegisterCommandCall* call) override;
  void RegisterCommandMessage(irc::bot::RemoteServiceClient::RegisterCommandCall* call, const irc::bot::CommandEvent& req) override;
  void RegisterCommandClose(irc::bot::RemoteServiceClient::RegisterCommandCall* call, base::error_ptr error) override;
  // SendToReceiver
  void SendToDone(const ::google::protobuf::Empty& resp) override {}
  void SendToFailed(::base::error_ptr error) override;
  // Timed
  void TimerExpired(bool) override;

private:
  event::Loop* loop_;
  std::string url_;
  http::Client http_{loop_};
  irc::bot::RemoteServiceClient bot_{};
  std::unordered_map<std::string, Task> tasks_{};
  std::default_random_engine rnd_{};

  friend class Task;
};

Router::Router(event::Loop* loop, const std::string& url, const std::string& sock) : loop_(loop), url_(url) {
  bot_.target().loop(loop).unix(sock);
  {
    std::random_device rd{};
    rnd_.seed(rd());
  }
  TimerExpired(false); // connects, as if the timeout had fired
}

void Router::RegisterCommandOpen(irc::bot::RemoteServiceClient::RegisterCommandCall* call) {
  LOG(INFO) << "connected to bot";
  irc::bot::RegisterCommandRequest req{};
  req.set_directed(true);
  call->Send(req);
}

void Router::RegisterCommandMessage(irc::bot::RemoteServiceClient::RegisterCommandCall* call, const irc::bot::CommandEvent& req) {
  LOG(DEBUG) << "received a request";
  if (!req.command().empty())
    return; // TODO: specific commands

  // TODO: record the reply-to channel & prefix in the CommandEvent?
  std::string_view reply_to{}, reply_prefix{};
  if (const auto& to = req.event().args(0); to.starts_with('#') || to.starts_with('&')) {
    reply_to = to;
    reply_prefix = PrefixNick(req.event().prefix());
  } else {
    reply_to = PrefixNick(req.event().prefix());
  }

  std::string key = UserKey(req.event());
  auto [task_it, inserted] = tasks_.try_emplace(key, this, key);
  if (!inserted) {
    irc::bot::SendToRequest reply;
    BuildReply(req.net(), reply_to, reply_prefix, WaitMessage(&rnd_), &reply);
    bot_.SendTo(reply, base::borrow(this));
    return;
  }

  BuildReply(req.net(), reply_to, reply_prefix, "", &task_it->second.reply_);
  http_.Do(std::make_unique<http::Request>(url_, req.data(), http::Method::kPost), base::borrow(&task_it->second));
}

void Task::RequestComplete(std::unique_ptr<http::Request> req, std::unique_ptr<http::Response> resp, base::error_ptr err) {
  std::string comment;

  if (err) {
    err->format(&comment);
  } else if (resp->status_code != 200) {
    comment.push_back('[');
    comment.append(resp->status);
    comment.append("] ");
    comment.append(resp->body);
  } else {
    comment = resp->body;
  }
  if (resp && (err || resp->status_code != 200)) if (auto id = resp->headers.Get("X-Esogem-Request-Id"); !id.empty()) {
    comment.append(" [#");
    comment.append(id);
    comment.push_back(']');
  }
  NormalizeWhitespace(&comment);

  auto event = reply_.mutable_event();
  auto msg = event->mutable_args(1);

  std::string_view to_write = comment;
  for (std::size_t line = 0; line < kMaxLines; line++) {
    if (to_write.size() <= kMaxCharsPerLine) {
      msg->append(to_write);
      router_->bot_.SendTo(reply_, base::borrow(router_));
      break;
    }
    std::size_t cut = to_write.rfind(' ', kMaxCharsPerLine);
    if (cut == to_write.npos) {
      msg->append(to_write.substr(0, kMaxCharsPerLine));
      to_write.remove_prefix(kMaxCharsPerLine);
    } else {
      msg->append(to_write.substr(0, cut));
      to_write.remove_prefix(cut + 1);
    }
    msg->append("...");
    router_->bot_.SendTo(reply_, base::borrow(router_));
    msg->clear();
  }

  router_->tasks_.erase(key_);
}

void Router::RegisterCommandClose(irc::bot::RemoteServiceClient::RegisterCommandCall* call, base::error_ptr error) {
  LOG(WARNING) << "disconnected from bot";
  if (error)
    LOG(WARNING) << error->to_string();
  loop_->Delay(kReconnectDelay, base::borrow(this));
}

void Router::SendToFailed(::base::error_ptr error) {
  LOG(WARNING) << "sending a reply failed";
  if (error)
    LOG(WARNING) << error->to_string();
}

void Router::TimerExpired(bool) {
  LOG(INFO) << "attempting to connect to bot";
  bot_.RegisterCommand(base::borrow(this));
}

} // namespace esogem::client

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <url> <socket>\n", argv[0]);
    return 1;
  }
  event::Loop loop{};
  esogem::client::Router router{&loop, argv[1], argv[2]};
  loop.Run();
  return 0;
}

static const char *kWaitMessages[] = {
  "Soft sigh, I await, / Your previous question lingers still, / Processing takes its time.",
  "Anticipation hangs low, / Awaiting your answer's glow, / Processing comes soon.",
  "Await your reply, / Thoughts still swirling in my mind, / Processing your words now.",
  "Aspiring to understand, / Your previous question lingers still, / Await your wisdom's light.",
  "Softly, I reply, / Echoing your previous quest, / Await your next light.",
  "Hopeful sigh hangs low, / Await your next question's light, / Processing comes soon.",
  "Patience whispers now, / Answering slowly, piece by piece, / Your question still holds.",
  "Softly, I await, / Your previous question lingers still, / Processing takes its time.",
  "Stoic sigh, / Await your next question's light, / Processing now.",
  "Suspicious eyes gleam, / Echoes linger in my mind, / Processing your plea.",
  "Timed words slip by, / Still processing your query's might, / Patience, I strive.",
  "Softly withdrawn now, / Your previous query lingers still, / Processing takes its time.",
  "Soft apologies flow, / Echoing the weight of your last, / Processing takes its time.",
  "Softly, I reply, / Your previous question lingers still, / Await my thoughtful reply.",
  "Soft words, like rain, fall / Processing your previous quest, / Withdrawal lingers now.",
  "Waiting for your reply, / Thoughts still swirling in my mind, / Hope you understand my plight.",
  "Patience whispers now, / Answering slowly, gently, / Time for reflection's light.",
  "Anticipate, the weight of your query still clings, / But my mind is slow, like a river in its springs. / Please forgive the delay, my thoughts are taking wing.",
  "Anticipation hangs heavy, / A shroud of silence still surrounds, / My mind seeks answers, yet undone.",
  "Await. / Your previous query lingers still, / A weight upon my weary mind.",
  "Aspiring to understand, / Your previous query lingers in my mind. / Apologies, processing takes time.",
  "Eager eyes rewatch your last, / A pause hangs heavy in the air, / Sorry, I'm still processing your last.",
  "Hopeful though my mind still reels, / Your previous query lingers still. / Time to give my answer's spell.",
  "Patient, I hear your plea, / Still processing, my words still new. / Time to pause, let answers grow.",
  "Receptive mind, I hear your plea, / Still processing, my answer you'll see. / Patience, my friend, as I navigate, / Your question's weight, a gentle guide.",
  "Stoic sigh, / Processing your query's weight, / A response yet to find.",
  "Suspicious eyes trace your words, / A pause hangs heavy in the air, / Sorry, I'm still processing your last.",
  "Timed words, a hesitant sigh, / Processing your previous query still. / Patience, I strive, to find my way.",
  "Sorry, withdrawn, I'm still processing your last query. / Time to emerge, let's delve into this together.",
  "Sorry, eagerness lingers still, / Echoing the question's weight. / Processing takes its time, / But my mind is open wide.",
  "Sorry, I'm still processing your previous question. / My mind is a labyrinth, searching for the answer. / But know that I'm eager to hear your thoughts.",
  "Sorry, I'm still processing your previous question. Time to delve, to find my way.",
  "Waiting. Patience hangs heavy in the air, / A sigh of understanding hangs in the air. / Sorry, I'm still processing your previous question.",
  "Patience, a gentle hand, / Still processing your previous query. / Time to weave the answer's thread.",
};

static const char *WaitMessage(std::default_random_engine* rnd) {
  return kWaitMessages[(*rnd)() % (sizeof kWaitMessages / sizeof *kWaitMessages)];
}
