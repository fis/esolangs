// Package model handles interfacing with the model-hosting binary.
package model

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"syscall"
	"time"

	"google.golang.org/protobuf/proto"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

const (
	maxIdleTime = 30 * time.Minute // if idle longer than this, stop the model process
	gracePeriod = 30 * time.Second // maximum wait after cancelling a call or attempting to stop the process before it gets forcibly killed
	maxBufSize  = 8 * 1024 * 1024  // absolute upper bound for a model call, will panic if exceeded
)

type Host struct {
	cfg  *spb.ModelConfig
	conn chan *modelConn
}

func NewHost(cfg *spb.ModelConfig) (*Host, error) {
	if err := validateConfig(cfg); err != nil {
		return nil, fmt.Errorf("config validation: %w", err)
	}
	h := &Host{
		cfg:  cfg,
		conn: make(chan *modelConn, 1),
	}
	h.conn <- nil
	return h, nil
}

func validateConfig(cfg *spb.ModelConfig) error {
	if _, err := os.Stat(cfg.BinaryPath); err != nil {
		return fmt.Errorf("stat binary: %w", err)
	}
	if info, err := os.Stat(cfg.DataDir); err != nil {
		return fmt.Errorf("stat model dir: %w", err)
	} else if !info.IsDir() {
		return fmt.Errorf("not a directory: %s", cfg.DataDir)
	}
	return nil
}

func (h *Host) CountTokens(ctx context.Context, prompt string) (int, error) {
	return run(h, ctx, prompt, (*modelConn).countTokens)
}

func (h *Host) Generate(ctx context.Context, prompt string, logEntry *spb.LogEntry) (string, error) {
	resp, err := run(h, ctx, prompt, (*modelConn).generate)
	if err == nil {
		logEntry.Generations = append(logEntry.Generations, &spb.GenerationEntry{Prompt: prompt, Response: resp})
	}
	return resp, err
}

func run[T any](h *Host, ctx context.Context, prompt string, f func(*modelConn, context.Context, string) (T, error, bool)) (result T, err error) {
	select {
	case c := <-h.conn:
		defer func() { h.conn <- c }()
		if c == nil {
			c, err = newConn(h)
			if err != nil {
				return result, fmt.Errorf("loading model: %w", err)
			}
		}
		var shutdown bool
		result, err, shutdown = f(c, ctx, prompt)
		if shutdown {
			c.close()
			c = nil
		}
		return result, err
	case <-ctx.Done():
		return result, ctx.Err()
	}
}

type modelConn struct {
	host      *Host
	cancel    context.CancelFunc
	cmd       *exec.Cmd
	stdin     io.WriteCloser
	stdout    io.ReadCloser
	idleTimer *time.Timer
	lastCall  time.Time
	bufData   []byte
}

func newConn(host *Host) (c *modelConn, err error) {
	ctx, cancel := context.WithCancel(context.Background())
	c = &modelConn{host: host, cancel: cancel}
	c.cmd = exec.CommandContext(ctx, host.cfg.BinaryPath)
	c.stdin, err = c.cmd.StdinPipe()
	if err != nil {
		return nil, err
	}
	c.stdout, err = c.cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}
	c.cmd.Stderr = os.Stderr
	c.cmd.Cancel = func() error {
		if c.cmd.Process != nil {
			c.cmd.Process.Signal(syscall.SIGUSR1)
		}
		return c.stdin.Close()
	}
	c.cmd.WaitDelay = gracePeriod
	if err := c.cmd.Start(); err != nil {
		return nil, err
	}
	req := &spb.ModelConfigRequest{ModelDir: host.cfg.DataDir}
	resp := &spb.ModelConfigResponse{}
	if err := c.communicate(spb.ModelFunction_MODEL_FUNCTION_CONFIG, req, resp); err != nil || resp.Error != "" {
		if resp.Error != "" {
			err = fmt.Errorf("model: %s", resp.Error)
		}
		err = errors.Join(err, c.close())
		return nil, err
	}
	c.lastCall = time.Now()
	c.idleTimer = time.NewTimer(maxIdleTime)
	go c.idleCheck(ctx)
	return c, nil
}

func (c *modelConn) close() error {
	c.cancel()
	return c.cmd.Wait()
}

func (c *modelConn) idleCheck(ctx context.Context) {
	for {
		select {
		case <-c.idleTimer.C:
			activeConn := <-c.host.conn
			if activeConn != c {
				c.host.conn <- activeConn
				return // it's gone already
			}
			idleTime := time.Since(c.lastCall)
			if idleTime >= maxIdleTime {
				c.host.conn <- nil
				c.close()
				return
			}
			c.idleTimer.Reset(maxIdleTime - idleTime)
			c.host.conn <- c
			// continue waiting
		case <-ctx.Done():
			if !c.idleTimer.Stop() {
				<-c.idleTimer.C
			}
			return
		}
	}
}

func (c *modelConn) countTokens(ctx context.Context, prompt string) (tokens int, err error, shutdown bool) {
	req := &spb.ModelTokenCountRequest{Prompt: prompt}
	resp := &spb.ModelTokenCountResponse{}
	if err, shutdown := c.call(ctx, spb.ModelFunction_MODEL_FUNCTION_TOKEN_COUNT, req, resp); err != nil {
		return 0, err, shutdown
	}
	return int(resp.TokenCount), nil, false
}

func (c *modelConn) generate(ctx context.Context, prompt string) (generated string, err error, shutdown bool) {
	req := &spb.ModelGenerateRequest{Prompt: prompt}
	resp := &spb.ModelGenerateResponse{}
	if err, shutdown := c.call(ctx, spb.ModelFunction_MODEL_FUNCTION_GENERATE, req, resp); err != nil {
		return "", err, shutdown
	}
	return strings.TrimSpace(resp.Generated), nil, false
}

type messageWithError interface {
	GetError() string
	proto.Message
}

func (c *modelConn) call(ctx context.Context, fun spb.ModelFunction, req proto.Message, resp messageWithError) (err error, shutdown bool) {
	c.lastCall = time.Now()
	errCh := make(chan error, 1)
	go func() { errCh <- c.communicate(fun, req, resp) }()
	select {
	case err := <-errCh:
		if err != nil {
			return err, true
		} else if errStr := resp.GetError(); errStr != "" {
			return fmt.Errorf("model error: %s", errStr), false
		}
		return nil, false
	case <-ctx.Done():
		c.cmd.Process.Signal(syscall.SIGUSR1)
		select {
		case err := <-errCh:
			if err != nil {
				shutdown = true
			}
		case <-time.After(gracePeriod):
			c.cmd.Process.Kill()
			shutdown = true
		}
		return ctx.Err(), shutdown
	}
}

func (c *modelConn) communicate(fun spb.ModelFunction, req, resp proto.Message) (err error) {
	mo := proto.MarshalOptions{}
	reqSize := mo.Size(req)
	mo.UseCachedSize = true
	buf := c.buf(8 + reqSize)[:8]
	binary.LittleEndian.PutUint32(buf[0:4], uint32(fun))
	binary.LittleEndian.PutUint32(buf[4:8], uint32(reqSize))
	// TODO: seems like this may not be reusing the same buffer, either figure out why or put it back to bufData
	if buf, err = mo.MarshalAppend(buf, req); err != nil {
		return fmt.Errorf("marshalling to model: %w", err)
	}
	if _, err := c.stdin.Write(buf); err != nil {
		return fmt.Errorf("writing to model: %w", err)
	}

	buf = c.buf(4)
	if _, err := io.ReadFull(c.stdout, buf); err != nil {
		return fmt.Errorf("reading header from model: %w", err)
	}
	respSize := int(binary.LittleEndian.Uint32(buf))
	buf = c.buf(respSize)
	if _, err := io.ReadFull(c.stdout, buf); err != nil {
		return fmt.Errorf("reading body from model: %w", err)
	}
	if err := proto.Unmarshal(buf, resp); err != nil {
		return fmt.Errorf("unmarshalling from model: %w", err)
	}

	return nil
}

func (c *modelConn) buf(n int) []byte {
	if n > maxBufSize {
		panic(fmt.Sprintf("maximum model connection buffer size exceeded: %d > %d", n, maxBufSize))
	}
	if n > len(c.bufData) {
		c.bufData = append(c.bufData, make([]byte, n-len(c.bufData))...)
	}
	return c.bufData[:n]
}
