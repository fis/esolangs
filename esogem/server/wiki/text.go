package wiki

import (
	"fmt"
	"io"
	"iter"
	"slices"
	"strings"
	"unicode"

	"golang.org/x/net/html"
)

func extractText(htmlText string) ([]Section, error) {
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
	unrollDivs(root)

	var page []Section
	level, title, content := 0, "", ([]Paragraph)(nil)
	for c := root.FirstChild; c != nil; c = c.NextSibling {
		if l := headerLevel(c); l > 0 {
			if title != "" || len(content) > 0 {
				page = append(page, Section{Title: title, Level: level, Content: content})
				content = nil
			}
			level, title = l, nodeText(c)
			continue
		}
		if p, err := asParagraph(c); err != nil {
			return nil, err
		} else if p != nil {
			content = append(content, p)
		}
	}
	if title != "" || len(content) > 0 {
		page = append(page, Section{Title: title, Level: level, Content: content})
		content = nil
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

func unrollDivs(n *html.Node) {
	hasChildDivs := false
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		unrollDivs(c)
		if c.Type == html.ElementNode && c.Data == `div` {
			hasChildDivs = true
		}
	}
	if !hasChildDivs {
		return
	}
	var flattened []*html.Node
	for c, nc, i := n.FirstChild, (*html.Node)(nil), 0; c != nil; c, i = nc, i+1 {
		nc = c.NextSibling
		n.RemoveChild(c)
		if c.Type == html.ElementNode && c.Data == `div` {
			for gc, gnc := c.FirstChild, (*html.Node)(nil); gc != nil; gc = gnc {
				gnc = gc.NextSibling
				c.RemoveChild(gc)
				flattened = append(flattened, gc)
			}
		} else {
			flattened = append(flattened, c)
		}
	}
	for _, c := range flattened {
		n.InsertBefore(c, nil)
	}
}

func childrenByTag(n *html.Node, tag string) iter.Seq[*html.Node] {
	return func(yield func(*html.Node) bool) {
		yieldChildrenByTag(yield, n, tag)
	}
}

func yieldChildrenByTag(yield func(*html.Node) bool, n *html.Node, tag string) bool {
	if n.Type == html.ElementNode {
		if n.Data == tag {
			if !yield(n) {
				return false
			}
		} else {
			for c := n.FirstChild; c != nil; c = c.NextSibling {
				if !yieldChildrenByTag(yield, c, tag) {
					return false
				}
			}
		}
	}
	return true
}

func childrenByTags(n *html.Node, tags ...string) iter.Seq[*html.Node] {
	return func(yield func(*html.Node) bool) {
		yieldChildrenByTags(yield, n, tags...)
	}
}

func yieldChildrenByTags(yield func(*html.Node) bool, n *html.Node, tags ...string) bool {
	if n.Type == html.ElementNode {
		if slices.Contains(tags, n.Data) {
			if !yield(n) {
				return false
			}
		} else {
			for c := n.FirstChild; c != nil; c = c.NextSibling {
				if !yieldChildrenByTags(yield, c, tags...) {
					return false
				}
			}
		}
	}
	return true
}

func headerLevel(n *html.Node) int {
	if n.Type == html.ElementNode && len(n.Data) == 2 && n.Data[0] == 'h' && n.Data[1] >= '1' && n.Data[1] <= '6' {
		return int(n.Data[1] - '0')
	}
	return 0
}

func asParagraph(n *html.Node) (Paragraph, error) {
	switch n.Type {
	case html.ElementNode:
		switch n.Data {
		case `p`:
			return TextParagraph(nodeText(n)), nil
		case `pre`:
			var text strings.Builder
			if finalNL := preText(&text, n); !finalNL {
				text.WriteByte('\n')
			}
			return PreformattedParagraph(text.String()), nil
		case `ul`, `ol`:
			p := ListParagraph{Ordered: n.Data == `ol`}
			for li := range childrenByTag(n, `li`) {
				p.Items = append(p.Items, nodeText(li))
			}
			return &p, nil
		case `dl`:
			p := ListParagraph{Ordered: false}
			var currentTerm wsNormalizer
			for di := range childrenByTags(n, `dt`, `dd`) {
				switch di.Data {
				case `dt`:
					if currentTerm.text.Len() > 0 {
						p.Items = append(p.Items, currentTerm.text.String())
						currentTerm.reset()
					}
					childText(&currentTerm, di)
					currentTerm.writeString(": ")
				case `dd`:
					childText(&currentTerm, di)
					currentTerm.writeRune(' ')
				}
			}
			if currentTerm.text.Len() > 0 {
				p.Items = append(p.Items, currentTerm.text.String())
			}
			return &p, nil
		case `table`:
			var p TableParagraph
			for tr := range childrenByTag(n, `tr`) {
				var row []string
				for td := range childrenByTags(tr, `th`, `td`) {
					row = append(row, nodeText(td))
				}
				p = append(p, row)
			}
			return p, nil
		case `figure`:
			var buf wsNormalizer
			buf.writeString("[")
			for cap := range childrenByTag(n, `figcaption`) {
				childText(&buf, cap)
			}
			buf.writeString("]\n")
			return TextParagraph(buf.text.String()), nil
		default:
			return nil, fmt.Errorf("unexpected top-level element: %s", n.Data)
		}
	default:
		return nil, nil
	}
}

func nodeText(n *html.Node) string {
	var buf wsNormalizer
	switch n.Type {
	case html.TextNode:
		buf.writeString(n.Data)
	case html.ElementNode:
		childText(&buf, n)
	}
	return buf.text.String()
}

func childText(out *wsNormalizer, n *html.Node) {
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		switch c.Type {
		case html.TextNode:
			out.writeString(c.Data)
		case html.ElementNode:
			switch c.Data {
			case `p`, `td`, `th`, `li`:
				out.writeRune(' ')
				childText(out, c)
				out.writeRune(' ')
			case `b`, `strong`:
				out.writeRune('*')
				childText(out, c)
				out.writeRune('*')
			case `i`, `em`:
				out.writeRune('_')
				childText(out, c)
				out.writeRune('_')
			case `code`:
				out.writeRune('`')
				childText(out, c)
				out.writeRune('`')
			default:
				childText(out, c)
			}
		}
	}
}

func preText(out *strings.Builder, n *html.Node) (finalNL bool) {
	for c := n.FirstChild; c != nil; c = c.NextSibling {
		switch c.Type {
		case html.TextNode:
			out.WriteString(c.Data)
			finalNL = strings.HasSuffix(c.Data, "\n")
		case html.ElementNode:
			finalNL = preText(out, c)
		}
	}
	return finalNL
}

type Page struct {
	Title string
	Text  []Section
}

func (p *Page) Content() string {
	var buf strings.Builder
	p.WriteMarkdown(&buf)
	return buf.String()
}

func (p *Page) WriteMarkdown(buf Buffer) {
	fmt.Fprintf(buf, "# %s\n", p.Title)
	for _, sec := range p.Text {
		buf.WriteByte('\n')
		sec.WriteMarkdown(buf)
	}
}

type Section struct {
	Title   string
	Level   int
	Content []Paragraph
}

func (sec *Section) WriteMarkdown(buf Buffer) {
	if sec.Title != "" && sec.Level > 0 {
		for range sec.Level {
			buf.WriteByte('#')
		}
		fmt.Fprintf(buf, " %s\n", sec.Title)
		if len(sec.Content) > 0 {
			buf.WriteByte('\n')
		}
	}
	for i, para := range sec.Content {
		if i > 0 {
			buf.WriteByte('\n')
		}
		para.WriteMarkdown(buf)
	}
}

type Paragraph interface {
	WriteMarkdown(buf Buffer)
}

type TextParagraph string

func (tp TextParagraph) WriteMarkdown(buf Buffer) {
	buf.WriteString(string(tp))
	buf.WriteByte('\n')
}

type PreformattedParagraph string

func (pp PreformattedParagraph) WriteMarkdown(buf Buffer) {
	buf.WriteString("```\n")
	buf.WriteString(string(pp))
	buf.WriteString("```\n")
}

type ListParagraph struct {
	Ordered bool
	Items   []string
}

func (lp *ListParagraph) WriteMarkdown(buf Buffer) {
	for i, it := range lp.Items {
		if lp.Ordered {
			fmt.Fprintf(buf, "%d. ", i+1)
		} else {
			buf.WriteString("- ")
		}
		buf.WriteString(it)
		buf.WriteByte('\n')
	}
}

type TableParagraph [][]string

func (tp TableParagraph) WriteMarkdown(buf Buffer) {
	for _, row := range tp {
		for i, cell := range row {
			if i > 0 {
				buf.WriteString(` | `)
			}
			buf.WriteString(cell)
		}
		buf.WriteByte('\n')
	}
}

type Buffer interface {
	io.Writer
	io.ByteWriter
	io.StringWriter
}

type wsNormalizer struct {
	text  strings.Builder
	state wsState
}

type wsState int

const (
	leadingWS wsState = iota
	inWS
	inNonWS
)

func (wsn *wsNormalizer) reset() {
	wsn.text.Reset()
	wsn.state = leadingWS
}

func (wsn *wsNormalizer) writeString(s string) {
	for _, r := range s {
		wsn.writeRune(r)
	}
}

func (wsn *wsNormalizer) writeRune(r rune) {
	if unicode.IsSpace(r) {
		if wsn.state == inNonWS {
			wsn.state = inWS
		}
	} else {
		if wsn.state == inWS {
			wsn.text.WriteByte(' ')
		}
		wsn.state = inNonWS
		wsn.text.WriteRune(r)
	}
}
