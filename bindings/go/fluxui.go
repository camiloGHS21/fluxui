package fluxui

import (
	"errors"
	"math"
	"syscall"
	"unsafe"
)

var (
	fluxui_dll = syscall.NewLazyDLL("fluxui_shared.dll")

	fluxui_app_create                 = fluxui_dll.NewProc("fluxui_app_create")
	fluxui_app_destroy                = fluxui_dll.NewProc("fluxui_app_destroy")
	fluxui_app_init                   = fluxui_dll.NewProc("fluxui_app_init")
	fluxui_app_run                    = fluxui_dll.NewProc("fluxui_app_run")
	fluxui_app_stop                   = fluxui_dll.NewProc("fluxui_app_stop")
	fluxui_app_root                   = fluxui_dll.NewProc("fluxui_app_root")
	fluxui_app_set_backend            = fluxui_dll.NewProc("fluxui_app_set_backend")
	fluxui_app_get_backend            = fluxui_dll.NewProc("fluxui_app_get_backend")
	fluxui_app_set_update_callback_go = fluxui_dll.NewProc("fluxui_app_set_update_callback_go")
	fluxui_app_on_event               = fluxui_dll.NewProc("fluxui_app_on_event")
	fluxui_app_off_event              = fluxui_dll.NewProc("fluxui_app_off_event")
	fluxui_app_emit_custom_event      = fluxui_dll.NewProc("fluxui_app_emit_custom_event")
	fluxui_app_load_stylesheet        = fluxui_dll.NewProc("fluxui_app_load_stylesheet")
	fluxui_app_add_stylesheet         = fluxui_dll.NewProc("fluxui_app_add_stylesheet")
	fluxui_app_load_default_font      = fluxui_dll.NewProc("fluxui_app_load_default_font")

	fluxui_widget_add_panel           = fluxui_dll.NewProc("fluxui_widget_add_panel")
	fluxui_widget_add_button          = fluxui_dll.NewProc("fluxui_widget_add_button")
	fluxui_widget_add_text            = fluxui_dll.NewProc("fluxui_widget_add_text")
	fluxui_widget_add_canvas          = fluxui_dll.NewProc("fluxui_widget_add_canvas")
	fluxui_widget_add_text_input      = fluxui_dll.NewProc("fluxui_widget_add_text_input")
	fluxui_widget_add_select          = fluxui_dll.NewProc("fluxui_widget_add_select")
	fluxui_widget_add_option          = fluxui_dll.NewProc("fluxui_widget_add_option")

	fluxui_widget_add_element         = fluxui_dll.NewProc("fluxui_widget_add_element")
	fluxui_widget_add_form            = fluxui_dll.NewProc("fluxui_widget_add_form")
	fluxui_widget_add_fieldset        = fluxui_dll.NewProc("fluxui_widget_add_fieldset")
	fluxui_widget_add_label           = fluxui_dll.NewProc("fluxui_widget_add_label")
	fluxui_widget_add_legend          = fluxui_dll.NewProc("fluxui_widget_add_legend")
	fluxui_widget_add_input           = fluxui_dll.NewProc("fluxui_widget_add_input")
	fluxui_widget_add_password_input  = fluxui_dll.NewProc("fluxui_widget_add_password_input")
	fluxui_widget_add_textarea        = fluxui_dll.NewProc("fluxui_widget_add_textarea")
	fluxui_widget_add_checkbox        = fluxui_dll.NewProc("fluxui_widget_add_checkbox")
	fluxui_widget_add_radio           = fluxui_dll.NewProc("fluxui_widget_add_radio")
	fluxui_widget_add_range           = fluxui_dll.NewProc("fluxui_widget_add_range")
	fluxui_widget_add_anchor          = fluxui_dll.NewProc("fluxui_widget_add_anchor")
	fluxui_widget_add_details         = fluxui_dll.NewProc("fluxui_widget_add_details")
	fluxui_widget_add_summary         = fluxui_dll.NewProc("fluxui_widget_add_summary")
	fluxui_widget_add_dialog          = fluxui_dll.NewProc("fluxui_widget_add_dialog")
	fluxui_widget_add_meter           = fluxui_dll.NewProc("fluxui_widget_add_meter")
	fluxui_widget_add_progress_element = fluxui_dll.NewProc("fluxui_widget_add_progress_element")
	fluxui_widget_add_hr              = fluxui_dll.NewProc("fluxui_widget_add_hr")
	fluxui_widget_add_br              = fluxui_dll.NewProc("fluxui_widget_add_br")
	fluxui_widget_add_icon            = fluxui_dll.NewProc("fluxui_widget_add_icon")
	fluxui_widget_add_progress_bar    = fluxui_dll.NewProc("fluxui_widget_add_progress_bar")
	fluxui_widget_add_stat_card       = fluxui_dll.NewProc("fluxui_widget_add_stat_card")

	fluxui_text_set_content           = fluxui_dll.NewProc("fluxui_text_set_content")
	fluxui_button_set_label           = fluxui_dll.NewProc("fluxui_button_set_label")
	fluxui_text_input_set_value       = fluxui_dll.NewProc("fluxui_text_input_set_value")
	fluxui_text_input_get_value       = fluxui_dll.NewProc("fluxui_text_input_get_value")
	fluxui_text_input_set_placeholder = fluxui_dll.NewProc("fluxui_text_input_set_placeholder")
	fluxui_select_set_selected_index  = fluxui_dll.NewProc("fluxui_select_set_selected_index")
	fluxui_select_get_selected_index  = fluxui_dll.NewProc("fluxui_select_get_selected_index")

	fluxui_text_input_set_type        = fluxui_dll.NewProc("fluxui_text_input_set_type")
	fluxui_checkbox_set_checked       = fluxui_dll.NewProc("fluxui_checkbox_set_checked")
	fluxui_checkbox_get_checked       = fluxui_dll.NewProc("fluxui_checkbox_get_checked")
	fluxui_radio_set_checked          = fluxui_dll.NewProc("fluxui_radio_set_checked")
	fluxui_radio_get_checked          = fluxui_dll.NewProc("fluxui_radio_get_checked")
	fluxui_range_set_value            = fluxui_dll.NewProc("fluxui_range_set_value")
	fluxui_range_get_value            = fluxui_dll.NewProc("fluxui_range_get_value")
	fluxui_details_set_open           = fluxui_dll.NewProc("fluxui_details_set_open")
	fluxui_details_get_open           = fluxui_dll.NewProc("fluxui_details_get_open")
	fluxui_dialog_show                = fluxui_dll.NewProc("fluxui_dialog_show")
	fluxui_dialog_show_modal          = fluxui_dll.NewProc("fluxui_dialog_show_modal")
	fluxui_dialog_close               = fluxui_dll.NewProc("fluxui_dialog_close")
	fluxui_dialog_get_open            = fluxui_dll.NewProc("fluxui_dialog_get_open")
	fluxui_meter_set_value            = fluxui_dll.NewProc("fluxui_meter_set_value")
	fluxui_meter_get_value            = fluxui_dll.NewProc("fluxui_meter_get_value")
	fluxui_progress_element_set_value = fluxui_dll.NewProc("fluxui_progress_element_set_value")
	fluxui_progress_element_get_value = fluxui_dll.NewProc("fluxui_progress_element_get_value")
	fluxui_icon_set_glyph             = fluxui_dll.NewProc("fluxui_icon_set_glyph")
	fluxui_progress_bar_set_value     = fluxui_dll.NewProc("fluxui_progress_bar_set_value")
	fluxui_progress_bar_set_color     = fluxui_dll.NewProc("fluxui_progress_bar_set_color")

	fluxui_canvas_set_on_draw         = fluxui_dll.NewProc("fluxui_canvas_set_on_draw")
	fluxui_draw_rect                  = fluxui_dll.NewProc("fluxui_draw_rect")
	fluxui_draw_text                  = fluxui_dll.NewProc("fluxui_draw_text")
	fluxui_draw_image                 = fluxui_dll.NewProc("fluxui_draw_image")
	fluxui_renderer_flush             = fluxui_dll.NewProc("fluxui_renderer_flush")

	fluxui_widget_set_id              = fluxui_dll.NewProc("fluxui_widget_set_id")
	fluxui_widget_set_class           = fluxui_dll.NewProc("fluxui_widget_set_class")
	fluxui_widget_set_visible         = fluxui_dll.NewProc("fluxui_widget_set_visible")
	fluxui_widget_get_bounds          = fluxui_dll.NewProc("fluxui_widget_get_bounds")
	fluxui_widget_set_on_click        = fluxui_dll.NewProc("fluxui_widget_set_on_click")
	fluxui_widget_clear_children      = fluxui_dll.NewProc("fluxui_widget_clear_children")
	fluxui_widget_css                 = fluxui_dll.NewProc("fluxui_widget_css")

	fluxui_style_width_px             = fluxui_dll.NewProc("fluxui_style_width_px")
	fluxui_style_height_px            = fluxui_dll.NewProc("fluxui_style_height_px")
	fluxui_style_min_width_px         = fluxui_dll.NewProc("fluxui_style_min_width_px")
	fluxui_style_min_height_px        = fluxui_dll.NewProc("fluxui_style_min_height_px")
	fluxui_style_max_width_px         = fluxui_dll.NewProc("fluxui_style_max_width_px")
	fluxui_style_max_height_px        = fluxui_dll.NewProc("fluxui_style_max_height_px")
	fluxui_style_flex_grow            = fluxui_dll.NewProc("fluxui_style_flex_grow")
	fluxui_style_gap_px               = fluxui_dll.NewProc("fluxui_style_gap_px")
	fluxui_style_padding_all_px       = fluxui_dll.NewProc("fluxui_style_padding_all_px")
	fluxui_style_padding_px           = fluxui_dll.NewProc("fluxui_style_padding_px")
	fluxui_style_margin_all_px        = fluxui_dll.NewProc("fluxui_style_margin_all_px")
	fluxui_style_margin_px            = fluxui_dll.NewProc("fluxui_style_margin_px")
	fluxui_style_border_radius_px     = fluxui_dll.NewProc("fluxui_style_border_radius_px")
	fluxui_style_background_color     = fluxui_dll.NewProc("fluxui_style_background_color")
	fluxui_style_text_color           = fluxui_dll.NewProc("fluxui_style_text_color")
)

const (
	EventQuit          = 0
	EventWindowResized = 1
	EventMouseMove     = 2
	EventMouseDown     = 3
	EventMouseUp       = 4
	EventMouseWheel    = 5
	EventKeyDown       = 6
	EventKeyUp         = 7
	EventTextInput     = 8
	EventWidgetClick   = 9
	EventRouteChanged  = 10
	EventCustom        = 11
)

type App struct {
	handle uintptr
}

type Widget struct {
	handle uintptr
}

type Renderer struct {
	handle uintptr
}

type Rect struct {
	X, Y, W, H float32
}

type Color struct {
	R, G, B, A float32
}

type Event struct {
	Type          int32
	Target        uintptr
	Name          string
	Route         string
	PreviousRoute string
	Text          string
	X             float32
	Y             float32
	Dx            float32
	Dy            float32
	KeyCode       int32
	Modifiers     int32
	Button        int32
	ClickCount    int32
	Handled       bool
}

type cEventStruct struct {
	Type          int32
	Target        uintptr
	Name          uintptr
	Route         uintptr
	PreviousRoute uintptr
	Text          uintptr
	X             float32
	Y             float32
	Dx            float32
	Dy            float32
	KeyCode       int32
	Modifiers     int32
	Button        int32
	ClickCount    int32
	Handled       int32
}

func CreateApp() (*App, error) {
	r1, _, _ := fluxui_app_create.Call()
	if r1 == 0 {
		return nil, errors.New("failed to create FluxUI app")
	}
	return &App{handle: r1}, nil
}

func (a *App) Destroy() {
	if a.handle != 0 {
		fluxui_app_destroy.Call(a.handle)
		a.handle = 0
	}
}

func (a *App) Init(title string, width, height int) bool {
	cTitle, err := syscall.BytePtrFromString(title)
	if err != nil {
		return false
	}
	r1, _, _ := fluxui_app_init.Call(a.handle, uintptr(unsafe.Pointer(cTitle)), uintptr(width), uintptr(height))
	return r1 != 0
}

func (a *App) SetBackend(backend int) bool {
	r1, _, _ := fluxui_app_set_backend.Call(a.handle, uintptr(backend))
	return r1 != 0
}

func (a *App) GetBackend() int {
	r1, _, _ := fluxui_app_get_backend.Call(a.handle)
	return int(r1)
}

func (a *App) Run() {
	fluxui_app_run.Call(a.handle)
}

func (a *App) Stop() {
	fluxui_app_stop.Call(a.handle)
}

func (a *App) Root() *Widget {
	r1, _, _ := fluxui_app_root.Call(a.handle)
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (a *App) LoadStylesheet(path string) bool {
	cPath, err := syscall.BytePtrFromString(path)
	if err != nil {
		return false
	}
	r1, _, _ := fluxui_app_load_stylesheet.Call(a.handle, uintptr(unsafe.Pointer(cPath)))
	return r1 != 0
}

func (a *App) AddStylesheet(css string) {
	cCss, err := syscall.BytePtrFromString(css)
	if err != nil {
		return
	}
	fluxui_app_add_stylesheet.Call(a.handle, uintptr(unsafe.Pointer(cCss)))
}

func (a *App) LoadDefaultFont(size float32) bool {
	r1, _, _ := fluxui_app_load_default_font.Call(a.handle, uintptr(math.Float32bits(size)))
	return r1 != 0
}

func (a *App) EmitCustomEvent(name, text string) {
	cName, err := syscall.BytePtrFromString(name)
	if err != nil {
		return
	}
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return
	}
	fluxui_app_emit_custom_event.Call(a.handle, uintptr(unsafe.Pointer(cName)), uintptr(unsafe.Pointer(cText)))
}

func (w *Widget) AddPanel(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_panel.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddButton(label, className string) *Widget {
	cLabel, err := syscall.BytePtrFromString(label)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_button.Call(w.handle, uintptr(unsafe.Pointer(cLabel)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddText(text, className string) *Widget {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_text.Call(w.handle, uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddCanvas(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_canvas.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddTextInput(placeholder, className string) *Widget {
	cPlaceholder, err := syscall.BytePtrFromString(placeholder)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_text_input.Call(w.handle, uintptr(unsafe.Pointer(cPlaceholder)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddSelect(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_select.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddOption(label, value, className string) *Widget {
	cLabel, err := syscall.BytePtrFromString(label)
	if err != nil {
		return nil
	}
	cValue, err := syscall.BytePtrFromString(value)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_option.Call(w.handle, uintptr(unsafe.Pointer(cLabel)), uintptr(unsafe.Pointer(cValue)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddElement(tagName, text, className string) *Widget {
	cTag, err := syscall.BytePtrFromString(tagName)
	if err != nil {
		return nil
	}
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_element.Call(w.handle, uintptr(unsafe.Pointer(cTag)), uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddForm(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_form.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddFieldset(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_fieldset.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddLabel(text, className string) *Widget {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_label.Call(w.handle, uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddLegend(text, className string) *Widget {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_legend.Call(w.handle, uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddInput(inputType, placeholder, className string) *Widget {
	cType, err := syscall.BytePtrFromString(inputType)
	if err != nil {
		return nil
	}
	cPlaceholder, err := syscall.BytePtrFromString(placeholder)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_input.Call(w.handle, uintptr(unsafe.Pointer(cType)), uintptr(unsafe.Pointer(cPlaceholder)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddPasswordInput(placeholder, className string) *Widget {
	cPlaceholder, err := syscall.BytePtrFromString(placeholder)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_password_input.Call(w.handle, uintptr(unsafe.Pointer(cPlaceholder)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddTextarea(placeholder, className string) *Widget {
	cPlaceholder, err := syscall.BytePtrFromString(placeholder)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_textarea.Call(w.handle, uintptr(unsafe.Pointer(cPlaceholder)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddCheckbox(checked bool, className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	chk := 0
	if checked {
		chk = 1
	}
	r1, _, _ := fluxui_widget_add_checkbox.Call(w.handle, uintptr(chk), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddRadio(checked bool, group, className string) *Widget {
	cGroup, err := syscall.BytePtrFromString(group)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	chk := 0
	if checked {
		chk = 1
	}
	r1, _, _ := fluxui_widget_add_radio.Call(w.handle, uintptr(chk), uintptr(unsafe.Pointer(cGroup)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddRange(value, min, max, step float32, className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_range.Call(
		w.handle,
		uintptr(math.Float32bits(value)),
		uintptr(math.Float32bits(min)),
		uintptr(math.Float32bits(max)),
		uintptr(math.Float32bits(step)),
		uintptr(unsafe.Pointer(cClass)),
	)
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddAnchor(text, href, className string) *Widget {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cHref, err := syscall.BytePtrFromString(href)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_anchor.Call(w.handle, uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cHref)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddDetails(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_details.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddSummary(text, className string) *Widget {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_summary.Call(w.handle, uintptr(unsafe.Pointer(cText)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddDialog(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_dialog.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddMeter(value, min, max float32, className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_meter.Call(
		w.handle,
		uintptr(math.Float32bits(value)),
		uintptr(math.Float32bits(min)),
		uintptr(math.Float32bits(max)),
		uintptr(unsafe.Pointer(cClass)),
	)
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddProgressElement(value, max float32, className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_progress_element.Call(
		w.handle,
		uintptr(math.Float32bits(value)),
		uintptr(math.Float32bits(max)),
		uintptr(unsafe.Pointer(cClass)),
	)
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddHr(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_hr.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddBr(className string) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_br.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddIcon(glyph, className string) *Widget {
	cGlyph, err := syscall.BytePtrFromString(glyph)
	if err != nil {
		return nil
	}
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_icon.Call(w.handle, uintptr(unsafe.Pointer(cGlyph)), uintptr(unsafe.Pointer(cClass)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddProgressBar(className string, progress float32) *Widget {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_progress_bar.Call(w.handle, uintptr(unsafe.Pointer(cClass)), uintptr(math.Float32bits(progress)))
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) AddStatCard(title, value, subtitle string, accent Color) *Widget {
	cTitle, err := syscall.BytePtrFromString(title)
	if err != nil {
		return nil
	}
	cValue, err := syscall.BytePtrFromString(value)
	if err != nil {
		return nil
	}
	cSubtitle, err := syscall.BytePtrFromString(subtitle)
	if err != nil {
		return nil
	}
	r1, _, _ := fluxui_widget_add_stat_card.Call(
		w.handle,
		uintptr(unsafe.Pointer(cTitle)),
		uintptr(unsafe.Pointer(cValue)),
		uintptr(unsafe.Pointer(cSubtitle)),
		uintptr(unsafe.Pointer(&accent)),
	)
	if r1 == 0 {
		return nil
	}
	return &Widget{handle: r1}
}

func (w *Widget) SetContent(text string) {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return
	}
	fluxui_text_set_content.Call(w.handle, uintptr(unsafe.Pointer(cText)))
}

func (w *Widget) SetButtonLabel(label string) {
	cLabel, err := syscall.BytePtrFromString(label)
	if err != nil {
		return
	}
	fluxui_button_set_label.Call(w.handle, uintptr(unsafe.Pointer(cLabel)))
}

func (w *Widget) SetInputValue(value string) {
	cValue, err := syscall.BytePtrFromString(value)
	if err != nil {
		return
	}
	fluxui_text_input_set_value.Call(w.handle, uintptr(unsafe.Pointer(cValue)))
}

func (w *Widget) GetInputValue() string {
	r1, _, _ := fluxui_text_input_get_value.Call(w.handle)
	return getString(r1)
}

func (w *Widget) SetInputPlaceholder(placeholder string) {
	cPlaceholder, err := syscall.BytePtrFromString(placeholder)
	if err != nil {
		return
	}
	fluxui_text_input_set_placeholder.Call(w.handle, uintptr(unsafe.Pointer(cPlaceholder)))
}

func (w *Widget) SetSelectIndex(index uint32) {
	fluxui_select_set_selected_index.Call(w.handle, uintptr(index))
}

func (w *Widget) GetSelectIndex() uint32 {
	r1, _, _ := fluxui_select_get_selected_index.Call(w.handle)
	return uint32(r1)
}

func (w *Widget) SetInputType(inputType string) {
	cType, err := syscall.BytePtrFromString(inputType)
	if err != nil {
		return
	}
	fluxui_text_input_set_type.Call(w.handle, uintptr(unsafe.Pointer(cType)))
}

func (w *Widget) SetCheckboxChecked(checked bool) {
	val := 0
	if checked {
		val = 1
	}
	fluxui_checkbox_set_checked.Call(w.handle, uintptr(val))
}

func (w *Widget) GetCheckboxChecked() bool {
	r1, _, _ := fluxui_checkbox_get_checked.Call(w.handle)
	return r1 != 0
}

func (w *Widget) SetRadioChecked(checked bool) {
	val := 0
	if checked {
		val = 1
	}
	fluxui_radio_set_checked.Call(w.handle, uintptr(val))
}

func (w *Widget) GetRadioChecked() bool {
	r1, _, _ := fluxui_radio_get_checked.Call(w.handle)
	return r1 != 0
}

func (w *Widget) SetRangeValue(val float32) {
	fluxui_range_set_value.Call(w.handle, uintptr(math.Float32bits(val)))
}

func (w *Widget) GetRangeValue() float32 {
	r1, _, _ := fluxui_range_get_value.Call(w.handle)
	return math.Float32frombits(uint32(r1))
}

func (w *Widget) SetDetailsOpen(open bool) {
	val := 0
	if open {
		val = 1
	}
	fluxui_details_set_open.Call(w.handle, uintptr(val))
}

func (w *Widget) GetDetailsOpen() bool {
	r1, _, _ := fluxui_details_get_open.Call(w.handle)
	return r1 != 0
}

func (w *Widget) DialogShow() {
	fluxui_dialog_show.Call(w.handle)
}

func (w *Widget) DialogShowModal() {
	fluxui_dialog_show_modal.Call(w.handle)
}

func (w *Widget) DialogClose() {
	fluxui_dialog_close.Call(w.handle)
}

func (w *Widget) DialogGetOpen() bool {
	r1, _, _ := fluxui_dialog_get_open.Call(w.handle)
	return r1 != 0
}

func (w *Widget) SetMeterValue(val float32) {
	fluxui_meter_set_value.Call(w.handle, uintptr(math.Float32bits(val)))
}

func (w *Widget) GetMeterValue() float32 {
	r1, _, _ := fluxui_meter_get_value.Call(w.handle)
	return math.Float32frombits(uint32(r1))
}

func (w *Widget) SetProgressValue(val float32) {
	fluxui_progress_element_set_value.Call(w.handle, uintptr(math.Float32bits(val)))
}

func (w *Widget) GetProgressValue() float32 {
	r1, _, _ := fluxui_progress_element_get_value.Call(w.handle)
	return math.Float32frombits(uint32(r1))
}

func (w *Widget) SetIconGlyph(glyph string) {
	cGlyph, err := syscall.BytePtrFromString(glyph)
	if err != nil {
		return
	}
	fluxui_icon_set_glyph.Call(w.handle, uintptr(unsafe.Pointer(cGlyph)))
}

func (w *Widget) SetProgressBarValue(val float32) {
	fluxui_progress_bar_set_value.Call(w.handle, uintptr(math.Float32bits(val)))
}

func (w *Widget) SetProgressBarColor(accent Color) {
	fluxui_progress_bar_set_color.Call(w.handle, uintptr(unsafe.Pointer(&accent)))
}

func (w *Widget) SetID(id string) {
	cID, err := syscall.BytePtrFromString(id)
	if err != nil {
		return
	}
	fluxui_widget_set_id.Call(w.handle, uintptr(unsafe.Pointer(cID)))
}

func (w *Widget) SetClass(className string) {
	cClass, err := syscall.BytePtrFromString(className)
	if err != nil {
		return
	}
	fluxui_widget_set_class.Call(w.handle, uintptr(unsafe.Pointer(cClass)))
}

func (w *Widget) SetVisible(visible bool) {
	val := 0
	if visible {
		val = 1
	}
	fluxui_widget_set_visible.Call(w.handle, uintptr(val))
}

func (w *Widget) GetBounds() Rect {
	var r Rect
	fluxui_widget_get_bounds.Call(uintptr(unsafe.Pointer(&r)), w.handle)
	return r
}

func (w *Widget) ClearChildren() {
	fluxui_widget_clear_children.Call(w.handle)
}

func (w *Widget) StyleWidth(px float32) {
	fluxui_style_width_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleHeight(px float32) {
	fluxui_style_height_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleMinWidth(px float32) {
	fluxui_style_min_width_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleMinHeight(px float32) {
	fluxui_style_min_height_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleMaxWidth(px float32) {
	fluxui_style_max_width_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleMaxHeight(px float32) {
	fluxui_style_max_height_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleFlexGrow(grow float32) {
	fluxui_style_flex_grow.Call(w.handle, uintptr(math.Float32bits(grow)))
}

func (w *Widget) StyleGap(px float32) {
	fluxui_style_gap_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StylePaddingAll(px float32) {
	fluxui_style_padding_all_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StylePadding(top, right, bottom, left float32) {
	fluxui_style_padding_px.Call(w.handle, uintptr(math.Float32bits(top)), uintptr(math.Float32bits(right)), uintptr(math.Float32bits(bottom)), uintptr(math.Float32bits(left)))
}

func (w *Widget) StyleMarginAll(px float32) {
	fluxui_style_margin_all_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleMargin(top, right, bottom, left float32) {
	fluxui_style_margin_px.Call(w.handle, uintptr(math.Float32bits(top)), uintptr(math.Float32bits(right)), uintptr(math.Float32bits(bottom)), uintptr(math.Float32bits(left)))
}

func (w *Widget) StyleBorderRadius(px float32) {
	fluxui_style_border_radius_px.Call(w.handle, uintptr(math.Float32bits(px)))
}

func (w *Widget) StyleBackgroundColor(r, g, b, a float32) {
	color := Color{R: r, G: g, B: b, A: a}
	fluxui_style_background_color.Call(w.handle, uintptr(unsafe.Pointer(&color)))
}

func (w *Widget) StyleTextColor(r, g, b, a float32) {
	color := Color{R: r, G: g, B: b, A: a}
	fluxui_style_text_color.Call(w.handle, uintptr(unsafe.Pointer(&color)))
}

var (
	clickCallbacks  = make(map[uintptr]func())
	updateCallbacks = make(map[uintptr]func(float32))
	eventCallbacks  = make(map[uintptr]func(*Event))
	drawCallbacks   = make(map[uintptr]func(*Renderer, Rect))
	callbackCounter uintptr
)

func goClickCallback(widget, userData uintptr) uintptr {
	if cb, ok := clickCallbacks[userData]; ok {
		cb()
	}
	return 0
}

func goUpdateCallback(appHandle, deltaTimeBits, userData uintptr) uintptr {
	if cb, ok := updateCallbacks[userData]; ok {
		dt := math.Float32frombits(uint32(deltaTimeBits))
		cb(dt)
	}
	return 0
}

func goEventCallback(appHandle, eventPtr, userData uintptr) uintptr {
	if cb, ok := eventCallbacks[userData]; ok {
		ce := (*cEventStruct)(unsafe.Pointer(eventPtr))
		ev := &Event{
			Type:          ce.Type,
			Target:        ce.Target,
			Name:          getString(ce.Name),
			Route:         getString(ce.Route),
			PreviousRoute: getString(ce.PreviousRoute),
			Text:          getString(ce.Text),
			X:             ce.X,
			Y:             ce.Y,
			Dx:            ce.Dx,
			Dy:            ce.Dy,
			KeyCode:       ce.KeyCode,
			Modifiers:     ce.Modifiers,
			Button:        ce.Button,
			ClickCount:    ce.ClickCount,
			Handled:       ce.Handled != 0,
		}
		cb(ev)
		if ev.Handled {
			ce.Handled = 1
		} else {
			ce.Handled = 0
		}
	}
	return 0
}

func goDrawCallback(canvasHandle, rendererPtr, boundsPtr, userData uintptr) uintptr {
	if cb, ok := drawCallbacks[userData]; ok {
		bounds := (*Rect)(unsafe.Pointer(boundsPtr))
		cb(&Renderer{handle: rendererPtr}, *bounds)
	}
	return 0
}

var (
	sysClickCallback  = syscall.NewCallback(goClickCallback)
	sysUpdateCallback = syscall.NewCallback(goUpdateCallback)
	sysEventCallback  = syscall.NewCallback(goEventCallback)
	sysDrawCallback   = syscall.NewCallback(goDrawCallback)
)

func (w *Widget) SetOnClick(cb func()) {
	callbackCounter++
	id := callbackCounter
	clickCallbacks[id] = cb
	fluxui_widget_set_on_click.Call(w.handle, sysClickCallback, id)
}

func (a *App) SetUpdateCallback(cb func(float32)) {
	updateCallbacks[a.handle] = cb
	fluxui_app_set_update_callback_go.Call(a.handle, sysUpdateCallback, a.handle)
}

func (a *App) OnEvent(eventType int32, cb func(*Event)) uint64 {
	callbackCounter++
	id := callbackCounter
	eventCallbacks[id] = cb
	r1, _, _ := fluxui_app_on_event.Call(a.handle, uintptr(eventType), sysEventCallback, id)
	return uint64(r1)
}

func (a *App) OffEvent(listenerId uint64) {
	fluxui_app_off_event.Call(a.handle, uintptr(listenerId))
}

func (w *Widget) SetOnDraw(cb func(*Renderer, Rect)) {
	callbackCounter++
	id := callbackCounter
	drawCallbacks[id] = cb
	fluxui_canvas_set_on_draw.Call(w.handle, sysDrawCallback, id)
}

func (r *Renderer) DrawRect(rect Rect, color Color) {
	fluxui_draw_rect.Call(r.handle, uintptr(unsafe.Pointer(&rect)), uintptr(unsafe.Pointer(&color)))
}

func (r *Renderer) DrawText(text string, x, y float32, color Color, fontSize float32) {
	cText, err := syscall.BytePtrFromString(text)
	if err != nil {
		return
	}
	fluxui_draw_text.Call(r.handle,
		uintptr(unsafe.Pointer(cText)),
		uintptr(math.Float32bits(x)),
		uintptr(math.Float32bits(y)),
		uintptr(unsafe.Pointer(&color)),
		uintptr(math.Float32bits(fontSize)),
	)
}

func (r *Renderer) DrawImage(nameOrPath string, rect Rect, opacity float32) {
	cName, err := syscall.BytePtrFromString(nameOrPath)
	if err != nil {
		return
	}
	fluxui_draw_image.Call(r.handle,
		uintptr(unsafe.Pointer(cName)),
		uintptr(unsafe.Pointer(&rect)),
		uintptr(math.Float32bits(opacity)),
	)
}

func (r *Renderer) Flush() {
	fluxui_renderer_flush.Call(r.handle)
}

func (a *App) GetHandle() uintptr {
	return a.handle
}

func (w *Widget) GetHandle() uintptr {
	return w.handle
}

func getString(p uintptr) string {
	if p == 0 {
		return ""
	}
	var buf []byte
	for i := 0; ; i++ {
		b := *(*byte)(unsafe.Pointer(p + uintptr(i)))
		if b == 0 {
			break
		}
		buf = append(buf, b)
	}
	return string(buf)
}
