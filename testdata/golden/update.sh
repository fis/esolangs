#! /bin/bash

set -e

curl http://localhost:12808/test/                   > esologs.index.html
curl http://localhost:12808/test/2020.html          > esologs.2020.html
curl http://localhost:12808/test/2021-01-01.html    > esologs.2021-01-01.html
curl http://localhost:12808/test/2021-01-01.txt     > esologs.2021-01-01.txt
curl http://localhost:12808/test/2021-01-01-raw.txt > esologs.2021-01-01-raw.txt
curl http://localhost:12808/test/2021-01.html       > esologs.2021-01.html
