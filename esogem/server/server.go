// Binary server is the esogem HTTP server.
package main

import (
	"fmt"
	"net/http"
	"os"

	"github.com/fis/esolangs/esogem/server/api"
	"google.golang.org/protobuf/encoding/prototext"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

func main() {
	if err := start(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func start() error {
	if len(os.Args) != 2 {
		return fmt.Errorf("usage: %s config.textpb", os.Args[0])
	}
	cfg, err := loadConfig(os.Args[1])
	if err != nil {
		return err
	}
	// TODO: shutdown gracefully on signal
	if err := api.AddHandler(http.DefaultServeMux, cfg); err != nil {
		return err
	}
	return http.ListenAndServe(cfg.ServerAddr, nil)
}

func loadConfig(path string) (*spb.Config, error) {
	cfgData, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	cfg := &spb.Config{}
	if err := prototext.Unmarshal(cfgData, cfg); err != nil {
		return nil, fmt.Errorf("parse config: %w", err)
	}
	return cfg, nil
}
