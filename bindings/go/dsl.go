package fluxui

// FluxUI Declarative DSL for Go — mirrors the C++ dsl.h API 1:1.
//
// Usage:
//
//	app := fluxui.NewApp(1200, 800, "CompanyGuard")
//	app.AddCSS(".app { width: 100%; height: 100% } ...")
//	devices := fluxui.NewState(128)
//	app.SetRoot(
//	    fluxui.Row(
//	        fluxui.Sidebar(
//	            fluxui.NavItem("Dashboard"),
//	            fluxui.NavItem("Dispositivos"),
//	        ).Class("sidebar"),
//	        fluxui.Column(
//	            fluxui.Text("CompanyGuard").Class("h1"),
//	            fluxui.TextFn(func() string { return strconv.Itoa(devices.Get()) }),
//	            fluxui.Button("Escanear ahora").Class("primary").OnClick(func() {
//	                devices.Set(devices.Get() + 1)
//	            }),
//	        ).Class("content"),
//	    ).Class("app"),
//	)
//	app.RunReactive()
//
// The same declarative pattern as C++/Rust/Java: build a tree of nodes, attach
// classes/handlers via chaining, and reactive State drives TextFn updates.

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
// The App's reactive update loop calls this every frame.
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
//  State[T] — lightweight reactive primitive (matches C++ State<T>).
// ============================================================

type State[T comparable] struct {
	value     T
	listeners []func()
}

// NewState creates a reactive State holding an initial value.
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
//  Node — deferred declarative element. Materialized on mount.
// ============================================================

type nodeKind int

const (
	kindDiv nodeKind = iota
	kindRow
	kindColumn
	kindSidebar
	kindCard
	kindGrid
	kindText
	kindTextFn
	kindButton
	kindNavItem
	kindInput
	kindCheckbox
	kindElement
)

// Node is a declarative description of a widget and its subtree. Building a Node
// allocates nothing in the native layer until SetRoot mounts it onto the App.
type Node struct {
	kind      nodeKind
	tag       string
	content   string
	className string
	id        string
	onClick   func()
	textFn    func() string
	checked   bool
	children  []*Node
}

func newNode(k nodeKind) *Node { return &Node{kind: k} }

// Chaining setters (match C++ WidgetBuilder).

func (n *Node) Class(cls string) *Node      { n.className = cls; return n }
func (n *Node) ID(id string) *Node          { n.id = id; return n }
func (n *Node) OnClick(fn func()) *Node      { n.onClick = fn; return n }

// mount materializes this node (and its subtree) under the given parent widget,
// returning the created widget. Reactive TextFn nodes register themselves.
func (n *Node) mount(parent *Widget) *Widget {
	var w *Widget
	switch n.kind {
	case kindRow:
		w = parent.AddPanel(n.className)
		applyFlex(w, false)
	case kindColumn:
		w = parent.AddPanel(n.className)
		applyFlex(w, true)
	case kindSidebar:
		cls := n.className
		if cls == "" {
			cls = "sidebar"
		}
		w = parent.AddElement("nav", "", cls)
		applyFlex(w, true)
	case kindCard:
		cls := n.className
		if cls == "" {
			cls = "card"
		}
		w = parent.AddPanel(cls)
		applyFlex(w, true)
	case kindGrid:
		w = parent.AddPanel(n.className)
	case kindDiv:
		w = parent.AddPanel(n.className)
	case kindText:
		w = parent.AddText(n.content, n.className)
	case kindTextFn:
		initial := ""
		if n.textFn != nil {
			initial = n.textFn()
		}
		w = parent.AddText(initial, n.className)
		if n.textFn != nil {
			registerReactiveText(w, n.textFn, initial)
		}
	case kindButton:
		w = parent.AddButton(n.content, n.className)
	case kindNavItem:
		cls := n.className
		if cls == "" {
			cls = "nav-item"
		}
		w = parent.AddButton(n.content, cls)
	case kindInput:
		w = parent.AddTextInput(n.content, n.className)
	case kindCheckbox:
		w = parent.AddCheckbox(n.checked, n.className)
	case kindElement:
		w = parent.AddElement(n.tag, n.content, n.className)
	default:
		w = parent.AddPanel(n.className)
	}

	if w == nil {
		return nil
	}
	if n.id != "" {
		w.SetID(n.id)
	}
	if n.onClick != nil {
		w.SetOnClick(n.onClick)
	}
	for _, child := range n.children {
		child.mount(w)
	}
	return w
}

func applyFlex(w *Widget, column bool) {
	if w == nil {
		return
	}
	// display:flex with the requested direction is expressed through CSS so the
	// declarative containers behave exactly like the C++ Row/Column builders.
	if column {
		w.appendInlineCSS("display:flex;flex-direction:column")
	} else {
		w.appendInlineCSS("display:flex;flex-direction:row")
	}
}

// ============================================================
//  Builder functions (match C++ free functions).
// ============================================================

func Row(children ...*Node) *Node     { n := newNode(kindRow); n.children = children; return n }
func Column(children ...*Node) *Node  { n := newNode(kindColumn); n.children = children; return n }
func Sidebar(children ...*Node) *Node { n := newNode(kindSidebar); n.children = children; return n }
func Card(children ...*Node) *Node    { n := newNode(kindCard); n.children = children; return n }
func Grid(children ...*Node) *Node    { n := newNode(kindGrid); n.children = children; return n }

func Text(content string) *Node {
	n := newNode(kindText)
	n.content = content
	return n
}

// TextFn is the reactive Text variant — re-evaluated whenever bound State changes.
func TextFn(fn func() string) *Node {
	n := newNode(kindTextFn)
	n.textFn = fn
	return n
}

func Button(label string) *Node  { n := newNode(kindButton); n.content = label; return n }
func NavItem(label string) *Node { n := newNode(kindNavItem); n.content = label; return n }
func Input(placeholder string) *Node {
	n := newNode(kindInput)
	n.content = placeholder
	return n
}
func Checkbox(checked bool) *Node { n := newNode(kindCheckbox); n.checked = checked; return n }

func Element(tag string, children ...*Node) *Node {
	n := newNode(kindElement)
	n.tag = tag
	n.children = children
	return n
}

// ============================================================
//  App convenience wrappers for the declarative flow.
// ============================================================

// NewApp creates and initializes an App + window in one call, matching the
// ergonomics of the C++ App{} constructor.
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

// SetRoot mounts a declarative Node tree as the application root.
func (a *App) SetRoot(root *Node) {
	r := a.Root()
	if r == nil || root == nil {
		return
	}
	r.ClearChildren()
	root.mount(r)
}

// RunReactive installs the reactive pump into the update loop and runs the app.
func (a *App) RunReactive() {
	a.SetUpdateCallback(func(float32) {
		PumpReactiveBindings()
	})
	a.Run()
}

// appendInlineCSS applies a small inline CSS declaration block to a widget via
// the native fluxui_widget_css helper, so the declarative flex containers behave
// exactly like the C++ Row/Column builders.
func (w *Widget) appendInlineCSS(css string) {
	if w == nil || w.handle == 0 {
		return
	}
	cCss, err := syscall.BytePtrFromString(css)
	if err != nil {
		return
	}
	fluxui_widget_css.Call(w.handle, uintptr(unsafe.Pointer(cCss)))
}
