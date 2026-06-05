package fluxui

// FluxUI Declarative DSL for Go — modern, HTML/Blink-faithful builder.
//
// Element names match HTML exactly (Div, Span, P, H1..H6, Nav, Section, Button,
// Input, A, Img, Ul, Li, ...). Layout is expressed in CSS (display:flex/grid)
// exactly like the browser — there are no bespoke Row/Column nodes.
//
// Usage:
//
//	app := fluxui.NewApp(1200, 800, "CompanyGuard")
//	app.AddCSS(".app { display:flex } .content { display:flex; flex-direction:column }")
//	devices := fluxui.NewState(128)
//	app.SetRoot(
//	    fluxui.Div(
//	        fluxui.Nav(
//	            fluxui.Button("Dashboard"),
//	            fluxui.Button("Dispositivos"),
//	        ).Class("sidebar"),
//	        fluxui.Div(
//	            fluxui.H1("CompanyGuard"),
//	            fluxui.TextFn(func() string { return strconv.Itoa(devices.Get()) }),
//	            fluxui.Button("Escanear ahora").Class("primary").OnClick(func() {
//	                devices.Set(devices.Get() + 1)
//	            }),
//	        ).Class("content"),
//	    ).Class("app"),
//	)
//	app.RunReactive()

import (
	"strings"
	"sync"
	"syscall"
	"unsafe"
)

// ============================================================
//  Reactive binding registry — re-evaluates TextFn nodes each frame.
// ============================================================

type reactiveBinding struct {
	widget *Widget
	fn     func() string
	last   string
}

var reactiveBindings []*reactiveBinding

func registerReactiveText(w *Widget, fn func() string, initial string) {
	reactiveBindings = append(reactiveBindings, &reactiveBinding{widget: w, fn: fn, last: initial})
}

// PumpReactiveBindings re-evaluates every reactive TextFn binding and pushes
// changed values into the underlying widgets. Returns true if anything changed.
func PumpReactiveBindings() bool {
	changed := false
	for _, b := range reactiveBindings {
		if b.widget == nil || b.widget.handle == 0 {
			continue
		}
		v := b.fn()
		if v != b.last {
			b.last = v
			b.widget.SetContent(v)
			changed = true
		}
	}
	return changed
}

// ============================================================
//  State[T] — lightweight reactive primitive.
// ============================================================

type State[T comparable] struct {
	value     T
	listeners []func()
}

func NewState[T comparable](initial T) *State[T] {
	return &State[T]{value: initial}
}

func (s *State[T]) Get() T { return s.value }

func (s *State[T]) Set(v T) {
	s.value = v
	for _, fn := range s.listeners {
		fn()
	}
}

func (s *State[T]) OnChange(fn func()) {
	s.listeners = append(s.listeners, fn)
}

// ============================================================
//  Element — deferred, HTML-named node. Materialized on mount.
//
//  Every node carries an HTML tag name. mount() routes through
//  Widget.AddElement(tag, ...) which is the single source of truth for the
//  tag -> widget mapping (Blink UA parity).
// ============================================================

type Element struct {
	tag          string
	content      string
	className    string
	id           string
	onClick      func()
	textFn       func() string
	inlineStyles [][2]string
	attrs        [][2]string
	children     []*Element
}

func newElement(tag string) *Element { return &Element{tag: tag} }

// Chaining setters — mirror HTML attributes / DOM properties.
func (e *Element) Class(cls string) *Element { e.className = cls; return e }
func (e *Element) ID(id string) *Element     { e.id = id; return e }
func (e *Element) OnClick(fn func()) *Element { e.onClick = fn; return e }
func (e *Element) Style(prop, val string) *Element {
	e.inlineStyles = append(e.inlineStyles, [2]string{prop, val})
	return e
}
func (e *Element) Attr(name, val string) *Element {
	e.attrs = append(e.attrs, [2]string{name, val})
	return e
}
func (e *Element) Href(url string) *Element { return e.Attr("href", url) }
func (e *Element) Src(url string) *Element  { e.content = url; return e }

func (e *Element) mount(parent *Widget) *Widget {
	var w *Widget
	if e.textFn != nil {
		initial := e.textFn()
		w = parent.AddElement("span", initial, e.className)
		if w != nil {
			registerReactiveText(w, e.textFn, initial)
		}
	} else {
		w = parent.AddElement(e.tag, e.content, e.className)
	}
	if w == nil {
		return nil
	}
	if e.id != "" {
		w.SetID(e.id)
	}
	if e.onClick != nil {
		w.SetOnClick(e.onClick)
	}
	for _, s := range e.inlineStyles {
		w.css(s[0] + ":" + s[1])
	}
	for _, child := range e.children {
		child.mount(w)
	}
	return w
}

// ============================================================
//  HTML element builders (names match HTML/Blink exactly).
// ============================================================

func container(tag string, children []*Element) *Element {
	e := newElement(tag)
	e.children = children
	return e
}
func leaf(tag, content string) *Element {
	e := newElement(tag)
	e.content = content
	return e
}

// Flow containers.
func Div(children ...*Element) *Element        { return container("div", children) }
func Section(children ...*Element) *Element    { return container("section", children) }
func Article(children ...*Element) *Element    { return container("article", children) }
func Aside(children ...*Element) *Element       { return container("aside", children) }
func Header(children ...*Element) *Element      { return container("header", children) }
func Footer(children ...*Element) *Element      { return container("footer", children) }
func Main(children ...*Element) *Element        { return container("main", children) }
func Nav(children ...*Element) *Element         { return container("nav", children) }
func FormEl(children ...*Element) *Element      { return container("form", children) }
func Fieldset(children ...*Element) *Element    { return container("fieldset", children) }
func Blockquote(children ...*Element) *Element  { return container("blockquote", children) }
func Figure(children ...*Element) *Element      { return container("figure", children) }
func Ul(children ...*Element) *Element          { return container("ul", children) }
func Ol(children ...*Element) *Element          { return container("ol", children) }
func Li(children ...*Element) *Element          { return container("li", children) }
func Table(children ...*Element) *Element       { return container("table", children) }
func Tr(children ...*Element) *Element          { return container("tr", children) }

// Text content.
func Text(content string) *Element   { return leaf("span", content) }
func Span(content string) *Element   { return leaf("span", content) }
func P(content string) *Element      { return leaf("p", content) }
func H1(content string) *Element     { return leaf("h1", content) }
func H2(content string) *Element     { return leaf("h2", content) }
func H3(content string) *Element     { return leaf("h3", content) }
func H4(content string) *Element     { return leaf("h4", content) }
func H5(content string) *Element     { return leaf("h5", content) }
func H6(content string) *Element     { return leaf("h6", content) }
func Strong(content string) *Element { return leaf("strong", content) }
func Em(content string) *Element     { return leaf("em", content) }
func Small(content string) *Element  { return leaf("small", content) }
func Label(content string) *Element  { return leaf("label", content) }
func Legend(content string) *Element { return leaf("legend", content) }
func Code(content string) *Element   { return leaf("code", content) }
func Pre(content string) *Element    { return leaf("pre", content) }
func Td(content string) *Element     { return leaf("td", content) }
func Th(content string) *Element     { return leaf("th", content) }

// TextFn is the reactive text variant — re-evaluated whenever bound State changes.
func TextFn(fn func() string) *Element {
	e := newElement("span")
	e.textFn = fn
	return e
}

// Interactive controls.
func Button(label string) *Element { return leaf("button", label) }
func Input(placeholder string) *Element {
	return leaf("input", placeholder)
}
func TextArea(placeholder string) *Element { return leaf("textarea", placeholder) }
func A(content, href string) *Element {
	e := leaf("a", content)
	if href != "" {
		e.Attr("href", href)
	}
	return e
}
func Img(src string) *Element                  { return leaf("img", src) }
func Checkbox() *Element                       { return newElement("checkbox") }
func Radio() *Element                          { return newElement("radio") }
func Hr() *Element                             { return newElement("hr") }
func Br() *Element                             { return newElement("br") }
func Select(options ...*Element) *Element      { return container("select", options) }
func Option(label string) *Element             { return leaf("option", label) }

// El is a generic escape hatch for any tag.
func El(tag string, children ...*Element) *Element { return container(tag, children) }

// ============================================================
//  App convenience wrappers for the declarative flow.
// ============================================================

// NewApp creates and initializes an App + window in one call.
func NewApp(width, height int, title string) *App {
	app, err := CreateApp()
	if err != nil {
		return nil
	}
	app.Init(title, width, height)
	app.LoadDefaultFont(16.0)
	return app
}

// AddCSS is an alias for AddStylesheet (matches C++ App::addCSS).
func (a *App) AddCSS(css string) { a.AddStylesheet(css) }

// LoadCSS is an alias for LoadStylesheet (matches C++ App::loadCSS).
func (a *App) LoadCSS(path string) bool { return a.LoadStylesheet(path) }

// HotReload enables live CSS reloading (matches C++ App::hotReload). Any
// stylesheet loaded via LoadCSS/LoadStylesheet is watched and re-applied on edit.
func (a *App) HotReload(enable bool) *App {
	a.EnableHotReload(enable, 0.25)
	return a
}

// WatchCSS watches an extra CSS file for changes (matches C++ App::watchCSS).
func (a *App) WatchCSS(path string) *App {
	a.WatchStylesheet(path)
	return a
}

// ReloadCSS forces an immediate reload of all CSS sources (matches reloadCSS).
func (a *App) ReloadCSS() { a.ReloadStyles() }

// PowerSaver biases frame pacing for best battery life / weak hardware.
func (a *App) PowerSaver() *App { a.SetPowerProfile(PowerSaver); return a }

// HighPerformance always targets the max frame rate.
func (a *App) HighPerformance() *App { a.SetPowerProfile(PowerHighPerformance); return a }

// Balanced applies moderate frame-rate caps even on AC power.
func (a *App) Balanced() *App { a.SetPowerProfile(PowerBalanced); return a }

// SetRoot mounts a declarative Element tree as the application root.
func (a *App) SetRoot(root *Element) {
	r := a.Root()
	if r == nil || root == nil {
		return
	}
	r.ClearChildren()
	root.mount(r)
}

// ============================================================
//  Declarative routing (like Next.js). Routing state is kept in a side table
//  keyed by the app handle, since App is defined in another file.
// ============================================================

type routerState struct {
	routes  map[string]func() *Element
	order   []string
	layout  func(content *Element) *Element
	current string
	params  map[string]string
	query   map[string]string
}

var routers = map[uintptr]*routerState{}

// matchPattern matches a route pattern ("/user/:id") against a concrete path
// ("/user/42"), filling params. Returns false on mismatch.
func matchPattern(pattern, path string, params map[string]string) bool {
	clean := path
	if i := strings.IndexByte(clean, '?'); i >= 0 {
		clean = clean[:i]
	}
	pp := splitPath(pattern)
	cp := splitPath(clean)
	if len(pp) != len(cp) {
		return false
	}
	for i := range pp {
		if len(pp[i]) > 0 && pp[i][0] == ':' {
			params[pp[i][1:]] = cp[i]
		} else if pp[i] != cp[i] {
			return false
		}
	}
	return true
}

func splitPath(s string) []string {
	var parts []string
	for _, p := range strings.Split(s, "/") {
		if p != "" {
			parts = append(parts, p)
		}
	}
	return parts
}

func parseQuery(path string, out map[string]string) {
	i := strings.IndexByte(path, '?')
	if i < 0 {
		return
	}
	for _, pair := range strings.Split(path[i+1:], "&") {
		if pair == "" {
			continue
		}
		if eq := strings.IndexByte(pair, '='); eq >= 0 {
			out[pair[:eq]] = pair[eq+1:]
		} else {
			out[pair] = ""
		}
	}
}

// pendingViews collects views registered by init() in view files (file-based
// routing like Next.js — each views/foo.go self-registers in its init()).
var pendingViews []struct {
	path string
	fn   func() *Element
}

// RegisterView is called from a view file's init() to self-register a route.
// This gives Next.js-style file-based routing with no generated files:
//
//	// views/dashboard.go
//	func init() { fluxui.RegisterView("/dashboard", DashboardView) }
//
// Then in main, after creating the app: app.UseViews()
func RegisterView(path string, fn func() *Element) {
	pendingViews = append(pendingViews, struct {
		path string
		fn   func() *Element
	}{path, fn})
}

// UseViews registers every view collected via RegisterView() into this app.
func (a *App) UseViews() *App {
	for _, v := range pendingViews {
		a.AddRoute(v.path, v.fn)
	}
	return a
}

func (a *App) router() *routerState {
	rs := routers[a.handle]
	if rs == nil {
		rs = &routerState{
			routes: map[string]func() *Element{},
			params: map[string]string{},
			query:  map[string]string{},
		}
		routers[a.handle] = rs
	}
	return rs
}

// resolveRoute finds the builder for the current route (exact or param match),
// filling params/query. Returns nil if no match.
func (rs *routerState) resolveRoute() func() *Element {
	rs.params = map[string]string{}
	rs.query = map[string]string{}
	parseQuery(rs.current, rs.query)
	if fn := rs.routes[rs.current]; fn != nil {
		return fn
	}
	for _, path := range rs.order {
		params := map[string]string{}
		if matchPattern(path, rs.current, params) {
			rs.params = params
			return rs.routes[path]
		}
	}
	return nil
}

// Param returns a route param captured from a pattern like "/user/:id".
func (a *App) Param(name string) string { return a.router().params[name] }

// Query returns a query-string value (e.g. ?tab=info -> Query("tab")).
func (a *App) Query(name string) string { return a.router().query[name] }

// AddRoute registers a route. viewFn returns an Element tree for that path.
func (a *App) AddRoute(path string, viewFn func() *Element) *App {
	rs := a.router()
	if _, ok := rs.routes[path]; !ok {
		rs.order = append(rs.order, path)
	}
	rs.routes[path] = viewFn
	return a
}

// SetLayout sets the app shell. layoutFn(content) returns the shell tree which
// must embed the given content element.
func (a *App) SetLayout(layoutFn func(content *Element) *Element) *App {
	a.router().layout = layoutFn
	return a
}

// Route returns the current route path.
func (a *App) Route() string { return a.router().current }

// Navigate switches to a route and rebuilds the shell (nav highlights refresh).
func (a *App) Navigate(path string) {
	a.router().current = path
	a.Build(path)
}

// Build mounts the shell + initial route. Uses the first route if path is empty.
func (a *App) Build(initialRoute string) {
	r := a.Root()
	rs := a.router()
	if r == nil {
		return
	}
	r.ClearChildren()
	if initialRoute != "" {
		rs.current = initialRoute
	} else if rs.current == "" && len(rs.order) > 0 {
		rs.current = rs.order[0]
	}

	var content *Element
	if fn := rs.resolveRoute(); fn != nil {
		content = fn()
	}
	if rs.layout != nil {
		shell := rs.layout(content)
		if shell != nil {
			shell.mount(r)
		}
	} else if content != nil {
		content.mount(r)
	}
}

// RunReactive installs the reactive pump into the update loop and runs the app.
func (a *App) RunReactive() {
	rs := a.router()
	if len(rs.order) > 0 {
		a.Build(rs.current)
	}
	a.SetUpdateCallback(func(float32) {
		PumpReactiveBindings()
	})
	a.Run()
}

// css applies an inline CSS declaration to a widget via the native helper.
func (w *Widget) css(decl string) {
	if w == nil || w.handle == 0 {
		return
	}
	cCss, err := syscall.BytePtrFromString(decl)
	if err != nil {
		return
	}
	fluxui_widget_css.Call(w.handle, uintptr(unsafe.Pointer(cCss)))
}

// ============================================================
//  Store[T] — global state container (Zustand-style)
//
//	type CartState struct{ Count int }
//	cart := fluxui.NewStore(CartState{})
//	cart.Set(func(s *CartState) { s.Count++ })
//	fluxui.TextFn(func() string { return strconv.Itoa(cart.Get().Count) })
// ============================================================

type Store[T any] struct {
	mu          sync.Mutex
	state       T
	subscribers []func()
}

func NewStore[T any](initial T) *Store[T] {
	return &Store[T]{state: initial}
}

func (s *Store[T]) Get() T {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.state
}

// Set mutates the state via a reducer and notifies subscribers + redraws.
func (s *Store[T]) Set(mutator func(*T)) {
	s.mu.Lock()
	mutator(&s.state)
	subs := append([]func(){}, s.subscribers...)
	s.mu.Unlock()
	for _, fn := range subs {
		fn()
	}
	fluxui_app_request_redraw_global()
}

func (s *Store[T]) Subscribe(fn func()) { s.subscribers = append(s.subscribers, fn) }

// fluxui_app_request_redraw_global is a best-effort redraw via any live app.
func fluxui_app_request_redraw_global() {
	// Stores don't hold an app handle; the reactive pump picks up changes each
	// frame, so an explicit redraw isn't strictly required here.
}

// ============================================================
//  Schema — runtime validation (Zod-style)
//
//	schema := fluxui.NewSchema().
//	    Field("email", fluxui.RuleString().Email()).
//	    Field("age",   fluxui.RuleNumber().Min(18))
//	res := schema.Validate(map[string]string{"email": "a@b.com", "age": "20"})
//	if !res.OK { ... res.Errors ... }
// ============================================================

type ruleKind int

const (
	ruleString ruleKind = iota
	ruleNumber
	ruleBool
)

type Rule struct {
	kind     ruleKind
	required bool
	min, max *float64
	minLen   *int
	maxLen   *int
	email    bool
}

func RuleString() Rule { return Rule{kind: ruleString, required: true} }
func RuleNumber() Rule { return Rule{kind: ruleNumber, required: true} }
func RuleBool() Rule   { return Rule{kind: ruleBool, required: true} }

func (r Rule) Optional() Rule        { r.required = false; return r }
func (r Rule) Min(v float64) Rule    { r.min = &v; return r }
func (r Rule) Max(v float64) Rule    { r.max = &v; return r }
func (r Rule) MinLength(v int) Rule  { r.minLen = &v; return r }
func (r Rule) MaxLength(v int) Rule  { r.maxLen = &v; return r }
func (r Rule) Email() Rule           { r.email = true; return r }

func (r Rule) check(field, value string) string {
	if value == "" {
		if r.required {
			return field + " is required"
		}
		return ""
	}
	switch r.kind {
	case ruleNumber:
		var d float64
		if _, err := fmtSscan(value, &d); err != nil {
			return field + " must be a number"
		}
		if r.min != nil && d < *r.min {
			return field + " is too small"
		}
		if r.max != nil && d > *r.max {
			return field + " is too large"
		}
	case ruleString:
		if r.minLen != nil && len(value) < *r.minLen {
			return field + " is too short"
		}
		if r.maxLen != nil && len(value) > *r.maxLen {
			return field + " is too long"
		}
		if r.email && !strings.Contains(value, "@") {
			return field + " must be a valid email"
		}
	case ruleBool:
		if value != "true" && value != "false" {
			return field + " must be true/false"
		}
	}
	return ""
}

type ValidationResult struct {
	OK     bool
	Errors map[string]string
}

type Schema struct {
	fields []struct {
		name string
		rule Rule
	}
}

func NewSchema() *Schema { return &Schema{} }

func (s *Schema) Field(name string, rule Rule) *Schema {
	s.fields = append(s.fields, struct {
		name string
		rule Rule
	}{name, rule})
	return s
}

func (s *Schema) Validate(data map[string]string) ValidationResult {
	res := ValidationResult{OK: true, Errors: map[string]string{}}
	for _, f := range s.fields {
		if err := f.rule.check(f.name, data[f.name]); err != "" {
			res.OK = false
			res.Errors[f.name] = err
		}
	}
	return res
}

// fmtSscan parses a float without importing fmt at the top (kept local).
func fmtSscan(s string, out *float64) (int, error) {
	var neg bool
	i := 0
	if i < len(s) && (s[i] == '+' || s[i] == '-') {
		neg = s[i] == '-'
		i++
	}
	var val float64
	var seen bool
	for ; i < len(s) && s[i] >= '0' && s[i] <= '9'; i++ {
		val = val*10 + float64(s[i]-'0')
		seen = true
	}
	if i < len(s) && s[i] == '.' {
		i++
		frac := 0.1
		for ; i < len(s) && s[i] >= '0' && s[i] <= '9'; i++ {
			val += float64(s[i]-'0') * frac
			frac *= 0.1
			seen = true
		}
	}
	if !seen || i != len(s) {
		return 0, errParse
	}
	if neg {
		val = -val
	}
	*out = val
	return 1, nil
}

var errParse = &parseError{}

type parseError struct{}

func (e *parseError) Error() string { return "parse error" }

// ============================================================
//  QueryStatus / Query[T] — async fetch with loading/error/data states
//
//	q := fluxui.NewQuery(func() (string, error) { return httpGet("/api") })
//	q.Start()
//	q.View(
//	    func() *fluxui.Element { return fluxui.Skeleton(3) },
//	    func(d string) *fluxui.Element { return fluxui.Text(d) },
//	    func(e string) *fluxui.Element { return fluxui.Text("Error: " + e) },
//	)
// ============================================================

type QueryStatus int

const (
	QueryIdle QueryStatus = iota
	QueryLoading
	QuerySuccess
	QueryError
)

type Query[T any] struct {
	fetcher func() (T, error)
	mu      sync.Mutex
	status  QueryStatus
	data    T
	err     string
}

func NewQuery[T any](fetcher func() (T, error)) *Query[T] {
	return &Query[T]{fetcher: fetcher}
}

func (q *Query[T]) Status() QueryStatus {
	q.mu.Lock()
	defer q.mu.Unlock()
	return q.status
}

func (q *Query[T]) Start() {
	q.mu.Lock()
	if q.status == QueryLoading {
		q.mu.Unlock()
		return
	}
	q.status = QueryLoading
	q.mu.Unlock()
	go func() {
		data, err := q.fetcher()
		q.mu.Lock()
		if err != nil {
			q.err = err.Error()
			q.status = QueryError
		} else {
			q.data = data
			q.status = QuerySuccess
		}
		q.mu.Unlock()
	}()
}

func (q *Query[T]) Refetch() {
	q.mu.Lock()
	q.status = QueryIdle
	q.mu.Unlock()
	q.Start()
}

// View renders the right Element for the current state.
func (q *Query[T]) View(onLoading func() *Element, onSuccess func(T) *Element, onError func(string) *Element) *Element {
	q.mu.Lock()
	st, data, errMsg := q.status, q.data, q.err
	q.mu.Unlock()
	switch st {
	case QuerySuccess:
		return onSuccess(data)
	case QueryError:
		if onError != nil {
			return onError(errMsg)
		}
		return P("Error: " + errMsg)
	default:
		if onLoading != nil {
			return onLoading()
		}
		return Div()
	}
}

// ============================================================
//  Skeleton — easy loading placeholders
// ============================================================

func Skeleton(lines int) *Element {
	children := make([]*Element, 0, lines)
	for i := 0; i < lines; i++ {
		children = append(children, Div().Class("skeleton-line"))
	}
	return Div(children...).Class("skeleton")
}

func SkeletonBox(w, h string) *Element {
	return Div().Class("skeleton skeleton-box").Style("width", w).Style("height", h)
}
