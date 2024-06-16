// Binary fakemodel implements the server/model protocol with fixed responses to test questions.
//
// See /api/api_test.go for the corresponding integration test.
package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"strings"

	"google.golang.org/protobuf/proto"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

func main() {
	if err := handle(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

const (
	testModelDir = "testdata"
)

func handle() error {
	configured := false
	for {
		fun, size, err := readRequestHeader()
		if err == io.EOF {
			return nil
		} else if err != nil {
			return err
		}
		switch fun {
		case spb.ModelFunction_MODEL_FUNCTION_CONFIG:
			req, resp := &spb.ModelConfigRequest{}, &spb.ModelConfigResponse{}
			if err := readProto(size, req); err != nil {
				return err
			}
			if req.ModelDir == testModelDir {
				configured = true
			} else if req.ModelDir != testModelDir {
				resp.Error = "bad model directory"
			}
			if err := writeProto(resp); err != nil {
				return err
			}
		case spb.ModelFunction_MODEL_FUNCTION_TOKEN_COUNT:
			req, resp := &spb.ModelTokenCountRequest{}, &spb.ModelTokenCountResponse{}
			if err := readProto(size, req); err != nil {
				return err
			}
			if configured {
				handleTokenCount(req, resp)
			} else {
				resp.Error = "model not yet configured"
			}
			if err := writeProto(resp); err != nil {
				return err
			}
		case spb.ModelFunction_MODEL_FUNCTION_GENERATE:
			req, resp := &spb.ModelGenerateRequest{}, &spb.ModelGenerateResponse{}
			if err := readProto(size, req); err != nil {
				return err
			}
			if configured {
				handleGenerate(req, resp)
			} else {
				resp.Error = "model not yet configured"
			}
			if err := writeProto(resp); err != nil {
				return err
			}
		}
	}
}

func handleTokenCount(req *spb.ModelTokenCountRequest, resp *spb.ModelTokenCountResponse) {
	resp.TokenCount = int32(strings.Count(req.Prompt, " ") + 1)
}

func handleGenerate(req *spb.ModelGenerateRequest, resp *spb.ModelGenerateResponse) {
	switch {
	case strings.Contains(req.Prompt, "find the search term most likely to return relevant results"):
		resp.Generated = "- Foo\n- Bar\n"
	case strings.Contains(req.Prompt, "Answer the following question:"):
		if strings.Contains(req.Prompt, "Article about Foo.") && strings.Contains(req.Prompt, "Article about Bar.") {
			resp.Generated = "Test answer.\n"
		} else {
			resp.Error = "missing expected background content"
		}
	default:
		resp.Error = "unrecognized test prompt"
		resp.Error = fmt.Sprintf("unrecognized test prompt: %q", req.Prompt)
	}
}

func readRequestHeader() (fun spb.ModelFunction, size int, err error) {
	var data [8]byte
	if _, err := io.ReadFull(os.Stdin, data[:]); err != nil {
		return 0, 0, err
	}
	u1, u2 := binary.LittleEndian.Uint32(data[0:4]), binary.LittleEndian.Uint32(data[4:8])
	return spb.ModelFunction(u1), int(u2), nil
}

func readProto(size int, req proto.Message) error {
	data := make([]byte, size)
	if _, err := io.ReadFull(os.Stdin, data); err != nil {
		return err
	}
	return proto.Unmarshal(data, req)
}

func writeProto(resp proto.Message) (err error) {
	mo := proto.MarshalOptions{}
	size := mo.Size(resp)
	mo.UseCachedSize = true
	buf := make([]byte, 4, 4+size)
	binary.LittleEndian.PutUint32(buf[0:4], uint32(size))
	if buf, err = mo.MarshalAppend(buf, resp); err != nil {
		return err
	}
	if _, err := os.Stdout.Write(buf); err != nil {
		return err
	}
	return nil
}
