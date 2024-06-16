// Binary logcat converts the request log to human-readable formats.
package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"strings"
	"time"

	spb "github.com/fis/esolangs/esogem/server/proto"
	"github.com/fis/esolangs/esogem/server/proto/delim"
)

var shortFormat = flag.Bool("short", false, "print requests in short one-line-per-request format")

func main() {
	flag.Parse()
	if err := logcat(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func logcat() error {
	writer := writeLong
	if *shortFormat {
		writer = writeShort
	}

	for _, path := range flag.Args() {
		r, err := delim.OpenReader(path)
		if err != nil {
			return err
		}
		defer r.Close()

		var (
			e   spb.LogEntry
			buf []byte
		)
		for {
			buf, err = r.Read(&e, buf)
			if err == io.EOF {
				break
			} else if err != nil {
				return err
			}
			writer(&e)
		}
	}

	return nil
}

func writeShort(e *spb.LogEntry) {
	fmt.Printf("%s: %s: %s", timeStamp(e.TimeUsec), e.Request, e.Question)
	if len(e.SearchTerms) > 0 {
		fmt.Printf(" | terms: %s", strings.Join(e.SearchTerms, "; "))
	}
	if len(e.SearchResults) > 0 {
		fmt.Printf(" | results: %s", strings.Join(e.SearchResults, "; "))
	}
	if len(e.UsedResults) > 0 {
		fmt.Printf(" | used: %s", strings.Join(e.UsedResults, "; "))
	}
	if e.Error != "" {
		fmt.Printf(" => ERR: %s", e.Error)
	}
	fmt.Println()
}

func writeLong(e *spb.LogEntry) {
	fmt.Printf("%s: %s: %s\n", timeStamp(e.TimeUsec), e.Request, e.Question)
	if len(e.SearchTerms) > 0 {
		fmt.Printf("- search terms: %s\n", strings.Join(e.SearchTerms, "; "))
	}
	if len(e.SearchResults) > 0 {
		fmt.Printf("- result pages: %s\n", strings.Join(e.SearchResults, "; "))
	}
	if len(e.UsedResults) > 0 {
		fmt.Printf("- used pages: %s\n", strings.Join(e.UsedResults, "; "))
	}
	if e.Error != "" {
		fmt.Printf("- ERROR: %s\n", e.Error)
	}
	for i, g := range e.Generations {
		fmt.Printf("===== model prompt %d:\n%s\n", i+1, strings.TrimSpace(g.Prompt))
		fmt.Printf("===== model response %d:\n%s\n", i+1, strings.TrimSpace(g.Response))
		fmt.Println("=====")
	}
	fmt.Println()
}

func timeStamp(timeUsec int64) string {
	return time.UnixMicro(timeUsec).UTC().Format("2006-01-02 15:04:05")
}
