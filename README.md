# esolangs.org infrastructure

This repository holds pieces of infrastructure involved in keeping the
[esolangs.org](https://esolangs.org/) wiki running.

## Components

### esobot

IRC bot under the name `esowiki` on the `#esoteric` IRC channel in Freenode.

It listens for the MediaWiki UDP "recent changes" feed, and posts a note of any edits on the
channel. It also logs the channel, and and produces the data for the logs browser.

### esologs

A web frontend for browsing the logs of the `#esoteric` IRC channel.

## Development

For convenience, the code here is built on some assorted bits and pieces of C++ I had written for
other purposes. You can find them in the [github.com/fis/bracket](https://github.com/fis/bracket)
repository.

For code style and building and that sort of matters, see
[README.md](https://github.com/fis/bracket/blob/master/README.md) in that repository.
