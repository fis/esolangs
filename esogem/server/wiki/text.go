package wiki

import (
	"fmt"
	"strings"

	"golang.org/x/net/html"
)

func extractText(htmlText string) (Page, error) {
	nodes, err := html.ParseFragment(strings.NewReader(htmlText), nil)
	if err != nil {
		return nil, err
	} else if len(nodes) != 1 {
		// TODO: when returning an unmet expectation, summarize what we got instead
		return nil, fmt.Errorf("expected one node")
	}

	root := findFirstDiv(nodes[0])
	if root == nil {
		return nil, fmt.Errorf("missing <div>")
	}

	var page Page
	title, content := "", strings.Builder{}
	for c := root.FirstChild; c != nil; c = c.NextSibling {
		if c.Type == html.ElementNode && c.Data == `h2` {
			if content.Len() > 0 {
				page = append(page, Section{Title: title, Content: strings.TrimSpace(content.String())})
				content.Reset()
			}
			nodeText(&content, c)
			title = strings.TrimSpace(content.String())
			content.Reset()
			continue
		}
		nodeText(&content, c)
	}
	if content.Len() > 0 {
		page = append(page, Section{Title: title, Content: strings.TrimSpace(content.String())})
	}

	return page, nil
}

func findFirstDiv(n *html.Node) *html.Node {
	if n.Type == html.ElementNode && n.Data == `div` {
		return n
	}
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		if found := findFirstDiv(c); found != nil {
			return found
		}
	}
	return nil
}

func childText(out *strings.Builder, n *html.Node) {
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		nodeText(out, c)
	}
}

func nodeText(out *strings.Builder, n *html.Node) {
	switch n.Type {
	case html.TextNode:
		for _, r := range n.Data {
			if r != '\n' {
				out.WriteRune(r)
			}
		}
	case html.ElementNode:
		// TODO: find other elements common in generated wikitext that need special handling
		// at least <ol> might be nice
		switch n.Data {
		case `p`:
			childText(out, n)
			out.WriteByte('\n')
		case `ul`:
			for c := n.FirstChild; c != nil; c = c.NextSibling {
				if c.Type == html.ElementNode && c.Data == `li` {
					out.WriteString("- ")
					nodeText(out, c)
					out.WriteByte('\n')
				}
			}
		case `pre`:
			preText(out, n)
		default:
			childText(out, n)
		}
	}
}

func preText(out *strings.Builder, n *html.Node) {
	switch n.Type {
	case html.TextNode:
		out.WriteString(n.Data)
	case html.ElementNode:
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			preText(out, c)
		}
	}
}
