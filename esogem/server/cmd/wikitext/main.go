// Binary wikitext runs the wiki-to-plaintext conversion code on a wiki page.
package main

import (
	"context"
	"fmt"
	"os"
	"strconv"

	"github.com/fis/esolangs/esogem/server/wiki"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

func main() {
	for _, idArg := range os.Args[1:] {
		id, err := strconv.Atoi(idArg)
		if err != nil {
			fmt.Fprintf(os.Stderr, "%s: not a number\n", idArg)
			continue
		}
		if err := printText(id); err != nil {
			fmt.Fprintln(os.Stderr, err)
		}
	}
}

func printText(pageID int) error {
	wc := wiki.NewClient(&spb.WikiConfig{Url: "https://esolangs.org/w/api.php"})
	page, err := wc.Get(context.Background(), pageID)
	if err != nil {
		return err
	}
	fmt.Print(page.Content())
	return nil
}
