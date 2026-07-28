#ifndef ESOLOGS_VERIFIER_H_
#define ESOLOGS_VERIFIER_H_

#include <string>

#include "base/common.h"
#include "esologs/config.pb.h"
#include "web/request.h"
#include "web/response.h"
#include "web/writer.h"

namespace esologs {

class Verifier : public web::RequestHandler {
 public:

  explicit Verifier(const VerifierConfig& config);
  DISALLOW_COPY(Verifier);

  web::RequestHandler* css_handler();

  int HandleRequest(web::Request* req, web::Response* resp) override;

 private:
  class Template {
  public:
    Template(std::string_view path);
    DISALLOW_COPY(Template);

    void Render(web::Writer* w, const absl::flat_hash_map<std::string_view, std::string_view>& vars);

  private:
    struct Chunk {
      std::string data;
      bool literal;
      int cond;
      Chunk(std::string_view d, bool l, int c) : data(d), literal(l), cond(c) {}
    };

    std::vector<Chunk> content_ = {};
    absl::flat_hash_map<std::string, int> conds_ = {};
  };

  struct CssHandler : public web::RequestHandler {
    std::string css;
    int HandleRequest(web::Request* req, web::Response* resp) override;
  };

  std::string cookie_;
  std::string secret_;
  std::string domain_;
  std::unique_ptr<Template> form_tmpl_;
  std::unique_ptr<Template> success_tmpl_;
  CssHandler css_;
};

} // namespace esologs

#endif // ESOLOGS_STALKER_H_
