// FluxUI C ABI - stable FFI surface for C, C++, Rust, Zig, and other languages.
#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(FLUXUI_SHARED)
  #if defined(FLUXUI_BUILDING_DLL)
    #define FLUXUI_API __declspec(dllexport)
  #else
    #define FLUXUI_API __declspec(dllimport)
  #endif
#else
  #define FLUXUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxUIApp FluxUIApp;
typedef struct FluxUIWidget FluxUIWidget;

typedef struct FluxUIColor {
    float r;
    float g;
    float b;
    float a;
} FluxUIColor;

typedef struct FluxUIRect {
    float x;
    float y;
    float w;
    float h;
} FluxUIRect;

typedef enum FluxUIRenderBackend {
    FLUXUI_BACKEND_AUTO = 0,
    FLUXUI_BACKEND_VULKAN = 1,
    FLUXUI_BACKEND_DIRECT3D12 = 2,
    FLUXUI_BACKEND_DIRECT12 = FLUXUI_BACKEND_DIRECT3D12,
    FLUXUI_BACKEND_DIRECTX12 = FLUXUI_BACKEND_DIRECT3D12,
    FLUXUI_BACKEND_METAL = 3,
    FLUXUI_BACKEND_SKIA = 4,
    FLUXUI_BACKEND_COMPATIBILITY = 100
} FluxUIRenderBackend;

typedef void (*FluxUIUpdateCallback)(FluxUIApp* app, float delta_time, void* user_data);
typedef void (*FluxUIClickCallback)(FluxUIWidget* widget, void* user_data);
typedef void (*FluxUIRouteCallback)(FluxUIApp* app,
                                    FluxUIWidget* container,
                                    const char* route,
                                    void* user_data);
typedef void (*FluxUIDrawCallback)(FluxUIWidget* widget, void* renderer_ptr, FluxUIRect bounds, void* user_data);
typedef void (*FluxUIVirtualListItemCallback)(FluxUIWidget* item,
                                              uint32_t index,
                                              void* user_data);

typedef enum FluxUIEventType {
    FLUXUI_EVENT_ANY = 0,
    FLUXUI_EVENT_QUIT = 1,
    FLUXUI_EVENT_WINDOW_RESIZED = 2,
    FLUXUI_EVENT_MOUSE_MOVE = 3,
    FLUXUI_EVENT_MOUSE_DOWN = 4,
    FLUXUI_EVENT_MOUSE_UP = 5,
    FLUXUI_EVENT_MOUSE_WHEEL = 6,
    FLUXUI_EVENT_KEY_DOWN = 7,
    FLUXUI_EVENT_KEY_UP = 8,
    FLUXUI_EVENT_TEXT_INPUT = 9,
    FLUXUI_EVENT_WIDGET_CLICK = 10,
    FLUXUI_EVENT_ROUTE_CHANGED = 11,
    FLUXUI_EVENT_CUSTOM = 12
} FluxUIEventType;

typedef struct FluxUIEvent {
    FluxUIEventType type;
    FluxUIWidget* target;
    const char* name;
    const char* route;
    const char* previous_route;
    const char* text;
    float x;
    float y;
    float dx;
    float dy;
    int key_code;
    int modifiers;
    int button;
    int click_count;
    int handled;
} FluxUIEvent;

typedef void (*FluxUIEventCallback)(FluxUIApp* app,
                                    const FluxUIEvent* event,
                                    void* user_data);
typedef void (*FluxUIActionCallback)(FluxUIApp* app,
                                     const char* action_name,
                                     void* user_data);

// Application lifecycle
FLUXUI_API FluxUIApp* fluxui_app_create(void);
FLUXUI_API void fluxui_app_destroy(FluxUIApp* app);
FLUXUI_API int fluxui_app_set_backend(FluxUIApp* app, FluxUIRenderBackend backend);
FLUXUI_API FluxUIRenderBackend fluxui_app_get_backend(FluxUIApp* app);
FLUXUI_API FluxUIRenderBackend fluxui_default_backend(void);
FLUXUI_API const char* fluxui_backend_name(FluxUIRenderBackend backend);
FLUXUI_API int fluxui_backend_is_compiled(FluxUIRenderBackend backend);
FLUXUI_API int fluxui_backend_is_selectable(FluxUIRenderBackend backend);
FLUXUI_API int fluxui_app_init(FluxUIApp* app, const char* title, int width, int height);
FLUXUI_API void fluxui_app_shutdown(FluxUIApp* app);
FLUXUI_API void fluxui_app_run(FluxUIApp* app);
FLUXUI_API void fluxui_app_stop(FluxUIApp* app);

// Application resources and callbacks
FLUXUI_API int fluxui_app_load_stylesheet(FluxUIApp* app, const char* path);
FLUXUI_API void fluxui_app_add_stylesheet(FluxUIApp* app, const char* css);
FLUXUI_API int fluxui_app_load_font(FluxUIApp* app, const char* path, float size);
FLUXUI_API int fluxui_app_load_font_named(FluxUIApp* app, const char* path, float size, const char* name);
FLUXUI_API int fluxui_app_load_default_font(FluxUIApp* app, float size);
FLUXUI_API void fluxui_app_warm_font_cache(FluxUIApp* app, const float* sizes, uint32_t count, const char* name);
FLUXUI_API void fluxui_app_release_font_sources(FluxUIApp* app);
FLUXUI_API void fluxui_app_set_update_callback(FluxUIApp* app,
                                               FluxUIUpdateCallback callback,
                                               void* user_data);
FLUXUI_API FluxUIWidget* fluxui_app_root(FluxUIApp* app);
FLUXUI_API uint64_t fluxui_app_on_event(FluxUIApp* app,
                                        FluxUIEventType type,
                                        FluxUIEventCallback callback,
                                        void* user_data);
FLUXUI_API void fluxui_app_off_event(FluxUIApp* app, uint64_t listener_id);
FLUXUI_API void fluxui_app_emit_custom_event(FluxUIApp* app,
                                             const char* name,
                                             const char* text);
FLUXUI_API uint64_t fluxui_app_add_action(FluxUIApp* app,
                                          const char* name,
                                          int key_code,
                                          int modifiers,
                                          FluxUIActionCallback callback,
                                          void* user_data);
FLUXUI_API void fluxui_app_remove_action(FluxUIApp* app, uint64_t action_id);
FLUXUI_API int fluxui_app_dispatch_action(FluxUIApp* app, const char* name);
FLUXUI_API void fluxui_app_register_action(FluxUIApp* app,
                                           const char* name,
                                           FluxUIActionCallback callback,
                                           void* user_data);
FLUXUI_API int fluxui_app_load_keymap(FluxUIApp* app, const char* path);
FLUXUI_API void fluxui_app_add_keymap(FluxUIApp* app, const char* json_content);
FLUXUI_API void fluxui_app_add_route(FluxUIApp* app,
                                     const char* path,
                                     FluxUIRouteCallback callback,
                                     void* user_data);
FLUXUI_API int fluxui_app_navigate(FluxUIApp* app, const char* path);
FLUXUI_API const char* fluxui_app_current_route(FluxUIApp* app);
FLUXUI_API int fluxui_app_route_dirty(FluxUIApp* app);
FLUXUI_API int fluxui_app_render_route(FluxUIApp* app, FluxUIWidget* container);

// Tree mutation
FLUXUI_API void fluxui_widget_clear_children(FluxUIWidget* widget);
FLUXUI_API void fluxui_widget_reserve_children(FluxUIWidget* widget, uint32_t count);
FLUXUI_API FluxUIWidget* fluxui_widget_add_panel(FluxUIWidget* parent, const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_fieldset(FluxUIWidget* parent, const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_text(FluxUIWidget* parent,
                                                const char* text,
                                                const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_label(FluxUIWidget* parent,
                                                 const char* text,
                                                 const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_legend(FluxUIWidget* parent,
                                                  const char* text,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_button(FluxUIWidget* parent,
                                                  const char* label,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_text_input(FluxUIWidget* parent,
                                                      const char* placeholder,
                                                      const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_input(FluxUIWidget* parent,
                                                 const char* input_type,
                                                 const char* placeholder,
                                                 const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_password_input(FluxUIWidget* parent,
                                                          const char* placeholder,
                                                          const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_textarea(FluxUIWidget* parent,
                                                    const char* placeholder,
                                                    const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_checkbox(FluxUIWidget* parent,
                                                    int checked,
                                                    const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_radio(FluxUIWidget* parent,
                                                 int checked,
                                                 const char* group,
                                                 const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_range(FluxUIWidget* parent,
                                                 float value,
                                                 float min,
                                                 float max,
                                                 float step,
                                                 const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_select(FluxUIWidget* parent,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_option(FluxUIWidget* parent,
                                                  const char* label,
                                                  const char* value,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_anchor(FluxUIWidget* parent,
                                                  const char* text,
                                                  const char* href,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_details(FluxUIWidget* parent,
                                                   const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_summary(FluxUIWidget* parent,
                                                   const char* text,
                                                   const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_dialog(FluxUIWidget* parent,
                                                  const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_meter(FluxUIWidget* parent,
                                                 float value,
                                                 float min,
                                                 float max,
                                                 const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_progress_element(FluxUIWidget* parent,
                                                           float value,
                                                           float max,
                                                           const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_hr(FluxUIWidget* parent,
                                              const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_br(FluxUIWidget* parent,
                                              const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_icon(FluxUIWidget* parent,
                                                const char* glyph,
                                                const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_progress_bar(FluxUIWidget* parent,
                                                        const char* class_name,
                                                        float progress);
FLUXUI_API FluxUIWidget* fluxui_widget_add_stat_card(FluxUIWidget* parent,
                                                     const char* title,
                                                     const char* value,
                                                     const char* subtitle,
                                                     FluxUIColor accent);
FLUXUI_API FluxUIWidget* fluxui_widget_add_canvas(FluxUIWidget* parent, const char* class_name);
FLUXUI_API FluxUIWidget* fluxui_widget_add_virtual_list(FluxUIWidget* parent,
                                                        const char* class_name,
                                                        uint32_t item_count,
                                                        float item_height,
                                                        FluxUIVirtualListItemCallback callback,
                                                        void* user_data);
FLUXUI_API void fluxui_virtual_list_set_item_count(FluxUIWidget* widget, uint32_t item_count);
FLUXUI_API void fluxui_virtual_list_refresh(FluxUIWidget* widget);
FLUXUI_API void fluxui_virtual_list_scroll_to_index(FluxUIWidget* widget,
                                                    uint32_t index,
                                                    int strategy);
FLUXUI_API void fluxui_canvas_set_on_draw(FluxUIWidget* canvas, FluxUIDrawCallback callback, void* user_data);

// Canvas 2D Drawing Primitives (usable inside draw callback)
FLUXUI_API void fluxui_draw_rect(void* renderer_ptr, FluxUIRect rect, FluxUIColor color);
FLUXUI_API void fluxui_draw_text(void* renderer_ptr, const char* text, float x, float y, FluxUIColor color, float font_size);
FLUXUI_API void fluxui_draw_image(void* renderer_ptr, const char* name_or_path, FluxUIRect rect, float opacity);
FLUXUI_API void fluxui_renderer_flush(void* renderer_ptr);

// Widget state
FLUXUI_API void fluxui_widget_set_id(FluxUIWidget* widget, const char* id);
FLUXUI_API void fluxui_widget_set_class(FluxUIWidget* widget, const char* class_name);
FLUXUI_API void fluxui_widget_set_visible(FluxUIWidget* widget, int visible);
FLUXUI_API FluxUIRect fluxui_widget_get_bounds(FluxUIWidget* widget);
FLUXUI_API void fluxui_widget_set_on_click(FluxUIWidget* widget,
                                           FluxUIClickCallback callback,
                                           void* user_data);

// Text-like widgets
FLUXUI_API void fluxui_text_set_content(FluxUIWidget* widget, const char* text);
FLUXUI_API void fluxui_button_set_label(FluxUIWidget* widget, const char* label);
FLUXUI_API void fluxui_text_input_set_value(FluxUIWidget* widget, const char* value);
FLUXUI_API const char* fluxui_text_input_get_value(FluxUIWidget* widget);
FLUXUI_API void fluxui_text_input_set_placeholder(FluxUIWidget* widget, const char* placeholder);
FLUXUI_API void fluxui_text_input_set_type(FluxUIWidget* widget, const char* input_type);
FLUXUI_API void fluxui_checkbox_set_checked(FluxUIWidget* widget, int checked);
FLUXUI_API int fluxui_checkbox_get_checked(FluxUIWidget* widget);
FLUXUI_API void fluxui_radio_set_checked(FluxUIWidget* widget, int checked);
FLUXUI_API int fluxui_radio_get_checked(FluxUIWidget* widget);
FLUXUI_API void fluxui_range_set_value(FluxUIWidget* widget, float value);
FLUXUI_API float fluxui_range_get_value(FluxUIWidget* widget);
FLUXUI_API void fluxui_select_set_selected_index(FluxUIWidget* widget, uint32_t index);
FLUXUI_API uint32_t fluxui_select_get_selected_index(FluxUIWidget* widget);
FLUXUI_API void fluxui_details_set_open(FluxUIWidget* widget, int open);
FLUXUI_API int fluxui_details_get_open(FluxUIWidget* widget);
FLUXUI_API void fluxui_dialog_show(FluxUIWidget* widget);
FLUXUI_API void fluxui_dialog_show_modal(FluxUIWidget* widget);
FLUXUI_API void fluxui_dialog_close(FluxUIWidget* widget);
FLUXUI_API int fluxui_dialog_get_open(FluxUIWidget* widget);
FLUXUI_API void fluxui_meter_set_value(FluxUIWidget* widget, float value);
FLUXUI_API float fluxui_meter_get_value(FluxUIWidget* widget);
FLUXUI_API void fluxui_progress_element_set_value(FluxUIWidget* widget, float value);
FLUXUI_API float fluxui_progress_element_get_value(FluxUIWidget* widget);
FLUXUI_API void fluxui_icon_set_glyph(FluxUIWidget* widget, const char* glyph);
FLUXUI_API void fluxui_progress_bar_set_value(FluxUIWidget* widget, float progress);
FLUXUI_API void fluxui_progress_bar_set_color(FluxUIWidget* widget, FluxUIColor color);

// Inline style helpers. Loading CSS and assigning classes is preferred for larger UIs.
FLUXUI_API FluxUIColor fluxui_color_rgba(float r, float g, float b, float a);
FLUXUI_API FluxUIColor fluxui_color_rgb_u8(uint8_t r, uint8_t g, uint8_t b);
FLUXUI_API void fluxui_style_width_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_height_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_min_width_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_min_height_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_max_width_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_max_height_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_flex_grow(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_gap_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_padding_all_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_padding_px(FluxUIWidget* widget,
                                        float top,
                                        float right,
                                        float bottom,
                                        float left);
FLUXUI_API void fluxui_style_margin_all_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_margin_px(FluxUIWidget* widget,
                                       float top,
                                       float right,
                                       float bottom,
                                       float left);
FLUXUI_API void fluxui_style_border_radius_px(FluxUIWidget* widget, float value);
FLUXUI_API void fluxui_style_background_color(FluxUIWidget* widget, FluxUIColor color);
FLUXUI_API void fluxui_style_text_color(FluxUIWidget* widget, FluxUIColor color);

#ifdef __cplusplus
}
#endif
