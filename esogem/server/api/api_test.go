package api

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"testing"

	spb "github.com/fis/esolangs/esogem/server/proto"
)

const testModelName = "gemma3:4b"

func TestHandleQuestion(t *testing.T) {
	th := &testHandler{}
	defer th.checkUnexpected(t)
	ts := testServer(th)
	defer ts.Close()

	cfg := &spb.Config{
		Model: &spb.ModelConfig{
			TokenizerSpm: "testdata/tokenizer.spm",
			OllamaUrl:    ts.URL + "/api/generate",
			ModelName:    testModelName,
			ContextSize:  4096,
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

func testServer(th *testHandler) *httptest.Server {
	mux := http.NewServeMux()
	mux.Handle("/", th)
	mux.HandleFunc("POST /api/generate", fakeModel)
	return httptest.NewServer(mux)
}

type testHandler struct {
	unexpected []string
}

func (th *testHandler) checkUnexpected(t *testing.T) {
	t.Helper()
	for _, path := range th.unexpected {
		t.Errorf("unexpected HTTP request: %s", path)
	}
}

var testResponses = map[string]string{
	`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=Foo&srwhat=title&utf8=`:  `{"query":{"search":[{"title":"Foo","pageid":1}]}}`,
	`/api.php?action=query&format=json&list=search&srlimit=2&srnamespace=0&srprop=&srsearch=Foo&srwhat=text&utf8=`:   `{}`,
	`/api.php?action=query&format=json&list=search&srlimit=3&srnamespace=0&srprop=&srsearch=Bar&srwhat=title&utf8=`:  `{"query":{"search":[{"title":"Bar","pageid":2}]}}`,
	`/api.php?action=query&format=json&list=search&srlimit=2&srnamespace=0&srprop=&srsearch=Bar&srwhat=text&utf8=`:   `{}`,
	`/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=1&prop=text&utf8=`: `{"parse":{"text":{"*":"<div><p>Article about Foo.</p></div>"}}}`,
	`/api.php?action=parse&disableeditsection=&disablelimitreport=&disabletoc=&format=json&pageid=2&prop=text&utf8=`: `{"parse":{"text":{"*":"<div><p>Article about Bar.</p></div>"}}}`,
}

func (th *testHandler) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	path := req.URL.String()
	if resp, ok := testResponses[path]; ok {
		w.Write([]byte(resp))
		return
	}
	th.unexpected = append(th.unexpected, path)
	http.Error(w, "unimplemented", http.StatusInternalServerError)
}

func fakeModel(w http.ResponseWriter, req *http.Request) {
	var reqData struct {
		Model  string `json:"model"`
		Prompt string `json:"prompt"`
		Stream bool   `json:"stream"`
		Think  bool   `json:"think"`
	}
	if err := json.NewDecoder(req.Body).Decode(&reqData); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
	} else if reqData.Model != testModelName {
		http.Error(w, fmt.Sprintf("unknown model: %s", reqData.Model), http.StatusBadRequest)
	}
	generated, errorMsg := "", ""
	switch prompt := reqData.Prompt; {
	case strings.Contains(prompt, "find the search term most likely to return relevant results"):
		generated = "- Foo\n- Bar\n"
	case strings.Contains(prompt, "Answer the following question:"):
		if strings.Contains(prompt, "Article about Foo.") && strings.Contains(prompt, "Article about Bar.") {
			generated = "Test answer.\n"
		} else {
			errorMsg = "missing expected background content"
		}
	default:
		errorMsg = fmt.Sprintf("unrecognized test prompt: %q", prompt)
	}
	if errorMsg != "" {
		http.Error(w, errorMsg, http.StatusBadRequest)
	} else {
		respData := struct {
			Model    string `json:"model"`
			Response string `json:"response"`
			Done     bool   `json:"done"`
		}{
			Model:    testModelName,
			Response: generated,
			Done:     true,
		}
		w.Header().Set("Content-Type", `application/json`)
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(&respData)
	}
}
