// Package api implements the esogem HTTP API.
package api

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"sync/atomic"
	"time"

	"github.com/fis/esolangs/esogem/server/flow"
	"github.com/fis/esolangs/esogem/server/model"
	"github.com/fis/esolangs/esogem/server/proto/delim"
	"github.com/fis/esolangs/esogem/server/wiki"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

// TODO: make Serve take the mux rather than set up the server, makes it easier to test

const (
	requestIdHeader       = "x-esogem-request-id"
	maxConcurrentRequests = 10
	maxRequestBytes       = 1024 // comfortably fits an IRC message
	modelTimeout          = 5 * time.Minute
	logFileSize           = 2 * 1024 * 1024 // log file chunk size
)

func AddHandler(mux *http.ServeMux, cfg *spb.Config) error {
	modelHost, err := model.NewHost(cfg.Model)
	if err != nil {
		return err
	}
	wikiClient := wiki.NewClient(cfg.Wiki)
	ms := &modelServer{model: modelHost, wiki: wikiClient}
	h := &httpHandler{handler: limitConcurrency(ms.handleQuestion, maxConcurrentRequests)}
	if cfg.LogFile != "" {
		h.lw, err = delim.NewLogWriter(cfg.LogFile, logFileSize, 16)
		if err != nil {
			return fmt.Errorf("open logfile: %w", err)
		}
	}
	mux.Handle("POST /ask", h)
	mux.Handle("/", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "bad thing", http.StatusInternalServerError)
	}))
	return nil
}

type handler func(http.ResponseWriter, *http.Request, *spb.LogEntry) error

type httpHandler struct {
	handler handler
	id      idGenerator
	lw      *delim.LogWriter
}

func (h *httpHandler) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	logEntry := &spb.LogEntry{}
	logEntry.TimeUsec = h.id.next()
	logEntry.Request = fmt.Sprintf("%s %s", req.Method, req.URL.String())
	w.Header().Add(requestIdHeader, strconv.FormatInt(logEntry.TimeUsec, 16))

	// TODO: consider doing some sort of tracing
	err := h.handler(w, req, logEntry)
	if err != nil {
		logEntry.Error = err.Error()
	}

	if br, ok := err.(httpError); ok {
		http.Error(w, br.body, br.status)
	} else if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}

	if h.lw != nil {
		h.lw.Write(logEntry)
	}
	// TODO: maybe also write something to slog
}

type modelServer struct {
	model *model.Host
	wiki  *wiki.Client
	id    idGenerator
}

func (s *modelServer) handleQuestion(w http.ResponseWriter, req *http.Request, logEntry *spb.LogEntry) error {
	// TODO: make more whimsical and random error messages?

	body, err := io.ReadAll(io.LimitReader(req.Body, maxRequestBytes+1))
	if err != nil {
		return httpError{status: http.StatusBadRequest, body: "failed to read request"}
	} else if len(body) == 0 {
		return httpError{status: http.StatusBadRequest, body: "request must not be empty"}
	} else if len(body) > maxRequestBytes {
		return httpError{status: http.StatusRequestEntityTooLarge, body: "request too large"}
	}
	question := string(body)
	logEntry.Question = question

	fl := flow.New(s.model, s.wiki, question, logEntry)
	ctx, cancel := context.WithTimeout(req.Context(), modelTimeout)
	defer cancel()
	resp, err := fl.Respond(ctx)
	if err != nil {
		return err
	}

	w.Header().Add("Content-Type", "text/plain; charset=utf-8")
	w.Write([]byte(resp))
	return nil
}

func limitConcurrency(h handler, maxReqs int) handler {
	count := atomic.Int32{}
	return func(w http.ResponseWriter, req *http.Request, logEntry *spb.LogEntry) error {
		newCount := int(count.Add(1))
		defer count.Add(-1)
		if newCount > maxReqs {
			return httpError{status: http.StatusServiceUnavailable, body: "too many concurrent requests"}
		}
		return h(w, req, logEntry)
	}
}

type httpError struct {
	status int
	body   string
}

func (br httpError) Error() string {
	return fmt.Sprintf("http %d: %s", br.status, br.body)
}

type idGenerator struct {
	lastUsec atomic.Int64
}

func (g *idGenerator) next() int64 {
	for {
		now := time.Now().UnixMicro()
		last := g.lastUsec.Load()
		if now <= last {
			now = last + 1
		}
		if g.lastUsec.CompareAndSwap(last, now) {
			return now
		}
	}
}
