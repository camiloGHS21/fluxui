import ctypes
import os
import sys

# Find and load the shared library
lib_name = "fluxui_shared"
if sys.platform == "win32":
    lib_file = f"{lib_name}.dll"
elif sys.platform == "darwin":
    lib_file = f"lib{lib_name}.dylib"
else:
    lib_file = f"lib{lib_name}.so"

# Search in the build/Release directory relative to the workspace, or in current directories.
possible_paths = [
    os.path.join(os.path.dirname(__file__), "../../build/Release", lib_file),
    os.path.join(os.getcwd(), "build/Release", lib_file),
    os.path.join(os.getcwd(), lib_file),
    lib_file
]

_lib = None
for p in possible_paths:
    if os.path.exists(p):
        try:
            _lib = ctypes.CDLL(p)
            break
        except Exception:
            pass

if _lib is None:
    try:
        _lib = ctypes.CDLL(lib_file)
    except Exception as e:
        raise ImportError(f"Could not load FluxUI shared library. Searched in: {possible_paths}. Error: {e}")

class FluxUIColor(ctypes.Structure):
    _fields_ = [
        ("r", ctypes.c_float),
        ("g", ctypes.c_float),
        ("b", ctypes.c_float),
        ("a", ctypes.c_float)
    ]

class FluxUIRect(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("w", ctypes.c_float),
        ("h", ctypes.c_float)
    ]

# Callback types
FluxUIClickCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)
FluxUIUpdateCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_float, ctypes.c_void_p)
FluxUIRouteCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p)
FluxUIDrawCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, FluxUIRect, ctypes.c_void_p)
FluxUIVirtualListItemCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p)

# Define ctypes prototypes
# Application lifecycle
_lib.fluxui_app_create.restype = ctypes.c_void_p
_lib.fluxui_app_create.argtypes = []

_lib.fluxui_app_destroy.restype = None
_lib.fluxui_app_destroy.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_set_backend.restype = ctypes.c_int
_lib.fluxui_app_set_backend.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.fluxui_app_get_backend.restype = ctypes.c_int
_lib.fluxui_app_get_backend.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_init.restype = ctypes.c_int
_lib.fluxui_app_init.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]

_lib.fluxui_app_shutdown.restype = None
_lib.fluxui_app_shutdown.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_run.restype = None
_lib.fluxui_app_run.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_stop.restype = None
_lib.fluxui_app_stop.argtypes = [ctypes.c_void_p]

# Application resources & callbacks
_lib.fluxui_app_load_stylesheet.restype = ctypes.c_int
_lib.fluxui_app_load_stylesheet.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_app_add_stylesheet.restype = None
_lib.fluxui_app_add_stylesheet.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_app_load_font.restype = ctypes.c_int
_lib.fluxui_app_load_font.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float]

_lib.fluxui_app_load_font_named.restype = ctypes.c_int
_lib.fluxui_app_load_font_named.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float, ctypes.c_char_p]

_lib.fluxui_app_load_default_font.restype = ctypes.c_int
_lib.fluxui_app_load_default_font.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_app_warm_font_cache.restype = None
_lib.fluxui_app_warm_font_cache.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_uint32, ctypes.c_char_p]

_lib.fluxui_app_release_font_sources.restype = None
_lib.fluxui_app_release_font_sources.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_set_update_callback.restype = None
_lib.fluxui_app_set_update_callback.argtypes = [ctypes.c_void_p, FluxUIUpdateCallback, ctypes.c_void_p]

_lib.fluxui_app_root.restype = ctypes.c_void_p
_lib.fluxui_app_root.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_emit_custom_event.restype = None
_lib.fluxui_app_emit_custom_event.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_app_add_route.restype = None
_lib.fluxui_app_add_route.argtypes = [ctypes.c_void_p, ctypes.c_char_p, FluxUIRouteCallback, ctypes.c_void_p]

_lib.fluxui_app_navigate.restype = ctypes.c_int
_lib.fluxui_app_navigate.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_app_current_route.restype = ctypes.c_char_p
_lib.fluxui_app_current_route.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_route_dirty.restype = ctypes.c_int
_lib.fluxui_app_route_dirty.argtypes = [ctypes.c_void_p]

_lib.fluxui_app_render_route.restype = ctypes.c_int
_lib.fluxui_app_render_route.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

# Tree mutation
_lib.fluxui_widget_clear_children.restype = None
_lib.fluxui_widget_clear_children.argtypes = [ctypes.c_void_p]

_lib.fluxui_widget_reserve_children.restype = None
_lib.fluxui_widget_reserve_children.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

_lib.fluxui_widget_add_element.restype = ctypes.c_void_p
_lib.fluxui_widget_add_element.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_panel.restype = ctypes.c_void_p
_lib.fluxui_widget_add_panel.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_form.restype = ctypes.c_void_p
_lib.fluxui_widget_add_form.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_fieldset.restype = ctypes.c_void_p
_lib.fluxui_widget_add_fieldset.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_text.restype = ctypes.c_void_p
_lib.fluxui_widget_add_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_label.restype = ctypes.c_void_p
_lib.fluxui_widget_add_label.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_legend.restype = ctypes.c_void_p
_lib.fluxui_widget_add_legend.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_button.restype = ctypes.c_void_p
_lib.fluxui_widget_add_button.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_text_input.restype = ctypes.c_void_p
_lib.fluxui_widget_add_text_input.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_input.restype = ctypes.c_void_p
_lib.fluxui_widget_add_input.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_password_input.restype = ctypes.c_void_p
_lib.fluxui_widget_add_password_input.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_textarea.restype = ctypes.c_void_p
_lib.fluxui_widget_add_textarea.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_checkbox.restype = ctypes.c_void_p
_lib.fluxui_widget_add_checkbox.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p]

_lib.fluxui_widget_add_radio.restype = ctypes.c_void_p
_lib.fluxui_widget_add_radio.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_range.restype = ctypes.c_void_p
_lib.fluxui_widget_add_range.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_char_p]

_lib.fluxui_widget_add_select.restype = ctypes.c_void_p
_lib.fluxui_widget_add_select.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_option.restype = ctypes.c_void_p
_lib.fluxui_widget_add_option.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_anchor.restype = ctypes.c_void_p
_lib.fluxui_widget_add_anchor.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_details.restype = ctypes.c_void_p
_lib.fluxui_widget_add_details.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_summary.restype = ctypes.c_void_p
_lib.fluxui_widget_add_summary.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_dialog.restype = ctypes.c_void_p
_lib.fluxui_widget_add_dialog.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_meter.restype = ctypes.c_void_p
_lib.fluxui_widget_add_meter.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_char_p]

_lib.fluxui_widget_add_progress_element.restype = ctypes.c_void_p
_lib.fluxui_widget_add_progress_element.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_char_p]

_lib.fluxui_widget_add_hr.restype = ctypes.c_void_p
_lib.fluxui_widget_add_hr.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_br.restype = ctypes.c_void_p
_lib.fluxui_widget_add_br.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_icon.restype = ctypes.c_void_p
_lib.fluxui_widget_add_icon.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

_lib.fluxui_widget_add_progress_bar.restype = ctypes.c_void_p
_lib.fluxui_widget_add_progress_bar.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float]

_lib.fluxui_widget_add_stat_card.restype = ctypes.c_void_p
_lib.fluxui_widget_add_stat_card.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, FluxUIColor]

_lib.fluxui_widget_add_canvas.restype = ctypes.c_void_p
_lib.fluxui_widget_add_canvas.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_add_virtual_list.restype = ctypes.c_void_p
_lib.fluxui_widget_add_virtual_list.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_float, FluxUIVirtualListItemCallback, ctypes.c_void_p]

_lib.fluxui_virtual_list_set_item_count.restype = None
_lib.fluxui_virtual_list_set_item_count.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

_lib.fluxui_virtual_list_refresh.restype = None
_lib.fluxui_virtual_list_refresh.argtypes = [ctypes.c_void_p]

_lib.fluxui_virtual_list_scroll_to_index.restype = None
_lib.fluxui_virtual_list_scroll_to_index.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int]

_lib.fluxui_canvas_set_on_draw.restype = None
_lib.fluxui_canvas_set_on_draw.argtypes = [ctypes.c_void_p, FluxUIDrawCallback, ctypes.c_void_p]

# Canvas 2D Drawing Primitives
_lib.fluxui_draw_rect.restype = None
_lib.fluxui_draw_rect.argtypes = [ctypes.c_void_p, FluxUIRect, FluxUIColor]

_lib.fluxui_draw_text.restype = None
_lib.fluxui_draw_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_float, ctypes.c_float, FluxUIColor, ctypes.c_float]

_lib.fluxui_draw_image.restype = None
_lib.fluxui_draw_image.argtypes = [ctypes.c_void_p, ctypes.c_char_p, FluxUIRect, ctypes.c_float]

_lib.fluxui_renderer_flush.restype = None
_lib.fluxui_renderer_flush.argtypes = [ctypes.c_void_p]

# Widget state
_lib.fluxui_widget_set_id.restype = None
_lib.fluxui_widget_set_id.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_set_class.restype = None
_lib.fluxui_widget_set_class.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_widget_set_visible.restype = None
_lib.fluxui_widget_set_visible.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.fluxui_widget_get_bounds.restype = FluxUIRect
_lib.fluxui_widget_get_bounds.argtypes = [ctypes.c_void_p]

_lib.fluxui_widget_set_on_click.restype = None
_lib.fluxui_widget_set_on_click.argtypes = [ctypes.c_void_p, FluxUIClickCallback, ctypes.c_void_p]

# Text-like widgets state
_lib.fluxui_text_set_content.restype = None
_lib.fluxui_text_set_content.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_button_set_label.restype = None
_lib.fluxui_button_set_label.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_text_input_set_value.restype = None
_lib.fluxui_text_input_set_value.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_text_input_get_value.restype = ctypes.c_char_p
_lib.fluxui_text_input_get_value.argtypes = [ctypes.c_void_p]

_lib.fluxui_text_input_set_placeholder.restype = None
_lib.fluxui_text_input_set_placeholder.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_text_input_set_type.restype = None
_lib.fluxui_text_input_set_type.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_checkbox_set_checked.restype = None
_lib.fluxui_checkbox_set_checked.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.fluxui_checkbox_get_checked.restype = ctypes.c_int
_lib.fluxui_checkbox_get_checked.argtypes = [ctypes.c_void_p]

_lib.fluxui_radio_set_checked.restype = None
_lib.fluxui_radio_set_checked.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.fluxui_radio_get_checked.restype = ctypes.c_int
_lib.fluxui_radio_get_checked.argtypes = [ctypes.c_void_p]

_lib.fluxui_range_set_value.restype = None
_lib.fluxui_range_set_value.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_range_get_value.restype = ctypes.c_float
_lib.fluxui_range_get_value.argtypes = [ctypes.c_void_p]

_lib.fluxui_select_set_selected_index.restype = None
_lib.fluxui_select_set_selected_index.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

_lib.fluxui_select_get_selected_index.restype = ctypes.c_uint32
_lib.fluxui_select_get_selected_index.argtypes = [ctypes.c_void_p]

_lib.fluxui_details_set_open.restype = None
_lib.fluxui_details_set_open.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.fluxui_details_get_open.restype = ctypes.c_int
_lib.fluxui_details_get_open.argtypes = [ctypes.c_void_p]

_lib.fluxui_dialog_show.restype = None
_lib.fluxui_dialog_show.argtypes = [ctypes.c_void_p]

_lib.fluxui_dialog_show_modal.restype = None
_lib.fluxui_dialog_show_modal.argtypes = [ctypes.c_void_p]

_lib.fluxui_dialog_close.restype = None
_lib.fluxui_dialog_close.argtypes = [ctypes.c_void_p]

_lib.fluxui_dialog_get_open.restype = ctypes.c_int
_lib.fluxui_dialog_get_open.argtypes = [ctypes.c_void_p]

_lib.fluxui_meter_set_value.restype = None
_lib.fluxui_meter_set_value.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_meter_get_value.restype = ctypes.c_float
_lib.fluxui_meter_get_value.argtypes = [ctypes.c_void_p]

_lib.fluxui_progress_element_set_value.restype = None
_lib.fluxui_progress_element_set_value.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_progress_element_get_value.restype = ctypes.c_float
_lib.fluxui_progress_element_get_value.argtypes = [ctypes.c_void_p]

_lib.fluxui_icon_set_glyph.restype = None
_lib.fluxui_icon_set_glyph.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

_lib.fluxui_progress_bar_set_value.restype = None
_lib.fluxui_progress_bar_set_value.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_progress_bar_set_color.restype = None
_lib.fluxui_progress_bar_set_color.argtypes = [ctypes.c_void_p, FluxUIColor]

# Style helpers
_lib.fluxui_color_rgba.restype = FluxUIColor
_lib.fluxui_color_rgba.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]

_lib.fluxui_color_rgb_u8.restype = FluxUIColor
_lib.fluxui_color_rgb_u8.argtypes = [ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]

_lib.fluxui_style_width_px.restype = None
_lib.fluxui_style_width_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_height_px.restype = None
_lib.fluxui_style_height_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_min_width_px.restype = None
_lib.fluxui_style_min_width_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_min_height_px.restype = None
_lib.fluxui_style_min_height_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_max_width_px.restype = None
_lib.fluxui_style_max_width_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_max_height_px.restype = None
_lib.fluxui_style_max_height_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_flex_grow.restype = None
_lib.fluxui_style_flex_grow.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_gap_px.restype = None
_lib.fluxui_style_gap_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_padding_all_px.restype = None
_lib.fluxui_style_padding_all_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_padding_px.restype = None
_lib.fluxui_style_padding_px.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]

_lib.fluxui_style_margin_all_px.restype = None
_lib.fluxui_style_margin_all_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_margin_px.restype = None
_lib.fluxui_style_margin_px.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float]

_lib.fluxui_style_border_radius_px.restype = None
_lib.fluxui_style_border_radius_px.argtypes = [ctypes.c_void_p, ctypes.c_float]

_lib.fluxui_style_background_color.restype = None
_lib.fluxui_style_background_color.argtypes = [ctypes.c_void_p, FluxUIColor]

_lib.fluxui_style_text_color.restype = None
_lib.fluxui_style_text_color.argtypes = [ctypes.c_void_p, FluxUIColor]

_lib.fluxui_widget_css.restype = None
_lib.fluxui_widget_css.argtypes = [ctypes.c_void_p, ctypes.c_char_p]


class Renderer:
    def __init__(self, handle):
        self.handle = handle

    def draw_rect(self, x, y, w, h, r, g, b, a=1.0):
        rect = FluxUIRect(x, y, w, h)
        color = FluxUIColor(r, g, b, a)
        _lib.fluxui_draw_rect(self.handle, rect, color)

    def draw_text(self, text, x, y, r, g, b, a=1.0, font_size=14.0):
        color = FluxUIColor(r, g, b, a)
        _lib.fluxui_draw_text(self.handle, text.encode('utf-8'), float(x), float(y), color, float(font_size))

    def draw_image(self, name_or_path, x, y, w, h, opacity=1.0):
        rect = FluxUIRect(x, y, w, h)
        _lib.fluxui_draw_image(self.handle, name_or_path.encode('utf-8'), rect, float(opacity))

    def flush(self):
        _lib.fluxui_renderer_flush(self.handle)


class Widget:
    def __init__(self, handle):
        self.handle = handle
        self._click_cb = None
        self._draw_cb = None
        self._list_item_cb = None

    def clear_children(self):
        _lib.fluxui_widget_clear_children(self.handle)

    def reserve_children(self, count):
        _lib.fluxui_widget_reserve_children(self.handle, int(count))

    def set_id(self, widget_id):
        _lib.fluxui_widget_set_id(self.handle, widget_id.encode('utf-8'))

    def set_class(self, class_name):
        _lib.fluxui_widget_set_class(self.handle, class_name.encode('utf-8'))

    def set_visible(self, visible):
        _lib.fluxui_widget_set_visible(self.handle, 1 if visible else 0)

    def get_bounds(self):
        bounds = _lib.fluxui_widget_get_bounds(self.handle)
        return (bounds.x, bounds.y, bounds.w, bounds.h)

    # Tag additions
    def add_element(self, tag_name, text="", class_name=""):
        res = _lib.fluxui_widget_add_element(self.handle, tag_name.encode('utf-8'), text.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_panel(self, class_name=""):
        res = _lib.fluxui_widget_add_panel(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_form(self, class_name=""):
        res = _lib.fluxui_widget_add_form(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_fieldset(self, class_name=""):
        res = _lib.fluxui_widget_add_fieldset(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_text(self, text, class_name=""):
        res = _lib.fluxui_widget_add_text(self.handle, text.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_label(self, text, class_name=""):
        res = _lib.fluxui_widget_add_label(self.handle, text.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_legend(self, text, class_name=""):
        res = _lib.fluxui_widget_add_legend(self.handle, text.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_button(self, label, class_name=""):
        res = _lib.fluxui_widget_add_button(self.handle, label.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_text_input(self, placeholder="", class_name=""):
        res = _lib.fluxui_widget_add_text_input(self.handle, placeholder.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_input(self, input_type, placeholder="", class_name=""):
        res = _lib.fluxui_widget_add_input(self.handle, input_type.encode('utf-8'), placeholder.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_password_input(self, placeholder="", class_name=""):
        res = _lib.fluxui_widget_add_password_input(self.handle, placeholder.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_textarea(self, placeholder="", class_name=""):
        res = _lib.fluxui_widget_add_textarea(self.handle, placeholder.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_checkbox(self, checked=False, class_name=""):
        res = _lib.fluxui_widget_add_checkbox(self.handle, 1 if checked else 0, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_radio(self, checked=False, group="", class_name=""):
        res = _lib.fluxui_widget_add_radio(self.handle, 1 if checked else 0, group.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_range(self, value=0.0, min_val=0.0, max_val=100.0, step=1.0, class_name=""):
        res = _lib.fluxui_widget_add_range(self.handle, float(value), float(min_val), float(max_val), float(step), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_select(self, class_name=""):
        res = _lib.fluxui_widget_add_select(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_option(self, label, value, class_name=""):
        res = _lib.fluxui_widget_add_option(self.handle, label.encode('utf-8'), value.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_anchor(self, text, href, class_name=""):
        res = _lib.fluxui_widget_add_anchor(self.handle, text.encode('utf-8'), href.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_details(self, class_name=""):
        res = _lib.fluxui_widget_add_details(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_summary(self, text, class_name=""):
        res = _lib.fluxui_widget_add_summary(self.handle, text.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_dialog(self, class_name=""):
        res = _lib.fluxui_widget_add_dialog(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_meter(self, value, min_val=0.0, max_val=100.0, class_name=""):
        res = _lib.fluxui_widget_add_meter(self.handle, float(value), float(min_val), float(max_val), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_progress_element(self, value, max_val=100.0, class_name=""):
        res = _lib.fluxui_widget_add_progress_element(self.handle, float(value), float(max_val), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_hr(self, class_name=""):
        res = _lib.fluxui_widget_add_hr(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_br(self, class_name=""):
        res = _lib.fluxui_widget_add_br(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_icon(self, glyph, class_name=""):
        res = _lib.fluxui_widget_add_icon(self.handle, glyph.encode('utf-8'), class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_progress_bar(self, class_name="", progress=0.0):
        res = _lib.fluxui_widget_add_progress_bar(self.handle, class_name.encode('utf-8'), float(progress))
        return Widget(res) if res else None

    def add_stat_card(self, title, value, subtitle="", accent_r=0.0, accent_g=0.0, accent_b=0.0, accent_a=1.0):
        accent = FluxUIColor(accent_r, accent_g, accent_b, accent_a)
        res = _lib.fluxui_widget_add_stat_card(self.handle, title.encode('utf-8'), value.encode('utf-8'), subtitle.encode('utf-8'), accent)
        return Widget(res) if res else None

    def add_canvas(self, class_name=""):
        res = _lib.fluxui_widget_add_canvas(self.handle, class_name.encode('utf-8'))
        return Widget(res) if res else None

    def add_virtual_list(self, class_name, item_count, item_height, callback):
        def native_cb(item_ptr, index, user_data):
            callback(Widget(item_ptr), int(index))

        self._list_item_cb = FluxUIVirtualListItemCallback(native_cb)
        res = _lib.fluxui_widget_add_virtual_list(self.handle, class_name.encode('utf-8'), int(item_count), float(item_height), self._list_item_cb, None)
        return Widget(res) if res else None

    def set_virtual_list_item_count(self, item_count):
        _lib.fluxui_virtual_list_set_item_count(self.handle, int(item_count))

    def refresh_virtual_list(self):
        _lib.fluxui_virtual_list_refresh(self.handle)

    def scroll_virtual_list_to_index(self, index, strategy=0):
        _lib.fluxui_virtual_list_scroll_to_index(self.handle, int(index), int(strategy))

    # Canvas Drawing hook
    def set_on_draw(self, callback):
        def native_draw(widget_ptr, renderer_ptr, bounds, user_data):
            callback(self, Renderer(renderer_ptr), (bounds.x, bounds.y, bounds.w, bounds.h))

        self._draw_cb = FluxUIDrawCallback(native_draw)
        _lib.fluxui_canvas_set_on_draw(self.handle, self._draw_cb, None)

    # State Modifiers/Queries
    def set_content(self, text):
        _lib.fluxui_text_set_content(self.handle, text.encode('utf-8'))

    def set_label(self, label):
        _lib.fluxui_button_set_label(self.handle, label.encode('utf-8'))

    def set_value(self, value):
        _lib.fluxui_text_input_set_value(self.handle, value.encode('utf-8'))

    def get_value(self):
        res = _lib.fluxui_text_input_get_value(self.handle)
        return res.decode('utf-8') if res else ""

    def set_placeholder(self, placeholder):
        _lib.fluxui_text_input_set_placeholder(self.handle, placeholder.encode('utf-8'))

    def set_type(self, input_type):
        _lib.fluxui_text_input_set_type(self.handle, input_type.encode('utf-8'))

    def set_checked(self, checked):
        val = 1 if checked else 0
        _lib.fluxui_checkbox_set_checked(self.handle, val)
        _lib.fluxui_radio_set_checked(self.handle, val)

    def get_checked(self):
        return (_lib.fluxui_checkbox_get_checked(self.handle) != 0 or
                _lib.fluxui_radio_get_checked(self.handle) != 0)

    def set_range_value(self, value):
        _lib.fluxui_range_set_value(self.handle, float(value))

    def get_range_value(self):
        return _lib.fluxui_range_get_value(self.handle)

    def set_selected_index(self, index):
        _lib.fluxui_select_set_selected_index(self.handle, int(index))

    def get_selected_index(self):
        return _lib.fluxui_select_get_selected_index(self.handle)

    def set_open(self, open_state):
        _lib.fluxui_details_set_open(self.handle, 1 if open_state else 0)

    def get_open(self):
        return _lib.fluxui_details_get_open(self.handle) != 0

    def dialog_show(self):
        _lib.fluxui_dialog_show(self.handle)

    def dialog_show_modal(self):
        _lib.fluxui_dialog_show_modal(self.handle)

    def dialog_close(self):
        _lib.fluxui_dialog_close(self.handle)

    def dialog_get_open(self):
        return _lib.fluxui_dialog_get_open(self.handle) != 0

    def set_meter_value(self, value):
        _lib.fluxui_meter_set_value(self.handle, float(value))

    def get_meter_value(self):
        return _lib.fluxui_meter_get_value(self.handle)

    def set_progress_value(self, value):
        _lib.fluxui_progress_element_set_value(self.handle, float(value))

    def get_progress_value(self):
        return _lib.fluxui_progress_element_get_value(self.handle)

    def set_glyph(self, glyph):
        _lib.fluxui_icon_set_glyph(self.handle, glyph.encode('utf-8'))

    def set_progress_bar_value(self, progress):
        _lib.fluxui_progress_bar_set_value(self.handle, float(progress))

    def set_progress_bar_color(self, r, g, b, a=1.0):
        color = FluxUIColor(r, g, b, a)
        _lib.fluxui_progress_bar_set_color(self.handle, color)

    # Style wrappers
    def style_width(self, px):
        _lib.fluxui_style_width_px(self.handle, float(px))

    def style_height(self, px):
        _lib.fluxui_style_height_px(self.handle, float(px))

    def style_min_width(self, px):
        _lib.fluxui_style_min_width_px(self.handle, float(px))

    def style_min_height(self, px):
        _lib.fluxui_style_min_height_px(self.handle, float(px))

    def style_max_width(self, px):
        _lib.fluxui_style_max_width_px(self.handle, float(px))

    def style_max_height(self, px):
        _lib.fluxui_style_max_height_px(self.handle, float(px))

    def style_flex_grow(self, val):
        _lib.fluxui_style_flex_grow(self.handle, float(val))

    def style_gap(self, px):
        _lib.fluxui_style_gap_px(self.handle, float(px))

    def style_padding_all(self, px):
        _lib.fluxui_style_padding_all_px(self.handle, float(px))

    def style_padding(self, top, right, bottom, left):
        _lib.fluxui_style_padding_px(self.handle, float(top), float(right), float(bottom), float(left))

    def style_margin_all(self, px):
        _lib.fluxui_style_margin_all_px(self.handle, float(px))

    def style_margin(self, top, right, bottom, left):
        _lib.fluxui_style_margin_px(self.handle, float(top), float(right), float(bottom), float(left))

    def style_border_radius(self, px):
        _lib.fluxui_style_border_radius_px(self.handle, float(px))

    def style_background_color(self, r, g, b, a=1.0):
        color = FluxUIColor(r, g, b, a)
        _lib.fluxui_style_background_color(self.handle, color)

    def style_text_color(self, r, g, b, a=1.0):
        color = FluxUIColor(r, g, b, a)
        _lib.fluxui_style_text_color(self.handle, color)

    def set_on_click(self, callback):
        # Store callback reference on self to prevent GC
        self._click_cb = FluxUIClickCallback(lambda w, u: callback())
        _lib.fluxui_widget_set_on_click(self.handle, self._click_cb, None)

    def css(self, declarations):
        _lib.fluxui_widget_css(self.handle, declarations.encode('utf-8'))


class App:
    def __init__(self):
        self.handle = _lib.fluxui_app_create()
        if not self.handle:
            raise RuntimeError("Failed to create FluxUI App")
        self._update_cb = None
        self._routes = {}

    def __del__(self):
        if hasattr(self, 'handle') and self.handle:
            _lib.fluxui_app_destroy(self.handle)

    def set_backend(self, backend_id):
        return _lib.fluxui_app_set_backend(self.handle, int(backend_id)) != 0

    def get_backend(self):
        return _lib.fluxui_app_get_backend(self.handle)

    def init(self, title, width, height):
        return _lib.fluxui_app_init(self.handle, title.encode('utf-8'), width, height) != 0

    def shutdown(self):
        _lib.fluxui_app_shutdown(self.handle)

    def run(self):
        _lib.fluxui_app_run(self.handle)

    def stop(self):
        _lib.fluxui_app_stop(self.handle)

    def load_stylesheet(self, path):
        return _lib.fluxui_app_load_stylesheet(self.handle, path.encode('utf-8')) != 0

    def add_stylesheet(self, css):
        _lib.fluxui_app_add_stylesheet(self.handle, css.encode('utf-8'))

    def load_font(self, path, size):
        return _lib.fluxui_app_load_font(self.handle, path.encode('utf-8'), float(size)) != 0

    def load_font_named(self, path, size, name):
        return _lib.fluxui_app_load_font_named(self.handle, path.encode('utf-8'), float(size), name.encode('utf-8')) != 0

    def load_default_font(self, size):
        return _lib.fluxui_app_load_default_font(self.handle, float(size)) != 0

    def warm_font_cache(self, sizes, name):
        count = len(sizes)
        float_arr_type = ctypes.c_float * count
        arr = float_arr_type(*sizes)
        _lib.fluxui_app_warm_font_cache(self.handle, arr, count, name.encode('utf-8'))

    def release_font_sources(self):
        _lib.fluxui_app_release_font_sources(self.handle)

    def set_update_callback(self, callback):
        def native_update(app_ptr, dt, user_data):
            callback(float(dt))
        self._update_cb = FluxUIUpdateCallback(native_update)
        _lib.fluxui_app_set_update_callback(self.handle, self._update_cb, None)

    def root(self):
        res = _lib.fluxui_app_root(self.handle)
        return Widget(res) if res else None

    def emit_custom_event(self, name, text):
        _lib.fluxui_app_emit_custom_event(self.handle, name.encode('utf-8'), text.encode('utf-8'))

    def add_route(self, path, callback):
        def native_route(app_ptr, container_ptr, route_c_str, user_data):
            route_str = route_c_str.decode('utf-8')
            callback(Widget(container_ptr), route_str)

        # Store callback object to avoid GC
        cb_obj = FluxUIRouteCallback(native_route)
        self._routes[path] = cb_obj
        _lib.fluxui_app_add_route(self.handle, path.encode('utf-8'), cb_obj, None)

    def navigate(self, path):
        return _lib.fluxui_app_navigate(self.handle, path.encode('utf-8')) != 0

    def current_route(self):
        res = _lib.fluxui_app_current_route(self.handle)
        return res.decode('utf-8') if res else ""

    def route_dirty(self):
        return _lib.fluxui_app_route_dirty(self.handle) != 0

    def render_route(self, container):
        return _lib.fluxui_app_render_route(self.handle, container.handle) != 0


# ============================================================
#  Declarative DSL — modern HTML/Blink-named functional builder.
#
#  Element names match HTML exactly (Div, Span, P, H1..H6, Nav, Section, Button,
#  Input, A, Img, Ul, Li, ...). Layout is expressed in CSS (display:flex/grid)
#  exactly like the browser — there are no bespoke Row/Column nodes. Every node
#  routes through Widget.add_element(tag, ...) for Blink UA parity.
#
#  Example:
#      import fluxui
#      app = fluxui.DslApp(1200, 800, "CompanyGuard")
#      app.add_css(".app { display:flex } .content { display:flex; flex-direction:column }")
#      devices = fluxui.State(128)
#      app.set_root(
#          fluxui.Div([
#              fluxui.Nav([
#                  fluxui.Button("Dashboard"),
#                  fluxui.Button("Dispositivos"),
#              ]).cls("sidebar"),
#              fluxui.Div([
#                  fluxui.H1("CompanyGuard"),
#                  fluxui.TextFn(lambda: str(devices.get())),
#                  fluxui.Button("Escanear ahora").cls("primary")
#                      .on_click(lambda: devices.set(devices.get() + 1)),
#              ]).cls("content"),
#          ]).cls("app")
#      )
#      app.run_reactive()
# ============================================================

# ---- Reactive binding registry — re-evaluates TextFn nodes each frame. ----
_reactive_bindings = []


def _register_reactive(widget, fn, initial):
    _reactive_bindings.append({"widget": widget, "fn": fn, "last": initial})


# ---- File-based view registry (self-registration, no codegen) ----
_pending_views = {}


def register_view(path, view_fn):
    """Register a view for file-based routing (call at module top level):

        # views/dashboard.py
        import fluxui
        fluxui.register_view("/dashboard", view)

    Then `app.use_views()` wires them all in. Or use `app.auto_routes(dir)` to
    scan a folder automatically.
    """
    _pending_views[path] = view_fn


def pump_reactive_bindings():
    """Re-evaluate every reactive TextFn binding, pushing changed values into the
    underlying widgets. Returns True if anything changed. Called each frame."""
    changed = False
    for b in _reactive_bindings:
        w = b["widget"]
        if w is None:
            continue
        v = b["fn"]()
        if v != b["last"]:
            b["last"] = v
            w.set_content(v)
            changed = True
    return changed


class State:
    """Lightweight reactive primitive (matches C++ State<T>)."""

    def __init__(self, initial=None):
        self._value = initial
        self._listeners = []

    def get(self):
        return self._value

    def set(self, value):
        self._value = value
        for fn in self._listeners:
            fn()

    def on_change(self, fn):
        self._listeners.append(fn)


class Element:
    """Deferred, HTML-named declarative node. Materialized on mount.

    Every node carries an HTML tag name; mount() routes through
    Widget.add_element(tag, ...) which is the single source of truth for the
    tag -> widget mapping (Blink UA parity).
    """

    def __init__(self, tag, content="", children=None, text_fn=None):
        self._tag = tag
        self._content = content
        self._text_fn = text_fn
        self._class_name = ""
        self._node_id = ""
        self._on_click = None
        self._on_mount = None
        self._inline_styles = []
        self._attrs = []
        self._children = children or []

    # Chaining setters — mirror HTML attributes / DOM properties.
    def cls(self, class_name):
        self._class_name = class_name
        return self

    # Alias matching C++ .className()
    def class_name(self, class_name):
        self._class_name = class_name
        return self

    def id(self, node_id):
        self._node_id = node_id
        return self

    def on_click(self, fn):
        self._on_click = fn
        return self

    def on_mount(self, fn):
        """Receive the materialized native Widget for advanced setup."""
        self._on_mount = fn
        return self

    def style(self, prop, val):
        self._inline_styles.append((prop, val))
        return self

    def attr(self, name, val):
        self._attrs.append((name, val))
        return self

    def href(self, url):
        return self.attr("href", url)

    def src(self, url):
        self._content = url
        return self

    def mount(self, parent):
        if self._text_fn is not None:
            initial = self._text_fn()
            w = parent.add_element("span", initial, self._class_name)
            if w:
                _register_reactive(w, self._text_fn, initial)
        else:
            w = parent.add_element(self._tag, self._content, self._class_name)

        if not w:
            return None
        if self._node_id:
            w.set_id(self._node_id)
        if self._on_click:
            w.set_on_click(self._on_click)
        for prop, val in self._inline_styles:
            w.css("{}:{}".format(prop, val))
        if self._on_mount:
            self._on_mount(w)
        for child in self._children:
            child.mount(w)
        return w


# ---- HTML element builders (names match HTML/Blink exactly). ----
def _container(tag, children):
    return Element(tag, children=children or [])


def _leaf(tag, content):
    return Element(tag, content=content)


# Flow containers.
def Div(children=None):
    return _container("div", children)


def Section(children=None):
    return _container("section", children)


def Article(children=None):
    return _container("article", children)


def Aside(children=None):
    return _container("aside", children)


def Header(children=None):
    return _container("header", children)


def Footer(children=None):
    return _container("footer", children)


def Main(children=None):
    return _container("main", children)


def Nav(children=None):
    return _container("nav", children)


def Form(children=None):
    return _container("form", children)


def Fieldset(children=None):
    return _container("fieldset", children)


def Blockquote(children=None):
    return _container("blockquote", children)


def Figure(children=None):
    return _container("figure", children)


def Ul(children=None):
    return _container("ul", children)


def Ol(children=None):
    return _container("ol", children)


def Li(children=None):
    return _container("li", children)


def Table(children=None):
    return _container("table", children)


def Tr(children=None):
    return _container("tr", children)


# Text content.
def Text(content):
    return _leaf("span", content)


def Span(content):
    return _leaf("span", content)


def P(content):
    return _leaf("p", content)


def H1(content):
    return _leaf("h1", content)


def H2(content):
    return _leaf("h2", content)


def H3(content):
    return _leaf("h3", content)


def H4(content):
    return _leaf("h4", content)


def H5(content):
    return _leaf("h5", content)


def H6(content):
    return _leaf("h6", content)


def Strong(content):
    return _leaf("strong", content)


def Em(content):
    return _leaf("em", content)


def Small(content):
    return _leaf("small", content)


def Label(content):
    return _leaf("label", content)


def Legend(content):
    return _leaf("legend", content)


def Code(content):
    return _leaf("code", content)


def Pre(content):
    return _leaf("pre", content)


def Td(content):
    return _leaf("td", content)


def Th(content):
    return _leaf("th", content)


def TextFn(fn):
    """Reactive text — re-evaluated whenever bound State changes."""
    return Element("span", text_fn=fn)


# Interactive controls.
def Button(label):
    return _leaf("button", label)


def Input(type_or_placeholder="", placeholder=None):
    """Input("text") for an untyped placeholder, or Input("email", "name@x.com")."""
    if placeholder is None:
        return _leaf("input", type_or_placeholder)
    e = _leaf("input", placeholder)
    e.on_mount(lambda w: w.set_type(type_or_placeholder))
    return e


def PasswordInput(placeholder=""):
    e = _leaf("input", placeholder)
    e.on_mount(lambda w: w.set_type("password"))
    return e


def TextArea(placeholder=""):
    return _leaf("textarea", placeholder)


def A(content, href=""):
    e = _leaf("a", content)
    if href:
        e.attr("href", href)
    return e


def Img(src):
    return _leaf("img", src)


def Checkbox(checked=False):
    e = Element("checkbox")
    if checked:
        e.on_mount(lambda w: w.set_checked(True))
    return e


def Radio(checked=False, group=""):
    e = Element("radio")
    if checked:
        e.on_mount(lambda w: w.set_checked(True))
    return e


def Range(value=0.5, min=0.0, max=100.0, step=1.0):
    e = Element("range")
    e.on_mount(lambda w: w.set_range_value(value))
    return e


def Meter(value, min=0.0, max=100.0):
    e = Element("meter")
    e.on_mount(lambda w: w.set_meter_value(value))
    return e


def Progress(value=-1.0, max=100.0):
    e = Element("progress")
    e.on_mount(lambda w: w.set_progress_value(value))
    return e


def Details(children=None):
    return _container("details", children)


def Summary(content):
    return _leaf("summary", content)


def Dialog(children=None):
    return _container("dialog", children)


def Hr():
    return Element("hr")


def Br():
    return Element("br")


def Select(options=None):
    return _container("select", options)


def Option(label):
    return _leaf("option", label)


def El(tag, children=None):
    """Generic escape hatch for any HTML tag."""
    return _container(tag, children)


class DslApp(App):
    """App subclass adding declarative set_root + reactive run loop."""

    def __init__(self, width=1200, height=800, title="FluxUI App"):
        super().__init__()
        self.init(title, width, height)
        self.load_default_font(16.0)
        self._routes = {}
        self._layout = None
        self._current_route = ""
        self._content_slot = None

    def add_css(self, css):
        self.add_stylesheet(css)

    def load_css(self, path):
        return self.load_stylesheet(path)

    def set_root(self, root_node):
        r = self.root()
        if r is None or root_node is None:
            return
        r.clear_children()
        root_node.mount(r)

    # ---- Declarative routing (like Next.js) ----

    def add_route(self, path, view_fn):
        """Register a route. view_fn() returns an Element tree."""
        self._routes[path] = view_fn
        return self

    def use_views(self):
        """Register all views collected via fluxui.register_view() (file-based)."""
        for path, view_fn in _pending_views.items():
            self._routes[path] = view_fn
        return self

    def set_layout(self, layout_fn):
        """Set the app shell. layout_fn(content_element) returns the shell tree.
        The shell should embed the given content element somewhere."""
        self._layout = layout_fn
        return self

    def route(self):
        return self._current_route

    def navigate(self, path):
        """Switch to a route; rebuilds the shell so nav highlights refresh."""
        self._current_route = path
        self.build(path)

    def build(self, initial_route=None):
        """Build the shell + initial route. Uses the first route if none given."""
        r = self.root()
        if r is None:
            return
        r.clear_children()
        if initial_route:
            self._current_route = initial_route
        elif not self._current_route and self._routes:
            self._current_route = next(iter(self._routes))

        content = None
        view_fn = self._routes.get(self._current_route)
        if view_fn:
            content = view_fn()

        if self._layout:
            shell = self._layout(content)
            if shell:
                shell.mount(r)
        elif content:
            content.mount(r)
        self.requestRedraw() if hasattr(self, "requestRedraw") else None

    def auto_routes(self, views_dir):
        """File-based routing like Next.js: import every .py module in views_dir
        and register a route for each exported view function.

        Convention: views/dashboard.py defining `dashboard_view()` (or `view()`)
        registers route "/dashboard". Underscores in the filename become hyphens.
        """
        import importlib.util
        import glob

        for py_file in sorted(glob.glob(os.path.join(views_dir, "*.py"))):
            mod_name = os.path.splitext(os.path.basename(py_file))[0]
            if mod_name.startswith("_"):
                continue
            spec = importlib.util.spec_from_file_location("fluxui_view_" + mod_name, py_file)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)

            # Find the view function: prefer "view", else "<name>_view", else any callable.
            view_fn = None
            if hasattr(module, "view") and callable(module.view):
                view_fn = module.view
            elif hasattr(module, mod_name + "_view") and callable(getattr(module, mod_name + "_view")):
                view_fn = getattr(module, mod_name + "_view")
            else:
                # PascalCase: dashboard -> DashboardView
                pascal = "".join(p.capitalize() for p in mod_name.split("_")) + "View"
                if hasattr(module, pascal) and callable(getattr(module, pascal)):
                    view_fn = getattr(module, pascal)
            if view_fn is None:
                continue

            route = "/" + mod_name.replace("_", "-")
            self.add_route(route, view_fn)
        return self

    def run_reactive(self):
        # Auto-build if routing is configured but nothing has been mounted yet.
        if self._routes:
            self.build(self._current_route or None)
        self.set_update_callback(lambda dt: pump_reactive_bindings())
        self.run()
