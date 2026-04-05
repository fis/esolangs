// Binary generate can be used to test-drive the model host binary.
//
// It's also convenient for generating multiple answers for the same prompt.
//
// The instruction-tuned turn indicators are added around the prompt automatically.
// The output is forced to be on one line by replacing newlines with a " / " sequence.
package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"strings"

	"github.com/fis/esolangs/esogem/server/model"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

var (
	tokenizerFile = flag.String("tokenizer", "", "tokenizer.spm file name [required]")
	ollamaURL     = flag.String("ollama", "http://localhost:11434/api/generate", "Ollama API endpoint address")
	modelName     = flag.String("model", "gemma3:4b", "model name ")
	outputCount   = flag.Int("n", 1, "number of times to generate output for the same prompt")
)

func main() {
	flag.Parse()
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	if *tokenizerFile == "" {
		return fmt.Errorf("must provide --tokenizer=.../tokenizer.spm option")
	}
	if flag.NArg() < 1 {
		return fmt.Errorf("must provide at least one non-option argument to use as a prompt")
	}

	modelHost, err := model.NewHost(&spb.ModelConfig{TokenizerSpm: *tokenizerFile, OllamaUrl: *ollamaURL, ModelName: *modelName, ContextSize: 4096})
	if err != nil {
		return err
	}

	for _, prompt := range flag.Args() {
		for range *outputCount {
			generated, err := modelHost.Generate(context.Background(), prompt, &spb.LogEntry{})
			if err != nil {
				return err
			}
			generated = strings.TrimSpace(generated)
			generated = strings.ReplaceAll(generated, "\n", " / ")
			fmt.Println(generated)
		}
	}

	return nil
}
