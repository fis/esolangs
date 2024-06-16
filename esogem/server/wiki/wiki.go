// Package wiki contains a client for the MediaWiki API.
package wiki

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strconv"
	"strings"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

type Client struct {
	apiEndpoint string
	namespaces  string
}

func NewClient(cfg *spb.WikiConfig) *Client {
	// TODO: test-parse URL, throw error
	// TODO: configure cache
	namespaces := cfg.Namespaces
	if namespaces == "" {
		namespaces = "0"
	}
	return &Client{
		apiEndpoint: cfg.Url,
		namespaces:  namespaces,
	}
}

type SearchType int

const (
	SearchTitle SearchType = iota
	SearchText
)

func (c *Client) Search(ctx context.Context, query string, kind SearchType, maxResults int) ([]PageID, error) {
	req := url.Values{}
	req.Add("action", "query")
	req.Add("list", "search")
	req.Add("srsearch", query)
	req.Add("srnamespace", c.namespaces)
	req.Add("srlimit", strconv.Itoa(maxResults))
	if kind == SearchTitle {
		req.Add("srwhat", "title")
	} else {
		req.Add("srwhat", "text")
	}
	req.Add("srprop", "")
	// TODO: include timestamp in srprop, make a cache for Get
	resp := &struct {
		Query struct {
			Results []PageID `json:"search"`
		} `json:"query"`
	}{}
	if err := c.apiCall(ctx, req, resp); err != nil {
		return nil, err
	}
	return resp.Query.Results, nil
}

func (c *Client) Get(ctx context.Context, pageID int) (Page, error) {
	req := url.Values{}
	req.Add("action", "parse")
	req.Add("pageid", strconv.Itoa(pageID))
	req.Add("prop", "text")
	req.Add("disablelimitreport", "")
	req.Add("disableeditsection", "")
	req.Add("disabletoc", "")
	// TODO: add "mobileformat" once it's supported
	resp := &struct {
		Parse struct {
			Text struct {
				Content string `json:"*"`
			} `json:"text"`
		} `json:"parse"`
	}{}
	if err := c.apiCall(ctx, req, resp); err != nil {
		return nil, err
	}
	page, err := extractText(resp.Parse.Text.Content)
	if err != nil {
		return nil, fmt.Errorf("wiki text extraction: %w", err)
	}
	return page, nil
}

func (c *Client) apiCall(ctx context.Context, args url.Values, respData any) error {
	args.Add("utf8", "")
	args.Add("format", "json")
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, c.apiEndpoint+"?"+args.Encode(), nil)
	if err != nil {
		return fmt.Errorf("request: %w", err)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return fmt.Errorf("http: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		// TODO: log wiki error response bodies?
		return fmt.Errorf("http: %s", resp.Status)
	}
	if err := json.NewDecoder(resp.Body).Decode(respData); err != nil {
		return fmt.Errorf("json: %w", err)
	}
	return nil
}

type PageID struct {
	ID    int    `json:"pageid"`
	Title string `json:"title"`
}

type Page []Section

type Section struct {
	Title   string
	Content string
}

func (p Page) Content() string {
	text := strings.Builder{}
	for _, s := range p {
		if s.Title != "" {
			text.WriteString("# ")
			text.WriteString(s.Title)
			text.WriteByte('\n')
		}
		text.WriteString(s.Content)
		text.WriteByte('\n')
	}
	return text.String()
}
