// Package model handles interfacing with the model-hosting binary.
package model

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"time"

	"github.com/eliben/go-sentencepiece"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

const (
	maxIdleTime = 30 * time.Minute // if idle longer than this, unload the model
)

type Host struct {
	cfg       *spb.ModelConfig
	tokenizer *sentencepiece.Processor
}

func NewHost(cfg *spb.ModelConfig) (*Host, error) {
	if err := validateConfig(cfg); err != nil {
		return nil, fmt.Errorf("config validation: %w", err)
	}
	tokenizer, err := sentencepiece.NewProcessorFromPath(cfg.GetTokenizerSpm())
	if err != nil {
		return nil, fmt.Errorf("failed to create tokenizer: %w", err)
	}
	h := &Host{
		cfg:       cfg,
		tokenizer: tokenizer,
	}
	return h, nil
}

func validateConfig(cfg *spb.ModelConfig) error {
	if _, err := os.Stat(cfg.GetTokenizerSpm()); err != nil {
		return fmt.Errorf("stat tokenizer: %w", err)
	}
	if cfg.GetOllamaUrl() == "" {
		return fmt.Errorf("missing required config: model.ollama_url")
	}
	if cfg.GetModelName() == "" {
		return fmt.Errorf("missing required config: model.model_name")
	}
	if cfg.GetContextSize() == 0 {
		return fmt.Errorf("missing required config: model.context_size")
	}
	return nil
}

func (h *Host) CountTokens(ctx context.Context, prompt string) int {
	tokens := h.tokenizer.Encode(prompt)
	return len(tokens) + 1 // account for BOS token
}

func (h *Host) Generate(ctx context.Context, prompt string, logEntry *spb.LogEntry) (string, error) {
	reqData := struct {
		Model            string `json:"model"`
		Prompt           string `json:"prompt"`
		Stream           bool   `json:"stream"`
		Think            bool   `json:"think"`
		KeepAliveSeconds int    `json:"keep_alive"`
		// TODO: options.num_ctx, maybe options.num_predict
	}{
		Model:            h.cfg.GetModelName(),
		Prompt:           prompt,
		Stream:           false,
		Think:            false,
		KeepAliveSeconds: int(maxIdleTime / time.Second),
	}
	reqBytes, err := json.Marshal(&reqData)
	if err != nil {
		return "", fmt.Errorf("encode request body: %w", err)
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, h.cfg.GetOllamaUrl(), bytes.NewReader(reqBytes))
	if err != nil {
		return "", fmt.Errorf("build request: %w", err)
	}
	req.Header.Set("Content-Type", `application/json`)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", fmt.Errorf("http: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("http: %s", resp.Status)
	}
	var respData struct {
		Response string `json:"response"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&respData); err != nil {
		return "", fmt.Errorf("decode response body: %w", err)
	}
	logEntry.Generations = append(logEntry.Generations, &spb.GenerationEntry{Prompt: prompt, Response: respData.Response})
	return respData.Response, nil
}
