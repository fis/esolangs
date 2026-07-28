#include <charconv>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_split.h"
#include <openssl/evp.h>

#include "base/exc.h"
#include "base/log.h"
#include "esologs/verifier.h"
#include "web/writer.h"

namespace esologs {

namespace {

std::string LoadFile(std::string_view path) {
  std::ifstream file{std::filesystem::path{path}, std::ios::in | std::ios::binary};
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

uint64_t Now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

struct WordGen {
  char seed[21];
  char word1[6];
  char word2[6];
  WordGen(std::uint64_t seed_num, std::string_view secret);
 private:
  void MakeWord(char word[6], std::uint16_t bits);
};

WordGen::WordGen(std::uint64_t seed_num, std::string_view secret) {
  std::snprintf(seed, sizeof seed, "%" PRIu64, seed_num);
  union {
    std::uint8_t bytes[32];
    struct { std::uint16_t w1, w2; } words;
  } hash;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
  EVP_DigestUpdate(ctx, &seed_num, sizeof seed_num);
  EVP_DigestUpdate(ctx, secret.data(), secret.size());
  EVP_DigestFinal_ex(ctx, hash.bytes, nullptr);
  EVP_MD_CTX_free(ctx);
  MakeWord(word1, hash.words.w1);
  MakeWord(word2, hash.words.w2);
}

void WordGen::MakeWord(char word[6], std::uint16_t bits) {
  static const char cons[16] = {'b', 'c', 'd', 'f', 'g', 'h', 'k', 'l', 'm', 'n', 'p', 'r', 's', 't', 'v', 'z'};
  static const char vow[4] = {'a', 'e', 'i', 'o'};
  word[0] = cons[bits & 0xf];
  word[1] = vow[(bits >> 4) & 0x3];
  word[2] = cons[(bits >> 6) & 0xf];
  word[3] = cons[(bits >> 10) & 0xf];
  word[4] = vow[(bits >> 14) & 0x3];
  word[5] = 0;
}

} // unnamed namespace

Verifier::Verifier(const VerifierConfig& config) : cookie_(config.cookie()), secret_(config.secret()), domain_(config.domain()) {
  form_tmpl_ = std::make_unique<Template>(config.form());
  success_tmpl_ = std::make_unique<Template>(config.success());
  if (!config.css().empty())
    css_.css = LoadFile(config.css());
}

web::RequestHandler* Verifier::css_handler() {
  if (css_.css.empty())
    return nullptr;
  return &css_;
}

int Verifier::HandleRequest(web::Request* req, web::Response* resp) {
  // Verifier logic:
  // - If it's a post request, parse the form data and verify the words against the seed.
  // - If it was a (successful) post request, or the cookie is already set, render the success page with Set-Cookie and new expiration time.
  // - Otherwise, render the form, potentially with an error message from the validation.

  if (req->method() == web::Method::kHead) {
    web::Writer web{resp, web::kContentTypeHTML, 200};
    return 200;
  }

  bool is_pass = false, has_cookie = false;
  std::string_view error{};
  std::string backref_b64;

  if (req->method() == web::Method::kPost) {
    std::string_view seed = req->param("seed"), word1 = req->param("key1"), word2 = req->param("key2");
    uint64_t seed_num;
    auto [ptr, ec] = std::from_chars(seed.data(), seed.data() + seed.size(), seed_num);
    if (ec != std::errc() || ptr != seed.data() + seed.size()) {
      error = "invalid seed: not a number";
    } else if (seed_num < Now() - 900000000) {
      error = "form too old (limit: 15 min)";
    } else {
      WordGen gen{seed_num, secret_};
      bool ok1 = word1 == gen.word1, ok2 = word2 == gen.word2;
      if (!ok1 && !ok2)
        error = "neither word matches";
      else if (!ok1)
        error = "first word does not match";
      else if (!ok2)
        error = "second word does not match";
      else
        is_pass = true;
    }
    backref_b64 = req->param("ref");
  } else if (auto qs = req->query(); qs.starts_with("ref=")) {
    absl::WebSafeBase64Escape(qs.substr(4), &backref_b64);
  }

  if (const char* cookie_data = req->header("Cookie"); cookie_data) {
    // TODO: move cookie-parsing into web::Request if a second use case comes up
    for (const auto cookie : absl::StrSplit(cookie_data, "; ")) {
      if (cookie_ == cookie) {
        has_cookie = true;
        break;
      }
    }
  }

  if (is_pass || has_cookie) {
    std::string backref;
    if (!backref_b64.empty())
      absl::WebSafeBase64Unescape(backref_b64, &backref);
    if (backref.find('"') != backref.npos)
      backref.clear();
    std::string set_cookie = "Set-Cookie: ";
    set_cookie += cookie_;
    set_cookie += "; Max-Age=7776000; Path=/";
    if (!domain_.empty()) {
      set_cookie += "; Domain=";
      set_cookie += domain_;
    }
    set_cookie += "\r\n";
    web::Writer web{resp, web::kContentTypeHTML, 200, set_cookie};
    success_tmpl_->Render(&web, {
      {"ref", backref},
      {"pass", is_pass ? "yes" : ""},
      {"cookie", !is_pass ? "yes" : ""},
    });
    return 200;
  }

  {
    WordGen gen{Now(), secret_};
    absl::flat_hash_map<std::string_view, std::string_view> vars = {
      {"seed", gen.seed},
      {"word1", gen.word1},
      {"word2", gen.word2},
      {"error", error},
      {"ref", backref_b64},
    };
    web::Writer web{resp, web::kContentTypeHTML};
    form_tmpl_->Render(&web, vars);
  }
  return 200;
}

int Verifier::CssHandler::HandleRequest(web::Request* req, web::Response* resp) {
  web::Writer web{resp, web::kContentTypeCSS};
  if (req->method() == web::Method::kGet)
    web.Write(css);
  return 200;
}

Verifier::Template::Template(std::string_view path) {
  std::string text = LoadFile(path);
  int cond = 0;
  for (std::string_view tail = text; !tail.empty(); ) {
    std::size_t next_pos = tail.find("${");
    if (next_pos == tail.npos) {
      content_.emplace_back(tail, true, cond);
      break;
    }
    if (next_pos > 0)
      content_.emplace_back(tail.substr(0, next_pos), true, cond);
    tail.remove_prefix(next_pos + 2);
    next_pos = tail.find('}');
    if (next_pos == tail.npos)
      throw base::Exception("unterminated variable in template");
    std::string_view op = tail.substr(0, next_pos);
    tail.remove_prefix(next_pos + 1);
    switch (op.empty() ? 0 : op[0]) {
    case 0:
      break;
    case '+':
      cond = conds_.try_emplace(op.substr(1), (int) conds_.size() + 1).first->second;
      break;
    case '-':
      cond = 0;
      break;
    default:
      content_.emplace_back(op, false, cond);
    }
  }
}

void Verifier::Template::Render(web::Writer* w, const absl::flat_hash_map<std::string_view, std::string_view>& vars) {
  std::vector<bool> active_conds(conds_.size() + 1);
  active_conds[0] = true; // implicit
  for (const auto& cond : conds_)
    if (auto it = vars.find(cond.first); it != vars.end() && !it->second.empty())
      active_conds[cond.second] = true;
  for (const auto& chunk : content_) {
    if (!active_conds[chunk.cond])
      continue;
    if (chunk.literal)
      w->Write(chunk.data);
    else if (auto it = vars.find(chunk.data); it != vars.end())
      w->Write(it->second);
    else
      w->Write("undefined");
  }
}

} // namespace esologs
