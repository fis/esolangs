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
	modelBin    = flag.String("bin", "", "model host executable [required]")
	modelDir    = flag.String("model", "", "directory holding the model files [required]")
	outputCount = flag.Int("n", 1, "number of times to generate output for the same prompt")
)

func main() {
	flag.Parse()
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run() error {
	if *modelBin == "" || *modelDir == "" {
		return fmt.Errorf("must provide both --bin=X and --model=X options")
	}
	if flag.NArg() < 1 {
		return fmt.Errorf("must provide at least one non-option argument to use as a prompt")
	}

	modelHost, err := model.NewHost(&spb.ModelConfig{BinaryPath: *modelBin, DataDir: *modelDir})
	if err != nil {
		return err
	}

	for _, userPrompt := range flag.Args() {
		prompt := fmt.Sprintf("<start_of_turn>user\n%s <end_of_turn>\n<start_of_turn>model\n", userPrompt)
		generated, err := modelHost.Generate(context.Background(), prompt, &spb.LogEntry{})
		if err != nil {
			return err
		}
		generated = strings.ReplaceAll(generated, "\n", " / ")
		fmt.Println(generated)
	}

	return nil
}
