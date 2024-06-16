package api

import (
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

func TestHandleQuestion(t *testing.T) {
	ts := httptest.NewServer(testHandler{})
	defer ts.Close()

	cfg := &spb.Config{
		Model: &spb.ModelConfig{
			BinaryPath: "testdata/fakemodel",
			DataDir:    "testdata", // unused, just checked by fakemodel
		},
		Wiki: &spb.WikiConfig{Url: ts.URL + "/api.php"},
	}
	mux := http.NewServeMux()
	if err := AddHandler(mux, cfg); err != nil {
		t.Fatal(err)
	}

	w := httptest.NewRecorder()
	req := httptest.NewRequest("POST", "https://example.com/ask", strings.NewReader("Test question?"))
	req.Header.Add("Content-Type", "application/octet-stream")
	mux.ServeHTTP(w, req)

	got := w.Result()
	want := "Test answer."
	if got.StatusCode != 200 {
		got.Write(os.Stderr)
		t.Errorf("unexpected non-200 response status: %s", got.Status)
	} else if body, _ := io.ReadAll(got.Body); string(body) != want {
		t.Errorf("unexpected 200 response body: %q", body)
	}
}

type testHandler struct{}

var testResponses = map[string]string{
	`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=Foo&srwhat=title&utf8=`:  `{"query":{"search":[{"title":"Foo","pageid":1}]}}`,
	`/api.php?action=query&format=json&list=search&srlimit=2&srnamespace=0&srprop=&srsearch=Foo&srwhat=text&utf8=`:   `{}`,
	`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=Bar&srwhat=title&utf8=`:  `{"query":{"search":[{"title":"Bar","pageid":2}]}}`,
	`/api.php?action=query&format=json&list=search&srlimit=2&srnamespace=0&srprop=&srsearch=Bar&srwhat=text&utf8=`:   `{}`,
	`/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=1&prop=text&utf8=`: `{"parse":{"text":{"*":"<div><p>Article about Foo.</p></div>"}}}`,
	`/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=2&prop=text&utf8=`: `{"parse":{"text":{"*":"<div><p>Article about Bar.</p></div>"}}}`,
}

func (testHandler) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	if resp, ok := testResponses[req.URL.String()]; ok {
		w.Write([]byte(resp))
		return
	}
	// TODO remove, just for debugging
	fmt.Fprintf(os.Stderr, "unimplemented endpoint: %s?%s\n", req.URL.Path, req.URL.RawQuery)
	http.Error(w, "unimplemented", http.StatusInternalServerError)
}
