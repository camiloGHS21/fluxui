package fluxui

// Headless tests for the Go DSL ecosystem features. These exercise pure logic
// (Store, Schema/Rule, Query, Skeleton, route param matching) and never create
// an App or mount widgets, so the native shared library is not required.

import (
	"testing"
	"time"
)

func TestStoreZustand(t *testing.T) {
	type CartState struct {
		Count int
		Last  string
	}
	cart := NewStore(CartState{})
	notified := 0
	cart.Subscribe(func() { notified++ })
	cart.Set(func(s *CartState) { s.Count++; s.Last = "apple" })
	if cart.Get().Count != 1 {
		t.Fatalf("expected count 1, got %d", cart.Get().Count)
	}
	if cart.Get().Last != "apple" {
		t.Fatalf("expected last apple, got %q", cart.Get().Last)
	}
	if notified != 1 {
		t.Fatalf("expected 1 notification, got %d", notified)
	}
}

func TestSchemaZod(t *testing.T) {
	schema := NewSchema().
		Field("email", RuleString().Email()).
		Field("age", RuleNumber().Min(18).Max(120)).
		Field("nick", RuleString().Optional().MaxLength(5))

	ok := schema.Validate(map[string]string{"email": "a@b.com", "age": "25", "nick": "joe"})
	if !ok.OK {
		t.Fatalf("expected valid, got errors %v", ok.Errors)
	}

	bad := schema.Validate(map[string]string{"email": "not-an-email", "age": "12"})
	if bad.OK {
		t.Fatal("expected invalid input to fail")
	}
	if _, has := bad.Errors["email"]; !has {
		t.Fatal("expected email error")
	}
	if _, has := bad.Errors["age"]; !has {
		t.Fatal("expected age error")
	}

	missing := schema.Validate(map[string]string{"age": "20"})
	if missing.OK {
		t.Fatal("expected missing required email to fail")
	}
	if _, has := missing.Errors["email"]; !has {
		t.Fatal("expected required email error")
	}
}

func TestQueryStates(t *testing.T) {
	q := NewQuery(func() (string, error) { return "hello", nil })
	if q.Status() != QueryIdle {
		t.Fatalf("expected idle, got %v", q.Status())
	}
	q.Start()
	// Worker runs on a goroutine; poll briefly until it settles.
	deadline := time.Now().Add(time.Second)
	for q.Status() == QueryLoading && time.Now().Before(deadline) {
		time.Sleep(5 * time.Millisecond)
	}
	if q.Status() != QuerySuccess {
		t.Fatalf("expected success, got %v", q.Status())
	}
	node := q.View(
		func() *Element { return Skeleton(2) },
		func(d string) *Element { return Text(d) },
		func(e string) *Element { return Text("Error: " + e) },
	)
	if node.content != "hello" {
		t.Fatalf("expected success view content 'hello', got %q", node.content)
	}
}

func TestSkeleton(t *testing.T) {
	sk := Skeleton(4)
	if sk.className != "skeleton" {
		t.Fatalf("expected class skeleton, got %q", sk.className)
	}
	if len(sk.children) != 4 {
		t.Fatalf("expected 4 lines, got %d", len(sk.children))
	}
	if sk.children[0].className != "skeleton-line" {
		t.Fatalf("expected skeleton-line, got %q", sk.children[0].className)
	}
}

func TestRouteParamMatch(t *testing.T) {
	params := map[string]string{}
	if !matchPattern("/user/:id", "/user/42", params) {
		t.Fatal("expected /user/:id to match /user/42")
	}
	if params["id"] != "42" {
		t.Fatalf("expected id 42, got %q", params["id"])
	}

	params = map[string]string{}
	if !matchPattern("/post/:cat/:slug", "/post/news/hello", params) {
		t.Fatal("expected multi-param match")
	}
	if params["cat"] != "news" || params["slug"] != "hello" {
		t.Fatalf("expected cat=news slug=hello, got %v", params)
	}

	params = map[string]string{}
	if matchPattern("/user/:id", "/post/42", params) {
		t.Fatal("expected mismatch on literal segment")
	}

	params = map[string]string{}
	if !matchPattern("/user/:id", "/user/7?tab=info", params) {
		t.Fatal("expected query string to be stripped before matching")
	}
	if params["id"] != "7" {
		t.Fatalf("expected id 7, got %q", params["id"])
	}
}
