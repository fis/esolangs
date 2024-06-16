package wiki_test

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"

	"github.com/fis/esolangs/esogem/server/wiki"
	"github.com/google/go-cmp/cmp"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

func TestSearch(t *testing.T) {
	ts := newTestServer(t, map[string]string{
		`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=title&utf8=`: "testdata/search.title.json",
		`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=befunge&srwhat=text&utf8=`:  "testdata/search.text.json",
	})
	// TODO: consider using t.Cleanup, but does that work for generating the errors?
	defer ts.finish(t)
	c := wiki.NewClient(&spb.WikiConfig{Url: ts.http.URL + "/api.php"})

	query := "befunge"
	tests := []struct {
		kind wiki.SearchType
		want []wiki.PageID
	}{
		{
			kind: wiki.SearchTitle,
			want: []wiki.PageID{
				{ID: 1005, Title: "Befunge"},
				{ID: 17044, Title: "Befunge colored"},
				{ID: 2459, Title: "Befunge-93"},
			},
		},
		{
			kind: wiki.SearchText,
			want: []wiki.PageID{
				{ID: 17942, Title: "StringSafunge"},
				{ID: 12159, Title: "Generic 2D Befunge"},
				{ID: 17875, Title: "4 esolang + Python polyglot"},
			},
		},
	}
	for _, test := range tests {
		if got, err := c.Search(context.Background(), query, test.kind, len(test.want)); err != nil {
			t.Errorf("Search(%s, %d): %v", query, test.kind, err)
		} else if !cmp.Equal(got, test.want) {
			t.Errorf("Search(%s, %d) = %#v, want %#v", query, test.kind, got, test.want)
		}
	}
}

func TestGet(t *testing.T) {
	ts := newTestServer(t, map[string]string{
		`/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=1003&prop=text&utf8=`: "testdata/get.json",
	})
	defer ts.finish(t)
	c := wiki.NewClient(&spb.WikiConfig{Url: ts.http.URL + "/api.php"})

	want, err := os.ReadFile("testdata/get.txt")
	if err != nil {
		t.Fatal(err)
	}

	pageID := 1003 // Iota
	if page, err := c.Get(context.Background(), pageID); err != nil {
		t.Errorf("Get(%d): %v", pageID, err)
	} else if got := page.Content(); got != string(want) {
		t.Errorf("Get(%d) = %s, want %s", pageID, got, want)
	}
}

type testServer struct {
	http      *httptest.Server
	responses map[string][]byte
	errors    []string
}

func newTestServer(t *testing.T, dataFiles map[string]string) *testServer {
	t.Helper()

	responses := map[string][]byte{}
	for req, file := range dataFiles {
		data, err := os.ReadFile(file)
		if err != nil {
			t.Fatal(err)
		}
		responses[req] = data
	}

	ts := &testServer{responses: responses}
	ts.http = httptest.NewServer(http.HandlerFunc(ts.handle))
	return ts
}

func (ts *testServer) finish(t *testing.T) {
	t.Helper()
	ts.http.Close()
	for _, errText := range ts.errors {
		t.Error(errText)
	}
}

func (ts *testServer) handle(w http.ResponseWriter, req *http.Request) {
	q := req.URL.String()
	resp, ok := ts.responses[q]
	if !ok {
		ts.errors = append(ts.errors, fmt.Sprintf("unexpected path: %s", q))
		http.Error(w, "unknown request", http.StatusNotFound)
		return
	}
	w.Header().Add("Content-Type", "text/json")
	w.Write(resp)
}
