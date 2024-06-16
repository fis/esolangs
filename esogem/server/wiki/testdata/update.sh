#! /bin/bash

curl 'https://esolangs.org/w/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=title&utf8=' > search.title.json
curl 'https://esolangs.org/w/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=text&utf8=' > search.text.json
curl 'https://esolangs.org/w/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=1003&prop=text&utf8=' > get.json
