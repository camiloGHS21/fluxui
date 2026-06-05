// FluxUI C ABI implementation
#include "fluxui/fluxui_c.h"
#include "fluxui/FluxUI.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

using namespace FluxUI;

struct FluxUIApp {
    Application app;
    FluxUIUpdateCallback updateCallback = nullptr;
    void* updateUserData = nullptr;
    bool initialized = false;
    bool shutdown = false;
};

static Widget* as_widget(FluxUIWidget* widget) {
    return reinterpret_cast<Widget*>(widget);
}

static FluxUIWidget* as_c_widget(Widget* widget) {
    return reinterpret_cast<FluxUIWidget*>(widget);
}

static const char* safe_cstr(const char* value) {
    return value ? value : "";
}

static Color to_color(FluxUIColor color) {
    return Color(color.r, color.g, color.b, color.a);
}

static FluxUIColor from_color(const Color& color) {
    return {color.r, color.g, color.b, color.a};
}

static RenderBackendType to_backend(FluxUIRenderBackend backend) {
    switch (backend) {
    case FLUXUI_BACKEND_VULKAN:
        return RenderBackendType::Vulkan;
    case FLUXUI_BACKEND_DIRECT3D12:
        return RenderBackendType::Direct3D12;
    case FLUXUI_BACKEND_SKIA:
        return RenderBackendType::Skia;
    case FLUXUI_BACKEND_METAL:
        return RenderBackendType::Metal;
    case FLUXUI_BACKEND_COMPATIBILITY:
        return RenderBackendType::Compatibility;
    case FLUXUI_BACKEND_AUTO:
    default:
        return RenderBackendType::Auto;
    }
}

static FluxUIRenderBackend from_backend(RenderBackendType backend) {
    switch (backend) {
    case RenderBackendType::Vulkan:
        return FLUXUI_BACKEND_VULKAN;
    case RenderBackendType::Direct3D12:
        return FLUXUI_BACKEND_DIRECT3D12;
    case RenderBackendType::Skia:
        return FLUXUI_BACKEND_SKIA;
    case RenderBackendType::Metal:
        return FLUXUI_BACKEND_METAL;
    case RenderBackendType::Compatibility:
        return FLUXUI_BACKEND_COMPATIBILITY;
    case RenderBackendType::Auto:
    default:
        return FLUXUI_BACKEND_AUTO;
    }
}

static UIEventType to_event_type(FluxUIEventType type) {
    switch (type) {
    case FLUXUI_EVENT_QUIT: return UIEventType::Quit;
    case FLUXUI_EVENT_WINDOW_RESIZED: return UIEventType::WindowResized;
    case FLUXUI_EVENT_MOUSE_MOVE: return UIEventType::MouseMove;
    case FLUXUI_EVENT_MOUSE_DOWN: return UIEventType::MouseDown;
    case FLUXUI_EVENT_MOUSE_UP: return UIEventType::MouseUp;
    case FLUXUI_EVENT_MOUSE_WHEEL: return UIEventType::MouseWheel;
    case FLUXUI_EVENT_KEY_DOWN: return UIEventType::KeyDown;
    case FLUXUI_EVENT_KEY_UP: return UIEventType::KeyUp;
    case FLUXUI_EVENT_TEXT_INPUT: return UIEventType::TextInput;
    case FLUXUI_EVENT_WIDGET_CLICK: return UIEventType::WidgetClick;
    case FLUXUI_EVENT_ROUTE_CHANGED: return UIEventType::RouteChanged;
    case FLUXUI_EVENT_CUSTOM: return UIEventType::Custom;
    case FLUXUI_EVENT_ANY:
    default: return UIEventType::Any;
    }
}

static FluxUIEventType from_event_type(UIEventType type) {
    switch (type) {
    case UIEventType::Quit: return FLUXUI_EVENT_QUIT;
    case UIEventType::WindowResized: return FLUXUI_EVENT_WINDOW_RESIZED;
    case UIEventType::MouseMove: return FLUXUI_EVENT_MOUSE_MOVE;
    case UIEventType::MouseDown: return FLUXUI_EVENT_MOUSE_DOWN;
    case UIEventType::MouseUp: return FLUXUI_EVENT_MOUSE_UP;
    case UIEventType::MouseWheel: return FLUXUI_EVENT_MOUSE_WHEEL;
    case UIEventType::KeyDown: return FLUXUI_EVENT_KEY_DOWN;
    case UIEventType::KeyUp: return FLUXUI_EVENT_KEY_UP;
    case UIEventType::TextInput: return FLUXUI_EVENT_TEXT_INPUT;
    case UIEventType::WidgetClick: return FLUXUI_EVENT_WIDGET_CLICK;
    case UIEventType::RouteChanged: return FLUXUI_EVENT_ROUTE_CHANGED;
    case UIEventType::Custom: return FLUXUI_EVENT_CUSTOM;
    case UIEventType::Any:
    default: return FLUXUI_EVENT_ANY;
    }
}

template <typename T>
static T* as(FluxUIWidget* widget) {
    return dynamic_cast<T*>(as_widget(widget));
}

extern "C" {

FluxUIApp* fluxui_app_create(void) {
    return new FluxUIApp();
}

void fluxui_app_destroy(FluxUIApp* app) {
    if (!app) return;
    fluxui_app_shutdown(app);
    delete app;
}

int fluxui_app_set_backend(FluxUIApp* app, FluxUIRenderBackend backend) {
    if (!app || app->initialized) return 0;
    app->app.setBackend(to_backend(backend));
    return 1;
}

FluxUIRenderBackend fluxui_app_get_backend(FluxUIApp* app) {
    if (!app) return FLUXUI_BACKEND_AUTO;
    return from_backend(app->app.activeBackend());
}

FluxUIRenderBackend fluxui_default_backend(void) {
    return from_backend(Renderer::defaultBackend());
}

const char* fluxui_backend_name(FluxUIRenderBackend backend) {
    return Renderer::backendName(to_backend(backend));
}

int fluxui_backend_is_compiled(FluxUIRenderBackend backend) {
    return Renderer::getBackendInfo(to_backend(backend)).compiled ? 1 : 0;
}

int fluxui_backend_is_selectable(FluxUIRenderBackend backend) {
    return Renderer::getBackendInfo(to_backend(backend)).selectable ? 1 : 0;
}

int fluxui_app_init(FluxUIApp* app, const char* title, int width, int height) {
    if (!app) return 0;
    if (app->initialized && !app->shutdown) return 1;

    app->initialized = app->app.init(safe_cstr(title), width, height);
    app->shutdown = !app->initialized;
    return app->initialized ? 1 : 0;
}

void fluxui_app_shutdown(FluxUIApp* app) {
    if (!app || !app->initialized || app->shutdown) return;
    app->app.shutdown();
    app->shutdown = true;
    app->initialized = false;
}

void fluxui_app_run(FluxUIApp* app) {
    if (!app || !app->initialized || app->shutdown) return;
    app->app.run();
}

void fluxui_app_stop(FluxUIApp* app) {
    if (!app) return;
    app->app.running = false;
}

int fluxui_app_load_stylesheet(FluxUIApp* app, const char* path) {
    if (!app || !path) return 0;
    return app->app.loadStylesheet(path) ? 1 : 0;
}

void fluxui_app_add_stylesheet(FluxUIApp* app, const char* css) {
    if (!app || !css) return;
    app->app.addStylesheet(css);
}

void fluxui_app_enable_hot_reload(FluxUIApp* app, int enable, float poll_interval_seconds) {
    if (!app) return;
    app->app.enableHotReload(enable != 0, poll_interval_seconds);
}

void fluxui_app_watch_stylesheet(FluxUIApp* app, const char* path) {
    if (!app || !path) return;
    app->app.watchStylesheet(path);
}

int fluxui_app_reload_styles(FluxUIApp* app) {
    if (!app) return 0;
    return app->app.reloadStyles() ? 1 : 0;
}

void fluxui_app_set_power_profile(FluxUIApp* app, int profile) {
    if (!app) return;
    // 0=Auto, 1=HighPerformance, 2=Balanced, 3=PowerSaver
    FluxUI::Application::PowerProfile p = FluxUI::Application::PowerProfile::Auto;
    switch (profile) {
        case 1: p = FluxUI::Application::PowerProfile::HighPerformance; break;
        case 2: p = FluxUI::Application::PowerProfile::Balanced; break;
        case 3: p = FluxUI::Application::PowerProfile::PowerSaver; break;
        default: p = FluxUI::Application::PowerProfile::Auto; break;
    }
    app->app.setPowerProfile(p);
}

void fluxui_app_set_frame_rate_limits(FluxUIApp* app, int active_fps, int battery_fps, int background_fps) {
    if (!app) return;
    app->app.setFrameRateLimits(active_fps, battery_fps, background_fps);
}

int fluxui_app_load_font(FluxUIApp* app, const char* path, float size) {
    return fluxui_app_load_font_named(app, path, size, "default");
}

int fluxui_app_load_font_named(FluxUIApp* app, const char* path, float size, const char* name) {
    if (!app || !path) return 0;
    return app->app.renderer().loadFont(path, size, safe_cstr(name)) ? 1 : 0;
}

int fluxui_app_load_default_font(FluxUIApp* app, float size) {
    if (!app) return 0;
    return app->app.renderer().loadDefaultFont(size) ? 1 : 0;
}

void fluxui_app_warm_font_cache(FluxUIApp* app, const float* sizes, uint32_t count, const char* name) {
    if (!app || !sizes || count == 0) return;
    std::vector<float> requestedSizes(sizes, sizes + count);
    app->app.renderer().warmFontCache(requestedSizes, (name && name[0]) ? name : "default");
}

void fluxui_app_release_font_sources(FluxUIApp* app) {
    if (!app) return;
    app->app.renderer().releaseFontSources();
}

void fluxui_app_set_update_callback(FluxUIApp* app,
                                    FluxUIUpdateCallback callback,
                                    void* user_data) {
    if (!app) return;
    app->updateCallback = callback;
    app->updateUserData = user_data;
    app->app.onUpdate = [app](float deltaTime) {
        if (app->updateCallback) {
            app->updateCallback(app, deltaTime, app->updateUserData);
        }
    };
}

void fluxui_app_set_update_callback_go(FluxUIApp* app,
                                       void (*callback)(FluxUIApp* app, uint32_t delta_time_bits, void* user_data),
                                       void* user_data) {
    if (!app) return;
    app->app.onUpdate = [app, callback, user_data](float deltaTime) {
        if (callback) {
            union { float f; uint32_t i; } u;
            u.f = deltaTime;
            callback(app, u.i, user_data);
        }
    };
}

FluxUIWidget* fluxui_app_root(FluxUIApp* app) {
    if (!app || !app->initialized || app->shutdown) return nullptr;
    return as_c_widget(app->app.root());
}

uint64_t fluxui_app_on_event(FluxUIApp* app,
                             FluxUIEventType type,
                             FluxUIEventCallback callback,
                             void* user_data) {
    if (!app || !callback) return 0;
    size_t id = app->app.on(to_event_type(type), [app, callback, user_data](UIEvent& event) {
        FluxUIEvent cEvent = {};
        cEvent.type = from_event_type(event.type);
        cEvent.target = as_c_widget(event.target);
        cEvent.name = event.name.c_str();
        cEvent.route = event.route.c_str();
        cEvent.previous_route = event.previousRoute.c_str();
        cEvent.text = event.text.c_str();
        cEvent.x = event.position.x;
        cEvent.y = event.position.y;
        cEvent.dx = event.delta.x;
        cEvent.dy = event.delta.y;
        cEvent.key_code = event.keyCode;
        cEvent.modifiers = event.modifiers;
        cEvent.button = event.button;
        cEvent.click_count = event.clickCount;
        cEvent.handled = event.handled ? 1 : 0;
        callback(app, &cEvent, user_data);
        event.handled = cEvent.handled != 0;
    });
    return static_cast<uint64_t>(id);
}

void fluxui_app_off_event(FluxUIApp* app, uint64_t listener_id) {
    if (!app) return;
    app->app.off(static_cast<size_t>(listener_id));
}

void fluxui_app_emit_custom_event(FluxUIApp* app,
                                  const char* name,
                                  const char* text) {
    if (!app) return;
    UIEvent event;
    event.type = UIEventType::Custom;
    event.name = safe_cstr(name);
    event.text = safe_cstr(text);
    app->app.emit(std::move(event));
}

uint64_t fluxui_app_add_action(FluxUIApp* app,
                               const char* name,
                               int key_code,
                               int modifiers,
                               FluxUIActionCallback callback,
                               void* user_data) {
    if (!app || !name || !callback) return 0;
    std::string actionName = safe_cstr(name);
    size_t id = app->app.addAction(
        actionName,
        key_code,
        modifiers,
        [app, callback, user_data](Application&, const std::string& dispatchedName) {
            callback(app, dispatchedName.c_str(), user_data);
        });
    return static_cast<uint64_t>(id);
}

void fluxui_app_remove_action(FluxUIApp* app, uint64_t action_id) {
    if (!app) return;
    app->app.removeAction(static_cast<size_t>(action_id));
}

int fluxui_app_dispatch_action(FluxUIApp* app, const char* name) {
    if (!app || !name) return 0;
    return app->app.dispatchAction(safe_cstr(name)) ? 1 : 0;
}

void fluxui_app_register_action(FluxUIApp* app,
                               const char* name,
                               FluxUIActionCallback callback,
                               void* user_data) {
    if (!app || !name || !callback) return;
    app->app.registerAction(safe_cstr(name), [app, callback, user_data](Application&, const std::string& dispatchedName) {
        callback(app, dispatchedName.c_str(), user_data);
    });
}

int fluxui_app_load_keymap(FluxUIApp* app, const char* path) {
    if (!app || !path) return 0;
    return app->app.loadKeymap(safe_cstr(path)) ? 1 : 0;
}

void fluxui_app_add_keymap(FluxUIApp* app, const char* json_content) {
    if (!app || !json_content) return;
    app->app.addKeymap(safe_cstr(json_content));
}

void fluxui_app_add_route(FluxUIApp* app,
                          const char* path,
                          FluxUIRouteCallback callback,
                          void* user_data) {
    if (!app || !path || !callback) return;
    std::string routePath = safe_cstr(path);
    app->app.addRoute(routePath, [app, callback, user_data, routePath](Application&, Widget* container) {
        callback(app, as_c_widget(container), routePath.c_str(), user_data);
    });
}

int fluxui_app_navigate(FluxUIApp* app, const char* path) {
    if (!app || !path) return 0;
    return app->app.navigate(path) ? 1 : 0;
}

const char* fluxui_app_current_route(FluxUIApp* app) {
    if (!app) return "";
    return app->app.currentRoute().c_str();
}

int fluxui_app_route_dirty(FluxUIApp* app) {
    if (!app) return 0;
    return app->app.routeDirty() ? 1 : 0;
}

int fluxui_app_render_route(FluxUIApp* app, FluxUIWidget* container) {
    if (!app || !container) return 0;
    return app->app.renderRoute(as_widget(container)) ? 1 : 0;
}

void fluxui_widget_clear_children(FluxUIWidget* widget) {
    Widget* w = as_widget(widget);
    if (!w) return;
    w->clearChildren();
}

void fluxui_widget_reserve_children(FluxUIWidget* widget, uint32_t count) {
    Widget* w = as_widget(widget);
    if (!w) return;
    w->reserveChildren(static_cast<size_t>(count));
}

FluxUIWidget* fluxui_widget_add_element(FluxUIWidget* parent,
                                        const char* tag_name,
                                        const char* text,
                                        const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->element(safe_cstr(tag_name),
                                  safe_cstr(text),
                                  safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_panel(FluxUIWidget* parent, const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<Panel>(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_form(FluxUIWidget* parent, const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->form(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_fieldset(FluxUIWidget* parent, const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->fieldset(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_text(FluxUIWidget* parent,
                                     const char* text,
                                     const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<Text>(safe_cstr(text), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_label(FluxUIWidget* parent,
                                      const char* text,
                                      const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->label(safe_cstr(text), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_legend(FluxUIWidget* parent,
                                       const char* text,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->legend(safe_cstr(text), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_button(FluxUIWidget* parent,
                                       const char* label,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<Button>(safe_cstr(label), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_text_input(FluxUIWidget* parent,
                                           const char* placeholder,
                                           const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<TextInput>(safe_cstr(placeholder), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_input(FluxUIWidget* parent,
                                      const char* input_type,
                                      const char* placeholder,
                                      const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    auto* input = p->add<TextInput>(safe_cstr(placeholder), safe_cstr(class_name));
    input->setInputType(safe_cstr(input_type));
    return as_c_widget(input);
}

FluxUIWidget* fluxui_widget_add_password_input(FluxUIWidget* parent,
                                               const char* placeholder,
                                               const char* class_name) {
    return fluxui_widget_add_input(parent, "password", placeholder, class_name);
}

FluxUIWidget* fluxui_widget_add_textarea(FluxUIWidget* parent,
                                         const char* placeholder,
                                         const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->textarea(safe_cstr(placeholder), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_checkbox(FluxUIWidget* parent,
                                         int checked,
                                         const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->checkbox(checked != 0, safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_radio(FluxUIWidget* parent,
                                      int checked,
                                      const char* group,
                                      const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->radio(checked != 0, safe_cstr(group), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_range(FluxUIWidget* parent,
                                      float value,
                                      float min,
                                      float max,
                                      float step,
                                      const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->range(value, min, max, step, safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_select(FluxUIWidget* parent,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->select(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_option(FluxUIWidget* parent,
                                       const char* label,
                                       const char* value,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->option(safe_cstr(label), safe_cstr(value), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_anchor(FluxUIWidget* parent,
                                       const char* text,
                                       const char* href,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->anchor(safe_cstr(text), safe_cstr(href), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_details(FluxUIWidget* parent,
                                        const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->details(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_summary(FluxUIWidget* parent,
                                        const char* text,
                                        const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->summary(safe_cstr(text), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_dialog(FluxUIWidget* parent,
                                       const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->dialog(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_meter(FluxUIWidget* parent,
                                      float value,
                                      float min,
                                      float max,
                                      const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->meter(value, min, max, safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_progress_element(FluxUIWidget* parent,
                                                 float value,
                                                 float max,
                                                 const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->progressElement(value, max, safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_hr(FluxUIWidget* parent,
                                   const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->hr(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_br(FluxUIWidget* parent,
                                   const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->br(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_icon(FluxUIWidget* parent,
                                     const char* glyph,
                                     const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<Icon>(safe_cstr(glyph), safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_progress_bar(FluxUIWidget* parent,
                                             const char* class_name,
                                             float progress) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;

    auto* bar = p->add<ProgressBar>();
    bar->className = safe_cstr(class_name);
    bar->progress = std::clamp(progress, 0.0f, 1.0f);
    return as_c_widget(bar);
}

FluxUIWidget* fluxui_widget_add_stat_card(FluxUIWidget* parent,
                                          const char* title,
                                          const char* value,
                                          const char* subtitle,
                                          FluxUIColor accent) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<StatCard>(
        safe_cstr(title),
        safe_cstr(value),
        safe_cstr(subtitle),
        to_color(accent)));
}

FluxUIWidget* fluxui_widget_add_canvas(FluxUIWidget* parent, const char* class_name) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    return as_c_widget(p->add<Canvas>(safe_cstr(class_name)));
}

FluxUIWidget* fluxui_widget_add_virtual_list(FluxUIWidget* parent,
                                             const char* class_name,
                                             uint32_t item_count,
                                             float item_height,
                                             FluxUIVirtualListItemCallback callback,
                                             void* user_data) {
    Widget* p = as_widget(parent);
    if (!p) return nullptr;
    auto builder = [callback, user_data](Widget* item, size_t index) {
        if (callback) {
            callback(as_c_widget(item), static_cast<uint32_t>(index), user_data);
        }
    };
    return as_c_widget(p->add<VirtualList>(
        static_cast<size_t>(item_count),
        item_height,
        std::move(builder),
        safe_cstr(class_name)));
}

void fluxui_virtual_list_set_item_count(FluxUIWidget* widget, uint32_t item_count) {
    if (auto* w = as<VirtualList>(widget)) {
        w->setItemCount(static_cast<size_t>(item_count));
    }
}

void fluxui_virtual_list_refresh(FluxUIWidget* widget) {
    if (auto* w = as<VirtualList>(widget)) {
        w->refresh();
    }
}

void fluxui_virtual_list_scroll_to_index(FluxUIWidget* widget, uint32_t index, int strategy) {
    if (auto* w = as<VirtualList>(widget)) {
        VirtualListScrollStrategy scrollStrategy = VirtualListScrollStrategy::Nearest;
        if (strategy >= static_cast<int>(VirtualListScrollStrategy::Start) &&
            strategy <= static_cast<int>(VirtualListScrollStrategy::Nearest)) {
            scrollStrategy = static_cast<VirtualListScrollStrategy>(strategy);
        }
        w->scrollToIndex(static_cast<size_t>(index), scrollStrategy);
    }
}

void fluxui_canvas_set_on_draw(FluxUIWidget* canvas, FluxUIDrawCallback callback, void* user_data) {
    auto* c = as<Canvas>(canvas);
    if (!c) return;
    if (!callback) {
        c->onDraw = nullptr;
        return;
    }
    c->onDraw = [canvas, callback, user_data](Renderer& renderer, const Rect& bounds) {
        FluxUIRect cBounds = { bounds.x, bounds.y, bounds.w, bounds.h };
        callback(canvas, &renderer, cBounds, user_data);
    };
}

void fluxui_draw_rect(void* renderer_ptr, FluxUIRect rect, FluxUIColor color) {
    if (!renderer_ptr) return;
    auto* r = reinterpret_cast<Renderer*>(renderer_ptr);
    r->drawRoundedRect(Rect(rect.x, rect.y, rect.w, rect.h), to_color(color), BorderRadius(0.0f));
}

void fluxui_draw_text(void* renderer_ptr, const char* text, float x, float y, FluxUIColor color, float font_size) {
    if (!renderer_ptr) return;
    auto* r = reinterpret_cast<Renderer*>(renderer_ptr);
    r->drawText(safe_cstr(text), Vec2(x, y), to_color(color), font_size);
}

void fluxui_draw_image(void* renderer_ptr, const char* name_or_path, FluxUIRect rect, float opacity) {
    if (!renderer_ptr) return;
    auto* r = reinterpret_cast<Renderer*>(renderer_ptr);
    r->drawImage(safe_cstr(name_or_path), Rect(rect.x, rect.y, rect.w, rect.h), opacity);
}

void fluxui_renderer_flush(void* renderer_ptr) {
    if (!renderer_ptr) return;
    auto* r = reinterpret_cast<Renderer*>(renderer_ptr);
    r->flush();
}

void fluxui_widget_set_id(FluxUIWidget* widget, const char* id) {
    Widget* w = as_widget(widget);
    if (!w) return;
    w->id = safe_cstr(id);
    w->markStyleDirty();
}

void fluxui_widget_set_class(FluxUIWidget* widget, const char* class_name) {
    Widget* w = as_widget(widget);
    if (!w) return;
    w->className = safe_cstr(class_name);
    w->markStyleDirty();
}

void fluxui_widget_set_visible(FluxUIWidget* widget, int visible) {
    Widget* w = as_widget(widget);
    if (!w) return;
    w->visible = visible != 0;
    w->markLayoutDirty();
}

FluxUIRect fluxui_widget_get_bounds(FluxUIWidget* widget) {
    Widget* w = as_widget(widget);
    if (!w) return {0, 0, 0, 0};
    return {w->bounds.x, w->bounds.y, w->bounds.w, w->bounds.h};
}

void fluxui_widget_set_on_click(FluxUIWidget* widget,
                                FluxUIClickCallback callback,
                                void* user_data) {
    Widget* w = as_widget(widget);
    if (!w) return;
    if (!callback) {
        w->onClick = nullptr;
        return;
    }

    w->style.cursor = CursorType::Pointer;
    w->markStyleDirty();
    w->onClick = [widget, callback, user_data]() {
        callback(widget, user_data);
    };
}

void fluxui_text_set_content(FluxUIWidget* widget, const char* text) {
    if (auto* w = as<Text>(widget)) {
        w->content = safe_cstr(text);
        w->markLayoutDirty();
    }
}

void fluxui_button_set_label(FluxUIWidget* widget, const char* label) {
    if (auto* w = as<Button>(widget)) {
        w->label = safe_cstr(label);
        w->markLayoutDirty();
    }
}

void fluxui_text_input_set_value(FluxUIWidget* widget, const char* value) {
    if (auto* w = as<TextInput>(widget)) {
        w->value = safe_cstr(value);
        w->markLayoutDirty();
    }
}

const char* fluxui_text_input_get_value(FluxUIWidget* widget) {
    if (auto* w = as<TextInput>(widget)) {
        return w->value.c_str();
    }
    return "";
}

void fluxui_text_input_set_placeholder(FluxUIWidget* widget, const char* placeholder) {
    if (auto* w = as<TextInput>(widget)) {
        w->placeholder = safe_cstr(placeholder);
        w->markLayoutDirty();
    }
}

void fluxui_text_input_set_type(FluxUIWidget* widget, const char* input_type) {
    if (auto* w = as<TextInput>(widget)) {
        w->setInputType(safe_cstr(input_type));
        w->markLayoutDirty();
    }
}

void fluxui_checkbox_set_checked(FluxUIWidget* widget, int checked) {
    if (auto* w = as<Checkbox>(widget)) {
        w->setChecked(checked != 0);
    }
}

int fluxui_checkbox_get_checked(FluxUIWidget* widget) {
    if (auto* w = as<Checkbox>(widget)) {
        return w->checked ? 1 : 0;
    }
    return 0;
}

void fluxui_radio_set_checked(FluxUIWidget* widget, int checked) {
    if (auto* w = as<Radio>(widget)) {
        w->setChecked(checked != 0);
    }
}

int fluxui_radio_get_checked(FluxUIWidget* widget) {
    if (auto* w = as<Radio>(widget)) {
        return w->checked ? 1 : 0;
    }
    return 0;
}

void fluxui_range_set_value(FluxUIWidget* widget, float value) {
    if (auto* w = as<RangeInput>(widget)) {
        w->setValue(value);
    }
}

float fluxui_range_get_value(FluxUIWidget* widget) {
    if (auto* w = as<RangeInput>(widget)) {
        return w->value;
    }
    return 0.0f;
}

void fluxui_select_set_selected_index(FluxUIWidget* widget, uint32_t index) {
    if (auto* w = as<Select>(widget)) {
        w->selectIndex(static_cast<size_t>(index));
    }
}

uint32_t fluxui_select_get_selected_index(FluxUIWidget* widget) {
    if (auto* w = as<Select>(widget)) {
        return static_cast<uint32_t>(
            std::min<size_t>(w->selectedIndex, std::numeric_limits<uint32_t>::max()));
    }
    return 0;
}

void fluxui_details_set_open(FluxUIWidget* widget, int open) {
    if (auto* w = as<Details>(widget)) {
        w->open = open != 0;
        w->markLayoutDirty();
        w->markStyleDirtyRecursive();
    }
}

int fluxui_details_get_open(FluxUIWidget* widget) {
    if (auto* w = as<Details>(widget)) {
        return w->open ? 1 : 0;
    }
    return 0;
}

void fluxui_dialog_show(FluxUIWidget* widget) {
    if (auto* w = as<Dialog>(widget)) {
        w->show();
    }
}

void fluxui_dialog_show_modal(FluxUIWidget* widget) {
    if (auto* w = as<Dialog>(widget)) {
        w->showModal();
    }
}

void fluxui_dialog_close(FluxUIWidget* widget) {
    if (auto* w = as<Dialog>(widget)) {
        w->close();
    }
}

int fluxui_dialog_get_open(FluxUIWidget* widget) {
    if (auto* w = as<Dialog>(widget)) {
        return w->open ? 1 : 0;
    }
    return 0;
}

void fluxui_meter_set_value(FluxUIWidget* widget, float value) {
    if (auto* w = as<Meter>(widget)) {
        w->value = value;
        w->markStyleDirty();
    }
}

float fluxui_meter_get_value(FluxUIWidget* widget) {
    if (auto* w = as<Meter>(widget)) {
        return w->value;
    }
    return 0.0f;
}

void fluxui_progress_element_set_value(FluxUIWidget* widget, float value) {
    if (auto* w = as<Progress>(widget)) {
        w->value = value;
        w->markStyleDirty();
    }
}

float fluxui_progress_element_get_value(FluxUIWidget* widget) {
    if (auto* w = as<Progress>(widget)) {
        return w->value;
    }
    return 0.0f;
}

void fluxui_icon_set_glyph(FluxUIWidget* widget, const char* glyph) {
    if (auto* w = as<Icon>(widget)) {
        w->glyph = safe_cstr(glyph);
        w->markLayoutDirty();
    }
}

void fluxui_progress_bar_set_value(FluxUIWidget* widget, float progress) {
    if (auto* w = as<ProgressBar>(widget)) {
        w->progress = std::clamp(progress, 0.0f, 1.0f);
    }
}

void fluxui_progress_bar_set_color(FluxUIWidget* widget, FluxUIColor color) {
    if (auto* w = as<ProgressBar>(widget)) {
        w->barColor = to_color(color);
    }
}

FluxUIColor fluxui_color_rgba(float r, float g, float b, float a) {
    return {r, g, b, a};
}

FluxUIColor fluxui_color_rgb_u8(uint8_t r, uint8_t g, uint8_t b) {
    return from_color(Color::fromRGBA(r, g, b, 255));
}

void fluxui_style_width_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.width = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_height_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.height = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_min_width_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.minWidth = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_min_height_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.minHeight = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_max_width_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.maxWidth = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_max_height_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.maxHeight = CSSValue::px(value); w->markStyleDirty(); }
}

void fluxui_style_flex_grow(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.flexGrow = value; w->markStyleDirty(); }
}

void fluxui_style_gap_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.gap = value; w->markStyleDirty(); }
}

void fluxui_style_padding_all_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.padding = EdgeInsets(value); w->markStyleDirty(); }
}

void fluxui_style_padding_px(FluxUIWidget* widget,
                             float top,
                             float right,
                             float bottom,
                             float left) {
    if (auto* w = as_widget(widget)) { w->style.padding = EdgeInsets(top, right, bottom, left); w->markStyleDirty(); }
}

void fluxui_style_margin_all_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.margin = EdgeInsets(value); w->markStyleDirty(); }
}

void fluxui_style_margin_px(FluxUIWidget* widget,
                            float top,
                            float right,
                            float bottom,
                            float left) {
    if (auto* w = as_widget(widget)) { w->style.margin = EdgeInsets(top, right, bottom, left); w->markStyleDirty(); }
}

void fluxui_style_border_radius_px(FluxUIWidget* widget, float value) {
    if (auto* w = as_widget(widget)) { w->style.borderRadius = BorderRadius(value); w->markStyleDirty(); }
}

void fluxui_style_background_color(FluxUIWidget* widget, FluxUIColor color) {
    if (auto* w = as_widget(widget)) { w->style.backgroundColor = to_color(color); w->markStyleDirty(); }
}

void fluxui_style_text_color(FluxUIWidget* widget, FluxUIColor color) {
    if (auto* w = as_widget(widget)) { w->style.color = to_color(color); w->markStyleDirty(); }
}

void fluxui_widget_css(FluxUIWidget* widget, const char* declarations) {
    if (auto* w = as_widget(widget)) { w->css(safe_cstr(declarations)); }
}

} // extern "C"
