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
    TargetConfig *target = config.add_target();
    target->set_name("test");
    target->set_log_path("testdata/logs");
    target->set_nick("esolangs");
    target->set_title("test logs");
    target->set_about("<p>These are test logs.</p>\n");
    target->set_announce("<p>This is a test announcement.</p>\n");
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
    {"/test/",                   "testdata/golden/esologs.index.html"},
    {"/test/2020.html",          "testdata/golden/esologs.2020.html"},
    {"/test/2021-01-01.html",    "testdata/golden/esologs.2021-01-01.html"},
    {"/test/2021-01-01.txt",     "testdata/golden/esologs.2021-01-01.txt"},
    {"/test/2021-01-01-raw.txt", "testdata/golden/esologs.2021-01-01-raw.txt"},
    {"/test/2021-01.html",       "testdata/golden/esologs.2021-01.html"},
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
