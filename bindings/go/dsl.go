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
}

var routers = map[uintptr]*routerState{}

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
		rs = &routerState{routes: map[string]func() *Element{}}
		routers[a.handle] = rs
	}
	return rs
}

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
	if fn := rs.routes[rs.current]; fn != nil {
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
