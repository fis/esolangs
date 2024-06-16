// Package flow implements the business logic of generating a response for a question.
package flow

import (
	"context"
	"fmt"
	"regexp"
	"strings"

	"github.com/fis/esolangs/esogem/server/wiki"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

const (
	maxPromptSize    = 3072 // TODO: figure out exact value
	numWikiResults   = 3    // maximum number of pages per search term
	minSearchTermLen = 2    // minimum number of runes in a search term
	maxSearchTermLen = 64   // maximum number of runes in a search term
)

type Flow struct {
	model      Model
	wikiSource WikiSource
	query      string
	log        *spb.LogEntry
	background articleList
}

type Model interface {
	CountTokens(ctx context.Context, prompt string) (int, error)
	Generate(ctx context.Context, prompt string, logEntry *spb.LogEntry) (string, error)
}

type WikiSource interface {
	Search(ctx context.Context, query string, kind wiki.SearchType, maxResults int) ([]wiki.PageID, error)
	Get(ctx context.Context, pageID int) (wiki.Page, error)
}

func New(model Model, wikiSource WikiSource, query string, logEntry *spb.LogEntry) *Flow {
	return &Flow{model: model, wikiSource: wikiSource, query: query, log: logEntry}
}

func (fl *Flow) Respond(ctx context.Context) (string, error) {
	searchTerms, err := fl.generateSearchTerms(ctx)
	if err != nil {
		return "", fmt.Errorf("extracting search terms: %w", err)
	}
	fl.log.SearchTerms = searchTerms

	prompt := makeResponsePrompt(fl.query, nil)
	promptSize, err := fl.model.CountTokens(ctx, prompt)
	if err != nil {
		return "", fmt.Errorf("counting tokens: %w", err)
	}

	// TODO: potentially generate extra search terms with the other prompt if needed
augmentPrompt:
	for _, term := range searchTerms {
		if promptSize >= maxPromptSize-100 {
			break // don't even bother trying
		}
		// TODO: get a few more results than needed, just because some might be empty (redirect pages)
		// TODO: filter out pages that are mere redirects
		pages, err := fl.wikiSource.Search(ctx, term, wiki.SearchTitle, numWikiResults)
		if err != nil {
			return "", fmt.Errorf("searching wiki for %q: %w", term, err)
		}
		if len(pages) < numWikiResults {
			extraTitles, err := fl.wikiSource.Search(ctx, term, wiki.SearchText, numWikiResults-len(pages))
			if err != nil {
				return "", fmt.Errorf("searching wiki for %q: %w", term, err)
			}
			pages = append(pages, extraTitles...)
		}
		for _, page := range pages {
			fl.log.SearchResults = append(fl.log.SearchResults, page.Title)
		}
		for _, page := range pages {
			if promptSize >= maxPromptSize-100 {
				break augmentPrompt
			} else if fl.background.hasPage(page.ID) {
				continue
			}
			content, err := fl.wikiSource.Get(ctx, page.ID)
			if err != nil {
				return "", fmt.Errorf("fetching wiki page %q (%d): %w", page.Title, page.ID, err)
			}
			newPrompt, newPromptSize, err := fl.addBackground(ctx, page, content, maxPromptSize-promptSize)
			if err != nil {
				return "", fmt.Errorf("augmenting prompt with wiki page %q (%d): %w", page.Title, page.ID, err)
			}
			if newPrompt != "" {
				prompt, promptSize = newPrompt, newPromptSize
			}
		}
	}
	for _, page := range fl.background {
		fl.log.UsedResults = append(fl.log.UsedResults, page.title)
	}

	resp, err := fl.model.Generate(ctx, prompt, fl.log)
	if err != nil {
		return "", fmt.Errorf("generating response: %w", err)
	}
	return resp, nil
}

var searchTermSeparator = regexp.MustCompile(`[\n,;]\s*(\d+[.)]|- )?`)
var badSearchTerm = regexp.MustCompile(`(?i)search term|(search|best) keyword`)

func (fl *Flow) generateSearchTerms(ctx context.Context) ([]string, error) {
	prompt := makeSearchPrompt(fl.query)
	resp, err := fl.model.Generate(ctx, prompt, fl.log)
	if err != nil {
		return nil, err
	}
	candidates := searchTermSeparator.Split(resp, -1)
	var terms []string
	for _, term := range candidates {
		if badSearchTerm.MatchString(term) {
			continue
		}
		term, _ = strings.CutPrefix(strings.TrimSpace(term), "- ")
		if len(term) >= minSearchTermLen && len(term) <= maxSearchTermLen {
			terms = append(terms, term)
		}
	}
	return terms, nil
}

func (fl *Flow) addBackground(ctx context.Context, pageID wiki.PageID, page wiki.Page, maxTokens int) (string, int, error) {
	newBackground := append(fl.background, article{pageID: pageID.ID, title: pageID.Title, content: page.Content()})
	prompt := makeResponsePrompt(fl.query, newBackground)
	promptSize, err := fl.model.CountTokens(ctx, prompt)
	if err != nil {
		return "", 0, err
	}
	// TODO: consider also compacting long pages to include more responses in the prompt, even if it technically fits
	if promptSize <= maxPromptSize {
		fl.background = newBackground
		return prompt, promptSize, nil
	}

	// TODO: try compacting using page sections first

	summaryPrompt := makeSummaryPrompt(fl.query, page.Content(), maxTokens)
	summary, err := fl.model.Generate(ctx, summaryPrompt, fl.log)
	if err != nil {
		return "", 0, err
	}

	newBackground[len(newBackground)-1].content = summary
	prompt = makeResponsePrompt(fl.query, newBackground)
	promptSize, err = fl.model.CountTokens(ctx, prompt)
	if err != nil {
		return "", 0, err
	}
	if promptSize <= maxPromptSize {
		fl.background = newBackground
		return prompt, promptSize, nil
	}

	return "", 0, nil // can't fit it in, so just ignore this page
}

type article struct {
	pageID  int
	title   string
	content string
}

type articleList []article

func (lst articleList) hasPage(pageID int) bool {
	for _, a := range lst {
		if a.pageID == pageID {
			return true
		}
	}
	return false
}

func makeSearchPrompt(query string) string {
	return fmt.Sprintf(`<start_of_turn>user
You are an assistant to help find information in an esoteric programming language wiki.
Your task is to find the search term most likely to return relevant results for a question.
The question is:

%s

What is the best keyword to find relevant results? Give only the answer with no formatting. <end_of_turn>
<start_of_turn>model
`, query)
}

func makeSummaryPrompt(query, article string, maxTokens int) string {
	// TODO: convert maxTokens to a length suggestion
	_ = maxTokens
	return fmt.Sprintf(`<start_of_turn>user
Your task is to summarize text but retain the information necessary to answer a question.
The question is:

%s

Keeping the preceding question in mind, summarize the following text in %d words or fewer:

%s <end_of_turn>
<start_of_turn>model
`, query, 500, article)
}

func makeResponsePrompt(query string, background []article) string {
	var sb strings.Builder
	sb.WriteString(`<start_of_turn>user
Your task is to answer questions on an IRC channel about esoteric programming languages.
`)

	if len(background) > 0 {
		sb.WriteString("Here is some background material:\n")
		for _, art := range background {
			sb.WriteString("\nTitle: ")
			sb.WriteString(art.title)
			sb.WriteString("\n\nArticle text:\n\n")
			sb.WriteString(art.content)
			sb.WriteByte('\n')
		}
	}

	fmt.Fprintf(&sb, `
Answer the following question:

%s

Answer the question using at most 100 words. <end_of_turn>
<start_of_turn>model
`, query)

	return sb.String()
}
