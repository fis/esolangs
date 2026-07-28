#! /bin/bash

curl 'https://esolangs.org/w/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=title&utf8=' > search.title.json
curl 'https://esolangs.org/w/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=text&utf8=' > search.text.json
for pageid in 24081 2596 18466 12058 19787 14985 16181 25173 24756 9555 15304 16994 19757 3419 10129 24854 1140 14244 1003; do
  curl 'https://esolangs.org/w/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid='$pageid'&prop=text&utf8=' > get.$pageid.json
done
