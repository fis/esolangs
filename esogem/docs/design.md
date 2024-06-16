# Design

This document contains some notes on how the `esogem` system works.

## Model

To save on resource use, all these steps will use the same model, one of the Gemma instruction-tuned variants.

The Gemma IT models expect the following prompt structure:

```
<start_of_turn>user
...user instructions go here... <end_of_turn>
<start_of_turn>model
```

To respond to a message from IRC, the overall sequence of steps is:

- Ask the model to generate search term(s) to look up in the wiki.
- While there's still unused search terms and some space in the prompt:
  - Pick a search term from the list and call the search API.
  - Get top K results sorted by relevance.
  - If plaintext page is too long, run the compaction process on it.
  - Include the text in the final prompt.
- Ask the model to generate the response.

The process for compacting an article is:

- If the original page is too long to fit even in an otherwise empty prompt:
  - If the page has sections, try to pick only those sections that seem relevant for the search term.
  - As a final fallback, just truncate the page text to fit.
- If the remaining text still doesn't fit in available prompt space:
  - Ask the model to summarize the text in the context of the user question.
- If the model-generated summary is too long, just truncate it.

### Prompts

Search query generation:

```
<start_of_turn>user
You are an assistant to help find information in an esoteric programming language wiki.
Your task is to make a list of search terms most likely to return relevant results for a question.
The question is:

...IRC question goes here...

Return only the list of 1 to 3 key search terms. <end_of_turn>
<start_of_turn>model
```

Article summarization:

```
<start_of_turn>user
Your task is to summarize text but retain the information necessary to answer a question.
The question is:

...IRC question goes here...

Keeping the preceding question in mind, summarize the following text in N words or fewer:

...article text goes here... <end_of_turn>
<start_of_turn>model
```

Response generation:

```
<start_of_turn>user
Your task is to answer questions on an IRC channel about esoteric programming languages.
Here is some background material:

...articles and article summaries...

Answer the following question:

...IRC question goes here...

Answer the question in the style of an IRC chat message written by a slightly snarky developer. <end_of_turn>
<start_of_turn>model
```

### Evaluation

Set of plausible questions for the model:

- TODO
- Make up some?

## System

There's four(-ish) components involved in how this is wired up to IRC:

- `esogem/model`:
  A CMake C++ binary that uses [gemma.cpp](https://github.com/google/gemma.cpp) to host the model.
  Uses a very simple synchronous request-response protocol over its standard input/output streams.
- `esogem/server`:
  A Go binary that implements most of the "business logic".
  Exposes a HTTP endpoint, and is the parent process to `esogem/model`.
  (There's also a pre-existing nginx proxy in front of this.)
- `esogem/client`:
  Part of the top-level Bazel project, a C++ executable.
  It uses the `brpc` service exposed by the bot to observe IRC questions, and makes HTTP requests to answer them.
  There's some per-user rate-limiting logic here as well.
- `esobot`:
  The IRC bot known as `esolangs`, which doesn't know anything specific about this particular function.

The `server` component runs on a moderately beefy (8 cores, 32G RAM) home-hosted server.
It auto-starts `model` as a subprocess, and talks to it over a pair of pipes.
If there have been no requests in a while, it also lets the `model` process die so it doesn't permanently reserve a lot of memory.
There's also a (pre-existing) nginx proxy in front of the Go HTTP server, handling TLS and auth.

The `client` component runs on the same system as the IRC bot, in order to reach its Unix domain socket remote control `brpc` service.
It uses that service to observe messages directed to the bot, and to respond.
