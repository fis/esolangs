#include "base/log.h"
#include "esologs/server.h"
#include "event/loop.h"
#include "proto/util.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    LOG(ERROR) << "usage: " << argv[0] << " <esologs.config>";
    return 1;
  }

  setenv("TZ", "UTC", 1);  // no-op, for safety
  event::Loop loop;
  esologs::Config config;
  proto::ReadText(argv[1], &config);
  esologs::Server server(config, &loop);
  LOG(INFO) << "server started";

  loop.Run();
  return 0;
}
