#include <fstream>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "httplib/httplib.h"

#include "esologs/config.pb.h"
#include "esologs/server.h"

namespace esologs {

struct ServerTest : public ::testing::Test {
  ServerTest() {
    Config config;
    config.set_listen_port("127.0.0.1:0");
    config.set_log_path("testdata/logs");
    config.set_nick("esowiki");
    server = std::make_unique<Server>(config, &loop);
    client = std::make_unique<httplib::Client>("127.0.0.1", server->port());
  }

  event::Loop loop;
  std::unique_ptr<Server> server;
  std::unique_ptr<httplib::Client> client;
};

TEST_F(ServerTest, DiffGoldenFiles) {
  static const struct {
    const char *url;
    const char *golden_file;
  } tests[] = {
    {"/",                   "testdata/golden/esologs.index.html"},
    {"/2020.html",          "testdata/golden/esologs.2020.html"},
    {"/2021-01-01.html",    "testdata/golden/esologs.2021-01-01.html"},
    {"/2021-01-01.txt",     "testdata/golden/esologs.2021-01-01.txt"},
    {"/2021-01-01-raw.txt", "testdata/golden/esologs.2021-01-01-raw.txt"},
    {"/2021-01.html",       "testdata/golden/esologs.2021-01.html"},
  };
  static const size_t ntests = sizeof tests / sizeof tests[0];

  for (size_t test_idx = 0; test_idx < ntests; test_idx++) {
    const auto& test = tests[test_idx];

    std::string golden;
    std::getline(std::ifstream(test.golden_file), golden, '\0');
    ASSERT_GT(golden.size(), 0);

    auto resp = client->Get(test.url);
    ASSERT_EQ(200, resp->status);
    EXPECT_EQ(golden, resp->body);
  }
}

} // namespace esologs