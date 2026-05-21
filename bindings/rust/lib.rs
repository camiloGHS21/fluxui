//! FluxUI Rust bindings.
//!
//! Build as a reusable rustc library:
//!
//! ```text
//! rustc --crate-name fluxui --crate-type rlib bindings/rust/lib.rs \
//!   -L native=build/Release -o build/Release/libfluxui.rlib
//! ```

use std::ffi::{c_void, CStr, CString};
use std::ptr::NonNull;

pub mod sys {
    use std::ffi::{c_char, c_void};

    #[repr(C)]
    pub struct FluxUIApp {
        _private: [u8; 0],
    }

    #[repr(C)]
    pub struct FluxUIWidget {
        _private: [u8; 0],
    }

    #[repr(C)]
    #[derive(Clone, Copy, Debug, Default)]
    pub struct FluxUIColor {
        pub r: f32,
        pub g: f32,
        pub b: f32,
        pub a: f32,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Debug, Default)]
    pub struct FluxUIRect {
        pub x: f32,
        pub y: f32,
        pub w: f32,
        pub h: f32,
    }

    pub type FluxUIClickCallback =
        extern "C" fn(widget: *mut FluxUIWidget, user_data: *mut c_void);
    pub type FluxUIUpdateCallback =
        extern "C" fn(app: *mut FluxUIApp, delta_time: f32, user_data: *mut c_void);
    pub type FluxUIDrawCallback =
        extern "C" fn(widget: *mut FluxUIWidget, renderer_ptr: *mut c_void, bounds: FluxUIRect, user_data: *mut c_void);
    pub type FluxUIRenderBackend = i32;

    pub const FLUXUI_BACKEND_AUTO: FluxUIRenderBackend = 0;
    pub const FLUXUI_BACKEND_VULKAN: FluxUIRenderBackend = 1;
    pub const FLUXUI_BACKEND_DIRECT3D12: FluxUIRenderBackend = 2;
    pub const FLUXUI_BACKEND_DIRECT12: FluxUIRenderBackend = FLUXUI_BACKEND_DIRECT3D12;
    pub const FLUXUI_BACKEND_DIRECTX12: FluxUIRenderBackend = FLUXUI_BACKEND_DIRECT3D12;
    pub const FLUXUI_BACKEND_METAL: FluxUIRenderBackend = 3;
    pub const FLUXUI_BACKEND_COMPATIBILITY: FluxUIRenderBackend = 100;

    #[link(name = "fluxui_shared")]
    extern "C" {
        pub fn fluxui_app_create() -> *mut FluxUIApp;
        pub fn fluxui_app_destroy(app: *mut FluxUIApp);
        pub fn fluxui_app_set_backend(
            app: *mut FluxUIApp,
            backend: FluxUIRenderBackend,
        ) -> i32;
        pub fn fluxui_app_get_backend(app: *mut FluxUIApp) -> FluxUIRenderBackend;
        pub fn fluxui_default_backend() -> FluxUIRenderBackend;
        pub fn fluxui_backend_name(backend: FluxUIRenderBackend) -> *const c_char;
        pub fn fluxui_backend_is_compiled(backend: FluxUIRenderBackend) -> i32;
        pub fn fluxui_backend_is_selectable(backend: FluxUIRenderBackend) -> i32;
        pub fn fluxui_app_init(
            app: *mut FluxUIApp,
            title: *const c_char,
            width: i32,
            height: i32,
        ) -> i32;
        pub fn fluxui_app_shutdown(app: *mut FluxUIApp);
        pub fn fluxui_app_run(app: *mut FluxUIApp);
        pub fn fluxui_app_stop(app: *mut FluxUIApp);
        pub fn fluxui_app_load_stylesheet(app: *mut FluxUIApp, path: *const c_char) -> i32;
        pub fn fluxui_app_add_stylesheet(app: *mut FluxUIApp, css: *const c_char);
        pub fn fluxui_app_load_font(app: *mut FluxUIApp, path: *const c_char, size: f32) -> i32;
        pub fn fluxui_app_load_font_named(
            app: *mut FluxUIApp,
            path: *const c_char,
            size: f32,
            name: *const c_char,
        ) -> i32;
        pub fn fluxui_app_load_default_font(app: *mut FluxUIApp, size: f32) -> i32;
        pub fn fluxui_app_warm_font_cache(
            app: *mut FluxUIApp,
            sizes: *const f32,
            count: u32,
            name: *const c_char,
        );
        pub fn fluxui_app_release_font_sources(app: *mut FluxUIApp);
        pub fn fluxui_app_set_update_callback(
            app: *mut FluxUIApp,
            callback: Option<FluxUIUpdateCallback>,
            user_data: *mut c_void,
        );
        pub fn fluxui_app_root(app: *mut FluxUIApp) -> *mut FluxUIWidget;

        pub fn fluxui_widget_clear_children(widget: *mut FluxUIWidget);
        pub fn fluxui_widget_reserve_children(widget: *mut FluxUIWidget, count: u32);
        pub fn fluxui_widget_add_panel(
            parent: *mut FluxUIWidget,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_text(
            parent: *mut FluxUIWidget,
            text: *const c_char,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_button(
            parent: *mut FluxUIWidget,
            label: *const c_char,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_text_input(
            parent: *mut FluxUIWidget,
            placeholder: *const c_char,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_icon(
            parent: *mut FluxUIWidget,
            glyph: *const c_char,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_progress_bar(
            parent: *mut FluxUIWidget,
            class_name: *const c_char,
            progress: f32,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_stat_card(
            parent: *mut FluxUIWidget,
            title: *const c_char,
            value: *const c_char,
            subtitle: *const c_char,
            accent: FluxUIColor,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_widget_add_canvas(
            parent: *mut FluxUIWidget,
            class_name: *const c_char,
        ) -> *mut FluxUIWidget;
        pub fn fluxui_canvas_set_on_draw(
            canvas: *mut FluxUIWidget,
            callback: Option<FluxUIDrawCallback>,
            user_data: *mut c_void,
        );
        pub fn fluxui_draw_rect(renderer_ptr: *mut c_void, rect: FluxUIRect, color: FluxUIColor);
        pub fn fluxui_draw_text(
            renderer_ptr: *mut c_void,
            text: *const c_char,
            x: f32,
            y: f32,
            color: FluxUIColor,
            font_size: f32,
        );
        pub fn fluxui_draw_image(
            renderer_ptr: *mut c_void,
            name_or_path: *const c_char,
            rect: FluxUIRect,
            opacity: f32,
        );
        pub fn fluxui_renderer_flush(renderer_ptr: *mut c_void);

        pub fn fluxui_widget_set_id(widget: *mut FluxUIWidget, id: *const c_char);
        pub fn fluxui_widget_set_class(widget: *mut FluxUIWidget, class_name: *const c_char);
        pub fn fluxui_widget_set_visible(widget: *mut FluxUIWidget, visible: i32);
        pub fn fluxui_widget_get_bounds(widget: *mut FluxUIWidget) -> FluxUIRect;
        pub fn fluxui_widget_set_on_click(
            widget: *mut FluxUIWidget,
            callback: Option<FluxUIClickCallback>,
            user_data: *mut c_void,
        );

        pub fn fluxui_text_set_content(widget: *mut FluxUIWidget, text: *const c_char);
        pub fn fluxui_button_set_label(widget: *mut FluxUIWidget, label: *const c_char);
        pub fn fluxui_text_input_set_value(widget: *mut FluxUIWidget, value: *const c_char);
        pub fn fluxui_text_input_get_value(widget: *mut FluxUIWidget) -> *const c_char;
        pub fn fluxui_text_input_set_placeholder(
            widget: *mut FluxUIWidget,
            placeholder: *const c_char,
        );
        pub fn fluxui_icon_set_glyph(widget: *mut FluxUIWidget, glyph: *const c_char);
        pub fn fluxui_progress_bar_set_value(widget: *mut FluxUIWidget, progress: f32);
        pub fn fluxui_progress_bar_set_color(widget: *mut FluxUIWidget, color: FluxUIColor);

        pub fn fluxui_color_rgba(r: f32, g: f32, b: f32, a: f32) -> FluxUIColor;
        pub fn fluxui_color_rgb_u8(r: u8, g: u8, b: u8) -> FluxUIColor;
        pub fn fluxui_style_width_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_height_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_min_width_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_min_height_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_max_width_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_max_height_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_flex_grow(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_gap_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_padding_all_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_padding_px(
            widget: *mut FluxUIWidget,
            top: f32,
            right: f32,
            bottom: f32,
            left: f32,
        );
        pub fn fluxui_style_margin_all_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_margin_px(
            widget: *mut FluxUIWidget,
            top: f32,
            right: f32,
            bottom: f32,
            left: f32,
        );
        pub fn fluxui_style_border_radius_px(widget: *mut FluxUIWidget, value: f32);
        pub fn fluxui_style_background_color(widget: *mut FluxUIWidget, color: FluxUIColor);
        pub fn fluxui_style_text_color(widget: *mut FluxUIWidget, color: FluxUIColor);
    }
}

pub use sys::{FluxUIColor as Color, FluxUIRect as Rect};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Backend {
    Auto,
    Vulkan,
    Direct3D12,
    Direct12,
    DirectX12,
    Metal,
    Compatibility,
}

impl Backend {
    fn as_sys(self) -> sys::FluxUIRenderBackend {
        match self {
            Backend::Auto => sys::FLUXUI_BACKEND_AUTO,
            Backend::Vulkan => sys::FLUXUI_BACKEND_VULKAN,
            Backend::Direct3D12 => sys::FLUXUI_BACKEND_DIRECT3D12,
            Backend::Direct12 => sys::FLUXUI_BACKEND_DIRECT12,
            Backend::DirectX12 => sys::FLUXUI_BACKEND_DIRECTX12,
            Backend::Metal => sys::FLUXUI_BACKEND_METAL,
            Backend::Compatibility => sys::FLUXUI_BACKEND_COMPATIBILITY,
        }
    }

    fn from_sys(value: sys::FluxUIRenderBackend) -> Self {
        match value {
            sys::FLUXUI_BACKEND_VULKAN => Backend::Vulkan,
            sys::FLUXUI_BACKEND_DIRECT3D12 => Backend::Direct3D12,
            sys::FLUXUI_BACKEND_METAL => Backend::Metal,
            sys::FLUXUI_BACKEND_COMPATIBILITY => Backend::Compatibility,
            _ => Backend::Auto,
        }
    }

    pub fn name(self) -> &'static str {
        let ptr = unsafe { sys::fluxui_backend_name(self.as_sys()) };
        if ptr.is_null() {
            return "";
        }
        unsafe { CStr::from_ptr(ptr) }.to_str().unwrap_or("")
    }

    pub fn is_compiled(self) -> bool {
        unsafe { sys::fluxui_backend_is_compiled(self.as_sys()) != 0 }
    }

    pub fn is_selectable(self) -> bool {
        unsafe { sys::fluxui_backend_is_selectable(self.as_sys()) != 0 }
    }

    pub fn default() -> Self {
        Backend::from_sys(unsafe { sys::fluxui_default_backend() })
    }
}

impl Default for Backend {
    fn default() -> Self {
        Backend::from_sys(unsafe { sys::fluxui_default_backend() })
    }
}

#[derive(Debug)]
pub enum Error {
    CreateFailed,
    InitFailed,
    InvalidString,
}

pub type Result<T> = std::result::Result<T, Error>;

pub struct App {
    raw: NonNull<sys::FluxUIApp>,
}

#[derive(Clone, Copy)]
pub struct Widget {
    raw: NonNull<sys::FluxUIWidget>,
}

impl App {
    pub fn create() -> Result<Self> {
        let raw = unsafe { sys::fluxui_app_create() };
        let raw = NonNull::new(raw).ok_or(Error::CreateFailed)?;
        Ok(Self { raw })
    }

    pub fn set_backend(&self, backend: Backend) -> bool {
        unsafe { sys::fluxui_app_set_backend(self.raw.as_ptr(), backend.as_sys()) != 0 }
    }

    pub fn backend(&self) -> Backend {
        Backend::from_sys(unsafe { sys::fluxui_app_get_backend(self.raw.as_ptr()) })
    }

    pub fn init(&self, title: &str, width: i32, height: i32) -> Result<()> {
        let title = cstring(title)?;
        let ok = unsafe { sys::fluxui_app_init(self.raw.as_ptr(), title.as_ptr(), width, height) };
        if ok == 0 {
            return Err(Error::InitFailed);
        }
        Ok(())
    }

    pub fn run(&self) {
        unsafe { sys::fluxui_app_run(self.raw.as_ptr()) }
    }

    pub fn stop(&self) {
        unsafe { sys::fluxui_app_stop(self.raw.as_ptr()) }
    }

    pub fn shutdown(&self) {
        unsafe { sys::fluxui_app_shutdown(self.raw.as_ptr()) }
    }

    pub fn load_font(&self, path: &str, size: f32) -> bool {
        let Ok(path) = cstring(path) else { return false };
        unsafe { sys::fluxui_app_load_font(self.raw.as_ptr(), path.as_ptr(), size) != 0 }
    }

    pub fn load_default_font(&self, size: f32) -> bool {
        unsafe { sys::fluxui_app_load_default_font(self.raw.as_ptr(), size) != 0 }
    }

    pub fn warm_font_cache(&self, sizes: &[f32], name: &str) -> bool {
        if sizes.is_empty() {
            return true;
        }
        let Ok(name) = cstring(name) else { return false };
        unsafe {
            sys::fluxui_app_warm_font_cache(
                self.raw.as_ptr(),
                sizes.as_ptr(),
                sizes.len().min(u32::MAX as usize) as u32,
                name.as_ptr(),
            );
        }
        true
    }

    pub fn release_font_sources(&self) {
        unsafe { sys::fluxui_app_release_font_sources(self.raw.as_ptr()) }
    }

    pub fn load_stylesheet(&self, path: &str) -> bool {
        let Ok(path) = cstring(path) else { return false };
        unsafe { sys::fluxui_app_load_stylesheet(self.raw.as_ptr(), path.as_ptr()) != 0 }
    }

    pub fn add_stylesheet(&self, css: &str) -> Result<()> {
        let css = cstring(css)?;
        unsafe { sys::fluxui_app_add_stylesheet(self.raw.as_ptr(), css.as_ptr()) };
        Ok(())
    }

    pub fn root(&self) -> Option<Widget> {
        let raw = unsafe { sys::fluxui_app_root(self.raw.as_ptr()) };
        NonNull::new(raw).map(|raw| Widget { raw })
    }

    pub fn raw(&self) -> *mut sys::FluxUIApp {
        self.raw.as_ptr()
    }
}

impl Drop for App {
    fn drop(&mut self) {
        unsafe { sys::fluxui_app_destroy(self.raw.as_ptr()) }
    }
}

impl Widget {
    pub fn add_panel(self, class_name: &str) -> Result<Option<Widget>> {
        let class_name = cstring(class_name)?;
        Ok(widget_from_ptr(unsafe {
            sys::fluxui_widget_add_panel(self.raw.as_ptr(), class_name.as_ptr())
        }))
    }

    pub fn add_text(self, text: &str, class_name: &str) -> Result<Option<Widget>> {
        let text = cstring(text)?;
        let class_name = cstring(class_name)?;
        Ok(widget_from_ptr(unsafe {
            sys::fluxui_widget_add_text(self.raw.as_ptr(), text.as_ptr(), class_name.as_ptr())
        }))
    }

    pub fn add_button(self, label: &str, class_name: &str) -> Result<Option<Widget>> {
        let label = cstring(label)?;
        let class_name = cstring(class_name)?;
        Ok(widget_from_ptr(unsafe {
            sys::fluxui_widget_add_button(self.raw.as_ptr(), label.as_ptr(), class_name.as_ptr())
        }))
    }

    pub fn add_text_input(self, placeholder: &str, class_name: &str) -> Result<Option<Widget>> {
        let placeholder = cstring(placeholder)?;
        let class_name = cstring(class_name)?;
        Ok(widget_from_ptr(unsafe {
            sys::fluxui_widget_add_text_input(
                self.raw.as_ptr(),
                placeholder.as_ptr(),
                class_name.as_ptr(),
            )
        }))
    }

    pub fn add_canvas(self, class_name: &str) -> Result<Option<Widget>> {
        let class_name = cstring(class_name)?;
        Ok(widget_from_ptr(unsafe {
            sys::fluxui_widget_add_canvas(
                self.raw.as_ptr(),
                class_name.as_ptr(),
            )
        }))
    }

    pub fn set_on_draw<F>(self, callback: F)
    where
        F: Fn(Widget, &Renderer, Rect) + 'static,
    {
        let boxed = Box::new(callback);
        let ptr = Box::into_raw(boxed);
        unsafe {
            sys::fluxui_canvas_set_on_draw(
                self.raw.as_ptr(),
                Some(rust_draw_callback::<F>),
                ptr as *mut c_void,
            );
        }
    }

    pub fn clear_children(self) {
        unsafe { sys::fluxui_widget_clear_children(self.raw.as_ptr()) }
    }

    pub fn reserve_children(self, count: u32) {
        unsafe { sys::fluxui_widget_reserve_children(self.raw.as_ptr(), count) }
    }

    pub fn bounds(self) -> Rect {
        unsafe { sys::fluxui_widget_get_bounds(self.raw.as_ptr()) }
    }

    pub fn set_on_click_raw(
        self,
        callback: sys::FluxUIClickCallback,
        user_data: *mut c_void,
    ) {
        unsafe { sys::fluxui_widget_set_on_click(self.raw.as_ptr(), Some(callback), user_data) }
    }

    pub fn set_on_click_stop_app(self, app: &App) {
        self.set_on_click_raw(stop_app_callback, app.raw() as *mut c_void);
    }

    pub fn raw(self) -> *mut sys::FluxUIWidget {
        self.raw.as_ptr()
    }
}

pub extern "C" fn stop_app_callback(
    _widget: *mut sys::FluxUIWidget,
    user_data: *mut c_void,
) {
    unsafe { sys::fluxui_app_stop(user_data as *mut sys::FluxUIApp) }
}

pub fn color_rgba(r: f32, g: f32, b: f32, a: f32) -> Color {
    unsafe { sys::fluxui_color_rgba(r, g, b, a) }
}

pub fn color_rgb_u8(r: u8, g: u8, b: u8) -> Color {
    unsafe { sys::fluxui_color_rgb_u8(r, g, b) }
}

fn cstring(value: &str) -> Result<CString> {
    CString::new(value).map_err(|_| Error::InvalidString)
}

fn widget_from_ptr(raw: *mut sys::FluxUIWidget) -> Option<Widget> {
    NonNull::new(raw).map(|raw| Widget { raw })
}

pub unsafe fn input_value<'a>(widget: Widget) -> &'a CStr {
    CStr::from_ptr(sys::fluxui_text_input_get_value(widget.raw.as_ptr()))
}

pub struct Renderer {
    raw: *mut std::ffi::c_void,
}

impl Renderer {
    pub fn draw_rect(&self, rect: Rect, color: Color) {
        unsafe { sys::fluxui_draw_rect(self.raw, rect, color) }
    }

    pub fn draw_text(&self, text: &str, x: f32, y: f32, color: Color, font_size: f32) -> Result<()> {
        let text = cstring(text)?;
        unsafe { sys::fluxui_draw_text(self.raw, text.as_ptr(), x, y, color, font_size) }
        Ok(())
    }

    pub fn draw_image(&self, name_or_path: &str, rect: Rect, opacity: f32) -> Result<()> {
        let name_or_path = cstring(name_or_path)?;
        unsafe { sys::fluxui_draw_image(self.raw, name_or_path.as_ptr(), rect, opacity) }
        Ok(())
    }

    pub fn flush(&self) {
        unsafe { sys::fluxui_renderer_flush(self.raw) }
    }
}

pub extern "C" fn rust_draw_callback<F>(
    widget: *mut sys::FluxUIWidget,
    renderer_ptr: *mut c_void,
    bounds: sys::FluxUIRect,
    user_data: *mut c_void,
) where
    F: Fn(Widget, &Renderer, Rect) + 'static,
{
    let closure = unsafe { &*(user_data as *const F) };
    let w = Widget { raw: NonNull::new(widget).unwrap() };
    let r = Renderer { raw: renderer_ptr };
    closure(w, &r, bounds);
}
