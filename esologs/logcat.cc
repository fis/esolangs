#include <cstdio>

#include "base/exc.h"
#include "esologs/log.pb.h"
#include "proto/delim.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s log.pb [log.pb ...]\n", argv[0]);
    return 1;
  }

  esologs::LogEvent event;

  for (int i = 1; i < argc; i++) {
    try {
      proto::DelimReader reader(argv[i]);
      while (reader.Read(&event)) {
        auto tstamp = event.time_us();
        std::printf(
            "[%02d:%02d:%02d.%03d] ",
            (int)(tstamp / 3600000000),
            (int)(tstamp / 60000000 % 60),
            (int)(tstamp / 1000000 % 60),
            (int)((tstamp + 500) / 1000 % 1000));
        if (event.direction() == esologs::LogEvent::SENT)
          std::fputs("=> ", stdout);
        if (!event.prefix().empty())
          std::printf(":%s ", event.prefix().c_str());
        std::fputs(event.command().c_str(), stdout);
        for (const auto& arg : event.args())
          std::printf(" '%s'", arg.c_str());
        std::fputc('\n', stdout);
      }
    } catch (base::Exception& e) {
      std::fprintf(stderr, "error reading %s: %s\n", argv[i], e.what());
    }
  }

  return 0;
}
