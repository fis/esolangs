#define _POSIX_C_SOURCE 200809L
#include <signal.h>

#include <algorithm>
#include <atomic>
#include <memory>

#include <gemma/gemma.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "model.pb.h"

namespace esogem::model {

constexpr const char* kTokenizerFile = "/tokenizer.spm";
constexpr const char* kWeightFile = "/2b-it-sfp.sbs";
constexpr gcpp::Model kModelType = gcpp::Model::GEMMA_2B;
constexpr size_t kMaxPromptTokens = 3072; // TODO: consider bumping these up (needs change in compilation settings)
constexpr size_t kMaxAnswerTokens = 1024;
constexpr float kTemperature = 1.0f;
constexpr int kBosToken = 2;

class Model {
public:
  Model();
  void Load(std::string_view path, std::string* error);
  int32_t TokenCount(const std::string& prompt, std::string* error);
  void Generate(const std::string& prompt, std::string* generated, std::string* error);

private:
  std::unique_ptr<gcpp::Gemma> gemma_;
  gcpp::KVCache kv_cache_;
  std::vector<int> prompt_tokens_;
  std::vector<int> generated_tokens_;
  std::mt19937 gen_;
  hwy::ThreadPool pool_{8};
};

Model::Model() {
  std::random_device rd;
  gen_.seed(rd());
}

void Model::Load(std::string_view path, std::string* error) {
  if (gemma_) {
    *error = "model already loaded";
    return;
  }

  gcpp::Path tokenizer{}, weights{};
  tokenizer.path.append(path);
  tokenizer.path.append(kTokenizerFile);
  weights.path.append(path);
  weights.path.append(kWeightFile);
  gemma_ = std::make_unique<gcpp::Gemma>(tokenizer, weights, kModelType, pool_);
  kv_cache_ = gcpp::CreateKVCache(kModelType);
}

int32_t Model::TokenCount(const std::string& prompt, std::string* error) {
  if (!gemma_) {
    *error = "model not loaded";
    return 0;
  }
  auto tokenizer = gemma_->Tokenizer();
  prompt_tokens_.clear();
  if (!tokenizer->Encode(prompt, &prompt_tokens_))
    *error = "tokenization failed";
  return prompt_tokens_.size() + 1; // account for BOS token
}

// interrupt signal handling
static std::atomic<bool> usr1_interrupt_signal;
static void usr1_action(int, siginfo_t*, void*) {
  usr1_interrupt_signal.store(true);
};

void Model::Generate(const std::string& prompt, std::string* generated, std::string* error) {
  if (!gemma_) {
    *error = "model not loaded";
    return;
  }
  auto tokenizer = gemma_->Tokenizer();

  prompt_tokens_.clear();
  if (!tokenizer->Encode(prompt, &prompt_tokens_)) {
    *error = "tokenization failed";
    return;
  }
  prompt_tokens_.insert(prompt_tokens_.begin(), kBosToken);
  size_t prompt_size = prompt_tokens_.size();
  if (prompt_size > kMaxPromptTokens) {
    // assume middle of the prompt is the least useful bit
    std::copy(prompt_tokens_.end() - kMaxPromptTokens/2, prompt_tokens_.end(), prompt_tokens_.begin() + kMaxPromptTokens/2);
    prompt_tokens_.resize(kMaxPromptTokens);
    prompt_size = kMaxPromptTokens;
  }

  generated_tokens_.clear();
  size_t current_pos = 0;
  auto stream_token = [this, &current_pos, prompt_size](int token, float) {
    ++current_pos;
    if (current_pos > prompt_size && token != gcpp::EOS_ID)
      generated_tokens_.push_back(token);
    return !usr1_interrupt_signal.load();
  };

  gcpp::RuntimeConfig runtime_config = {
    .max_tokens = kMaxPromptTokens + kMaxAnswerTokens,
    .max_generated_tokens = kMaxAnswerTokens,
    .temperature = kTemperature,
    .verbosity = 0,
    .gen = &gen_,
    .stream_token = stream_token,
    .accept_token = [](int) { return true; },
  };
  gcpp::TimingInfo timing; // TODO: maybe export as metrics
  usr1_interrupt_signal.store(false);
  gcpp::GenerateGemma(*gemma_, runtime_config, prompt_tokens_, 0, kv_cache_, pool_, timing);
  if (usr1_interrupt_signal.load())
    *error = "interrupted";
  else if (!tokenizer->Decode(generated_tokens_, generated))
    *error = "detokenization failed";
}

class ModelServer {
public:
  ModelServer() : in_(0), out_(1) {}
  void Serve();

private:
  // TODO: absl StatusOr stuff?
  bool HandleRequest();
  template<class Req, class Resp>
  bool HandleProtoRequest(uint32_t size, void (ModelServer::* handler)(const Req&, Resp*));
  void HandleConfig(const ModelConfigRequest& req, ModelConfigResponse* resp);
  void HandleTokenCount(const ModelTokenCountRequest& req, ModelTokenCountResponse* resp);
  void HandleGenerate(const ModelGenerateRequest& req, ModelGenerateResponse* resp);

  bool ReadRequestHeader(ModelFunction* fun, uint32_t* size);
  bool ReadProto(uint32_t size, google::protobuf::Message* req);
  bool WriteProto(const google::protobuf::Message& resp);

  std::unique_ptr<Model> model_;
  google::protobuf::io::FileInputStream in_;
  google::protobuf::io::FileOutputStream out_;
};

void ModelServer::Serve() {
  struct sigaction usr1{};
  usr1.sa_sigaction = usr1_action;
  usr1.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &usr1, NULL);
  for (;;) {
    if (!HandleRequest())
      break;
  }
}

bool ModelServer::HandleRequest() {
  ModelFunction fun;
  uint32_t size;
  if (!ReadRequestHeader(&fun, &size))
    return false;
  switch (fun) {
  case MODEL_FUNCTION_CONFIG: return HandleProtoRequest(size, &ModelServer::HandleConfig);
  case MODEL_FUNCTION_TOKEN_COUNT: return HandleProtoRequest(size, &ModelServer::HandleTokenCount);
  case MODEL_FUNCTION_GENERATE: return HandleProtoRequest(size, &ModelServer::HandleGenerate);
  default: return false;
  }
}

template<class Req, class Resp>
bool ModelServer::HandleProtoRequest(uint32_t size, void (ModelServer::* handler)(const Req&, Resp*)) {
  Req req{};
  Resp resp{};
  if (!ReadProto(size, &req))
    return false;
  (this->*handler)(req, &resp);
  return WriteProto(resp);
}

void ModelServer::HandleConfig(const ModelConfigRequest& req, ModelConfigResponse* resp) {
  if (model_) {
    resp->set_error("already configured");
    return;
  }
  model_ = std::make_unique<Model>();
  model_->Load(req.model_dir(), resp->mutable_error());
  if (!resp->error().empty())
    model_.reset();
}

void ModelServer::HandleTokenCount(const ModelTokenCountRequest& req, ModelTokenCountResponse* resp) {
  if (!model_) {
    resp->set_error("not configured");
    return;
  }
  resp->set_token_count(model_->TokenCount(req.prompt(), resp->mutable_error()));
}

void ModelServer::HandleGenerate(const ModelGenerateRequest& req, ModelGenerateResponse* resp) {
  if (!model_) {
    resp->set_error("not configured");
    return;
  }
  model_->Generate(req.prompt(), resp->mutable_generated(), resp->mutable_error());
}

bool ModelServer::ReadRequestHeader(ModelFunction* fun, uint32_t* size) {
  google::protobuf::io::CodedInputStream in{&in_};
  uint32_t raw_fun;
  if (!in.ReadLittleEndian32(&raw_fun))
    return false;
  *fun = static_cast<ModelFunction>(raw_fun);
  return in.ReadLittleEndian32(size);
}

bool ModelServer::ReadProto(uint32_t size, google::protobuf::Message* req) {
  google::protobuf::io::LimitingInputStream in{&in_, size};
  return req->ParseFromZeroCopyStream(&in);
}

bool ModelServer::WriteProto(const google::protobuf::Message& resp) {
  {
    google::protobuf::io::CodedOutputStream out{&out_};
    out.WriteLittleEndian32(resp.ByteSizeLong());
    resp.SerializeWithCachedSizes(&out);
    if (out.HadError())
      return false;
  }
  return out_.Flush();
}

} // namespace esogem::model

int main(void) {
  esogem::model::ModelServer ms;
  ms.Serve();
  return 0;
}
