#include "fluxui/fluxui_c.h"

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

JavaVM* g_vm = nullptr;

FluxUIApp* as_app(jlong handle) {
    return reinterpret_cast<FluxUIApp*>(static_cast<intptr_t>(handle));
}

FluxUIWidget* as_widget(jlong handle) {
    return reinterpret_cast<FluxUIWidget*>(static_cast<intptr_t>(handle));
}

jlong as_jlong(void* ptr) {
    return static_cast<jlong>(reinterpret_cast<intptr_t>(ptr));
}

class UtfChars {
public:
    UtfChars(JNIEnv* env, jstring value)
        : env_(env), value_(value) {
        if (value_) {
            chars_ = env_->GetStringUTFChars(value_, nullptr);
        }
    }

    ~UtfChars() {
        if (value_ && chars_) {
            env_->ReleaseStringUTFChars(value_, chars_);
        }
    }

    const char* c_str() const {
        return chars_ ? chars_ : "";
    }

private:
    JNIEnv* env_;
    jstring value_;
    const char* chars_ = nullptr;
};

JNIEnv* current_env(bool& detach) {
    detach = false;
    if (!g_vm) return nullptr;
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }
    if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) == JNI_OK) {
        detach = true;
        return env;
    }
    return nullptr;
}

void finish_callback(JNIEnv* env, bool detach) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (detach && g_vm) {
        g_vm->DetachCurrentThread();
    }
}

void java_click_callback(FluxUIWidget*, void* user_data) {
    jobject runnable = reinterpret_cast<jobject>(user_data);
    if (!runnable) return;
    bool detach = false;
    JNIEnv* env = current_env(detach);
    if (!env) return;

    jclass cls = env->GetObjectClass(runnable);
    jmethodID run = cls ? env->GetMethodID(cls, "run", "()V") : nullptr;
    if (run) {
        env->CallVoidMethod(runnable, run);
    }
    if (cls) env->DeleteLocalRef(cls);
    finish_callback(env, detach);
}

void java_update_callback(FluxUIApp*, float delta_time, void* user_data) {
    jobject callback = reinterpret_cast<jobject>(user_data);
    if (!callback) return;
    bool detach = false;
    JNIEnv* env = current_env(detach);
    if (!env) return;

    jclass cls = env->GetObjectClass(callback);
    jmethodID update = cls ? env->GetMethodID(cls, "update", "(F)V") : nullptr;
    if (update) {
        env->CallVoidMethod(callback, update, static_cast<jfloat>(delta_time));
    }
    if (cls) env->DeleteLocalRef(cls);
    finish_callback(env, detach);
}

void java_route_callback(FluxUIApp*, FluxUIWidget* container, const char* route, void* user_data) {
    jobject builder = reinterpret_cast<jobject>(user_data);
    if (!builder) return;
    bool detach = false;
    JNIEnv* env = current_env(detach);
    if (!env) return;

    jclass widgetClass = env->FindClass("io/fluxui/Widget");
    jmethodID widgetCtor = widgetClass ? env->GetMethodID(widgetClass, "<init>", "(J)V") : nullptr;
    jobject widget = widgetCtor ? env->NewObject(widgetClass, widgetCtor, as_jlong(container)) : nullptr;
    jstring routeString = env->NewStringUTF(route ? route : "");

    jclass builderClass = env->GetObjectClass(builder);
    jmethodID build = builderClass
        ? env->GetMethodID(builderClass, "build", "(Lio/fluxui/Widget;Ljava/lang/String;)V")
        : nullptr;
    if (build && widget && routeString) {
        env->CallVoidMethod(builder, build, widget, routeString);
    }

    if (routeString) env->DeleteLocalRef(routeString);
    if (widget) env->DeleteLocalRef(widget);
    if (builderClass) env->DeleteLocalRef(builderClass);
    if (widgetClass) env->DeleteLocalRef(widgetClass);
    finish_callback(env, detach);
}

void java_virtual_list_item_callback(FluxUIWidget* item, uint32_t index, void* user_data) {
    jobject builder = reinterpret_cast<jobject>(user_data);
    if (!builder) return;
    bool detach = false;
    JNIEnv* env = current_env(detach);
    if (!env) return;

    jclass widgetClass = env->FindClass("io/fluxui/Widget");
    jmethodID widgetCtor = widgetClass ? env->GetMethodID(widgetClass, "<init>", "(J)V") : nullptr;
    jobject row = widgetCtor ? env->NewObject(widgetClass, widgetCtor, as_jlong(item)) : nullptr;

    jclass builderClass = env->GetObjectClass(builder);
    jmethodID build = builderClass
        ? env->GetMethodID(builderClass, "build", "(Lio/fluxui/Widget;I)V")
        : nullptr;
    if (build && row) {
        env->CallVoidMethod(builder, build, row, static_cast<jint>(index));
    }

    if (row) env->DeleteLocalRef(row);
    if (builderClass) env->DeleteLocalRef(builderClass);
    if (widgetClass) env->DeleteLocalRef(widgetClass);
    finish_callback(env, detach);
}

} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_appCreate(JNIEnv*, jclass) {
    return as_jlong(fluxui_app_create());
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appDestroy(JNIEnv*, jclass, jlong app) {
    fluxui_app_destroy(as_app(app));
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appInit(
    JNIEnv* env,
    jclass,
    jlong app,
    jstring title,
    jint width,
    jint height) {
    UtfChars titleChars(env, title);
    return fluxui_app_init(as_app(app), titleChars.c_str(), width, height) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appSetBackend(
    JNIEnv*,
    jclass,
    jlong app,
    jint backend) {
    fluxui_app_set_backend(as_app(app), static_cast<FluxUIRenderBackend>(backend));
}

extern "C" JNIEXPORT jint JNICALL Java_io_fluxui_Native_appGetBackend(JNIEnv*, jclass, jlong app) {
    return static_cast<jint>(fluxui_app_get_backend(as_app(app)));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appRun(JNIEnv*, jclass, jlong app) {
    fluxui_app_run(as_app(app));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appStop(JNIEnv*, jclass, jlong app) {
    fluxui_app_stop(as_app(app));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appShutdown(JNIEnv*, jclass, jlong app) {
    fluxui_app_shutdown(as_app(app));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appAddStylesheet(
    JNIEnv* env,
    jclass,
    jlong app,
    jstring css) {
    UtfChars cssChars(env, css);
    fluxui_app_add_stylesheet(as_app(app), cssChars.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appLoadFont(
    JNIEnv* env,
    jclass,
    jlong app,
    jstring path,
    jfloat size) {
    UtfChars pathChars(env, path);
    return fluxui_app_load_font(as_app(app), pathChars.c_str(), size) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appLoadDefaultFont(
    JNIEnv*,
    jclass,
    jlong app,
    jfloat size) {
    return fluxui_app_load_default_font(as_app(app), size) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appWarmFontCache(
    JNIEnv* env,
    jclass,
    jlong app,
    jfloatArray sizes,
    jstring name) {
    UtfChars nameChars(env, name);
    if (!sizes) {
        fluxui_app_warm_font_cache(as_app(app), nullptr, 0, nameChars.c_str());
        return;
    }
    jsize count = env->GetArrayLength(sizes);
    jfloat* values = env->GetFloatArrayElements(sizes, nullptr);
    fluxui_app_warm_font_cache(
        as_app(app),
        values,
        static_cast<uint32_t>(count),
        nameChars.c_str());
    env->ReleaseFloatArrayElements(sizes, values, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_appReleaseFontSources(
    JNIEnv*,
    jclass,
    jlong app) {
    fluxui_app_release_font_sources(as_app(app));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_appSetUpdateCallback(
    JNIEnv* env,
    jclass,
    jlong app,
    jobject callback) {
    if (!callback) {
        fluxui_app_set_update_callback(as_app(app), nullptr, nullptr);
        return 0;
    }
    jobject callbackRef = env->NewGlobalRef(callback);
    fluxui_app_set_update_callback(as_app(app), java_update_callback, callbackRef);
    return as_jlong(callbackRef);
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_appRoot(JNIEnv*, jclass, jlong app) {
    return as_jlong(fluxui_app_root(as_app(app)));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_appAddRoute(
    JNIEnv* env,
    jclass,
    jlong app,
    jstring path,
    jobject builder) {
    UtfChars pathChars(env, path);
    if (!builder) {
        return 0;
    }
    jobject builderRef = env->NewGlobalRef(builder);
    fluxui_app_add_route(as_app(app), pathChars.c_str(), java_route_callback, builderRef);
    return as_jlong(builderRef);
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appNavigate(
    JNIEnv* env,
    jclass,
    jlong app,
    jstring path) {
    UtfChars pathChars(env, path);
    return fluxui_app_navigate(as_app(app), pathChars.c_str()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL Java_io_fluxui_Native_appCurrentRoute(
    JNIEnv* env,
    jclass,
    jlong app) {
    const char* route = fluxui_app_current_route(as_app(app));
    return env->NewStringUTF(route ? route : "");
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appRouteDirty(
    JNIEnv*,
    jclass,
    jlong app) {
    return fluxui_app_route_dirty(as_app(app)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_fluxui_Native_appRenderRoute(
    JNIEnv*,
    jclass,
    jlong app,
    jlong container) {
    return fluxui_app_render_route(as_app(app), as_widget(container)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_widgetClearChildren(
    JNIEnv*,
    jclass,
    jlong widget) {
    fluxui_widget_clear_children(as_widget(widget));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_widgetReserveChildren(
    JNIEnv*,
    jclass,
    jlong widget,
    jint count) {
    fluxui_widget_reserve_children(as_widget(widget), static_cast<uint32_t>(count));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddPanel(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring className) {
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_panel(as_widget(parent), cls.c_str()));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddText(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring text,
    jstring className) {
    UtfChars textChars(env, text);
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_text(as_widget(parent), textChars.c_str(), cls.c_str()));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddButton(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring label,
    jstring className) {
    UtfChars labelChars(env, label);
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_button(as_widget(parent), labelChars.c_str(), cls.c_str()));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddTextInput(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring placeholder,
    jstring className) {
    UtfChars placeholderChars(env, placeholder);
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_text_input(
        as_widget(parent),
        placeholderChars.c_str(),
        cls.c_str()));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddIcon(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring glyph,
    jstring className) {
    UtfChars glyphChars(env, glyph);
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_icon(as_widget(parent), glyphChars.c_str(), cls.c_str()));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddProgressBar(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring className,
    jfloat progress) {
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_progress_bar(as_widget(parent), cls.c_str(), progress));
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetAddCanvas(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring className) {
    UtfChars cls(env, className);
    return as_jlong(fluxui_widget_add_canvas(as_widget(parent), cls.c_str()));
}

extern "C" JNIEXPORT jlongArray JNICALL Java_io_fluxui_Native_widgetAddVirtualList(
    JNIEnv* env,
    jclass,
    jlong parent,
    jstring className,
    jint itemCount,
    jfloat itemHeight,
    jobject builder) {
    UtfChars cls(env, className);
    jobject builderRef = builder ? env->NewGlobalRef(builder) : nullptr;
    FluxUIWidget* widget = fluxui_widget_add_virtual_list(
        as_widget(parent),
        cls.c_str(),
        static_cast<uint32_t>(itemCount),
        itemHeight,
        builderRef ? java_virtual_list_item_callback : nullptr,
        builderRef);
    if (!widget && builderRef) {
        env->DeleteGlobalRef(builderRef);
        builderRef = nullptr;
    }
    jlong values[2] = {as_jlong(widget), as_jlong(builderRef)};
    jlongArray result = env->NewLongArray(2);
    env->SetLongArrayRegion(result, 0, 2, values);
    return result;
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_virtualListSetItemCount(
    JNIEnv*,
    jclass,
    jlong widget,
    jint itemCount) {
    fluxui_virtual_list_set_item_count(as_widget(widget), static_cast<uint32_t>(itemCount));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_virtualListRefresh(
    JNIEnv*,
    jclass,
    jlong widget) {
    fluxui_virtual_list_refresh(as_widget(widget));
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_virtualListScrollToIndex(
    JNIEnv*,
    jclass,
    jlong widget,
    jint index,
    jint strategy) {
    fluxui_virtual_list_scroll_to_index(
        as_widget(widget),
        static_cast<uint32_t>(index),
        strategy);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_widgetSetId(
    JNIEnv* env,
    jclass,
    jlong widget,
    jstring id) {
    UtfChars idChars(env, id);
    fluxui_widget_set_id(as_widget(widget), idChars.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_widgetSetClass(
    JNIEnv* env,
    jclass,
    jlong widget,
    jstring className) {
    UtfChars cls(env, className);
    fluxui_widget_set_class(as_widget(widget), cls.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_widgetSetVisible(
    JNIEnv*,
    jclass,
    jlong widget,
    jboolean visible) {
    fluxui_widget_set_visible(as_widget(widget), visible == JNI_TRUE ? 1 : 0);
}

extern "C" JNIEXPORT jlong JNICALL Java_io_fluxui_Native_widgetSetOnClick(
    JNIEnv* env,
    jclass,
    jlong widget,
    jobject callback) {
    if (!callback) {
        fluxui_widget_set_on_click(as_widget(widget), nullptr, nullptr);
        return 0;
    }
    jobject callbackRef = env->NewGlobalRef(callback);
    fluxui_widget_set_on_click(as_widget(widget), java_click_callback, callbackRef);
    return as_jlong(callbackRef);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_textSetContent(
    JNIEnv* env,
    jclass,
    jlong widget,
    jstring text) {
    UtfChars textChars(env, text);
    fluxui_text_set_content(as_widget(widget), textChars.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_buttonSetLabel(
    JNIEnv* env,
    jclass,
    jlong widget,
    jstring label) {
    UtfChars labelChars(env, label);
    fluxui_button_set_label(as_widget(widget), labelChars.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_progressBarSetValue(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat progress) {
    fluxui_progress_bar_set_value(as_widget(widget), progress);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_styleWidthPx(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat value) {
    fluxui_style_width_px(as_widget(widget), value);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_styleHeightPx(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat value) {
    fluxui_style_height_px(as_widget(widget), value);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_styleFlexGrow(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat value) {
    fluxui_style_flex_grow(as_widget(widget), value);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_styleGapPx(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat value) {
    fluxui_style_gap_px(as_widget(widget), value);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_stylePaddingAllPx(
    JNIEnv*,
    jclass,
    jlong widget,
    jfloat value) {
    fluxui_style_padding_all_px(as_widget(widget), value);
}

extern "C" JNIEXPORT void JNICALL Java_io_fluxui_Native_releaseCallback(
    JNIEnv* env,
    jclass,
    jlong callbackRef) {
    if (callbackRef != 0) {
        env->DeleteGlobalRef(reinterpret_cast<jobject>(static_cast<intptr_t>(callbackRef)));
    }
}
