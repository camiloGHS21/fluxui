#pragma once
// FluxUI public API - ecosystem: Store, Schema/Rule, Query, Skeleton helpers.
// Auto-split from dsl.h; do not include directly, use <fluxui/dsl.h>.
#include "fluxui/detail/dsl_app.h"
namespace fluxui {

// ============================================================
//  Store<T> — global state container (Zustand-style)
//
//  Like Zustand: a single shared store with state + actions. Components read
//  via get()/select() and subscribe to changes. Mutations notify subscribers
//  and request a redraw. Use it as a process-global singleton:
//
//      struct CartState { int count = 0; };
//      auto& cart = useStore<CartState>();
//      cart.set([](CartState& s){ s.count++; });
//      Text([&]{ return std::to_string(cart.get().count); });
// ============================================================
template <typename T>
class Store {
    T state_;
    std::vector<std::function<void()>> subscribers_;
public:
    Store() = default;
    explicit Store(T initial) : state_(std::move(initial)) {}

    const T& get() const { return state_; }

    // Mutate via a reducer-style function; notifies all subscribers.
    void set(std::function<void(T&)> mutator) {
        mutator(state_);
        notify();
    }
    // Replace the whole state.
    void replace(T next) { state_ = std::move(next); notify(); }

    // Read a derived value.
    template <typename R>
    R select(std::function<R(const T&)> selector) const { return selector(state_); }

    void subscribe(std::function<void()> fn) { subscribers_.push_back(std::move(fn)); }

    void notify() {
        for (auto& fn : subscribers_) fn();
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
    }
};

// Process-global store accessor (Zustand-style singleton per type).
template <typename T>
inline Store<T>& useStore() {
    static Store<T> store;
    return store;
}

// ============================================================
//  Schema — runtime validation (Zod-style)
//
//  Build a schema and validate values, collecting human-readable errors:
//
//      auto schema = Schema()
//          .field("email", Rule::string().email())
//          .field("age",   Rule::number().min(18));
//      auto result = schema.validate({{"email","a@b.com"},{"age","20"}});
//      if (!result.ok) for (auto& e : result.errors) ...
// ============================================================
struct Rule {
    enum class Kind { String, Number, Bool };
    Kind kind = Kind::String;
    bool required_ = true;
    std::optional<double> min_, max_;
    std::optional<size_t> minLen_, maxLen_;
    bool email_ = false;

    static Rule string() { Rule r; r.kind = Kind::String; return r; }
    static Rule number() { Rule r; r.kind = Kind::Number; return r; }
    static Rule boolean() { Rule r; r.kind = Kind::Bool; return r; }

    Rule& optional() { required_ = false; return *this; }
    Rule& min(double v) { min_ = v; return *this; }
    Rule& max(double v) { max_ = v; return *this; }
    Rule& minLength(size_t v) { minLen_ = v; return *this; }
    Rule& maxLength(size_t v) { maxLen_ = v; return *this; }
    Rule& email() { email_ = true; return *this; }

    // Validate a string value; returns an error message or empty if valid.
    std::string check(const std::string& field, const std::string& value) const {
        if (value.empty()) {
            return required_ ? (field + " is required") : "";
        }
        if (kind == Kind::Number) {
            try {
                double d = std::stod(value);
                if (min_ && d < *min_) return field + " must be >= " + std::to_string((long long)*min_);
                if (max_ && d > *max_) return field + " must be <= " + std::to_string((long long)*max_);
            } catch (...) {
                return field + " must be a number";
            }
        } else if (kind == Kind::String) {
            if (minLen_ && value.size() < *minLen_) return field + " is too short";
            if (maxLen_ && value.size() > *maxLen_) return field + " is too long";
            if (email_ && value.find('@') == std::string::npos) return field + " must be a valid email";
        } else if (kind == Kind::Bool) {
            if (value != "true" && value != "false") return field + " must be true/false";
        }
        return "";
    }
};

struct ValidationResult {
    bool ok = true;
    std::map<std::string, std::string> errors;  // field -> message
    std::string first() const { return errors.empty() ? "" : errors.begin()->second; }
};

class Schema {
    std::vector<std::pair<std::string, Rule>> fields_;
public:
    Schema& field(const std::string& name, Rule rule) {
        fields_.emplace_back(name, std::move(rule));
        return *this;
    }
    ValidationResult validate(const std::map<std::string, std::string>& data) const {
        ValidationResult res;
        for (const auto& [name, rule] : fields_) {
            auto it = data.find(name);
            std::string val = it != data.end() ? it->second : "";
            std::string err = rule.check(name, val);
            if (!err.empty()) { res.ok = false; res.errors[name] = err; }
        }
        return res;
    }
};

// ============================================================
//  Query<T> — async data fetching with loading/error/data states
//  (React Query / SWR-style). Runs a worker on a background thread and exposes
//  reactive status. Render different UI per state with .view():
//
//      static auto users = Query<std::string>([]{ return httpGet("/api/users"); });
//      users.start();
//      // in a view:
//      users.view(
//          []{ return Skeleton(3); },                     // loading
//          [](const std::string& data){ return Text(data); }, // success
//          [](const std::string& err){ return Text("Error: " + err); } // error
//      )
// ============================================================
enum class QueryStatus { Idle, Loading, Success, Error };

template <typename T>
class Query {
    std::function<T()> fetcher_;
    std::shared_ptr<std::atomic<int>> status_ = std::make_shared<std::atomic<int>>((int)QueryStatus::Idle);
    std::shared_ptr<T> data_ = std::make_shared<T>();
    std::shared_ptr<std::string> error_ = std::make_shared<std::string>();
public:
    explicit Query(std::function<T()> fetcher) : fetcher_(std::move(fetcher)) {}

    QueryStatus status() const { return (QueryStatus)status_->load(); }
    bool isLoading() const { return status() == QueryStatus::Loading; }
    bool isSuccess() const { return status() == QueryStatus::Success; }
    bool isError() const { return status() == QueryStatus::Error; }
    const T& data() const { return *data_; }
    const std::string& error() const { return *error_; }

    // Kick off the fetch on a background thread (idempotent while loading).
    void start() {
        if (status() == QueryStatus::Loading) return;
        status_->store((int)QueryStatus::Loading);
        if (auto* app = FluxUI::Application::instance()) app->requestRedraw();
        auto status = status_; auto data = data_; auto error = error_;
        auto fetcher = fetcher_;
        std::thread([status, data, error, fetcher]() {
            try {
                T result = fetcher();
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, data, result]() {
                        *data = result;
                        status->store((int)QueryStatus::Success);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            } catch (const std::exception& e) {
                std::string msg = e.what();
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, error, msg]() {
                        *error = msg;
                        status->store((int)QueryStatus::Error);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            } catch (...) {
                if (auto* app = FluxUI::Application::instance()) {
                    app->runOnMainThread([status, error]() {
                        *error = "unknown error";
                        status->store((int)QueryStatus::Error);
                        if (auto* a = FluxUI::Application::instance()) a->requestRedraw();
                    });
                }
            }
        }).detach();
    }

    void refetch() { status_->store((int)QueryStatus::Idle); start(); }

    // Render the right Element for the current state. Loading state defaults to
    // a skeleton if no loading builder is provided.
    Element view(std::function<Element()> onLoading,
                 std::function<Element(const T&)> onSuccess,
                 std::function<Element(const std::string&)> onError = nullptr) const {
        switch (status()) {
            case QueryStatus::Success: return onSuccess(*data_);
            case QueryStatus::Error:   return onError ? onError(*error_)
                                                      : detail::leaf("p", "Error: " + *error_);
            default:                   return onLoading ? onLoading()
                                                        : detail::leaf("div", "");
        }
    }
};

// ============================================================
//  Skeleton — easy loading placeholders (shimmer lines / blocks)
// ============================================================
inline Element Skeleton(int lines = 3) {
    Element e("div");
    e.className_ = "skeleton";
    for (int i = 0; i < lines; ++i) {
        e.children_.push_back(detail::tagOnly("div").className("skeleton-line"));
    }
    return e;
}
inline Element SkeletonBox(const std::string& w = "100%", const std::string& h = "120px") {
    return detail::tagOnly("div").className("skeleton skeleton-box").style("width", w).style("height", h);
}

} // namespace fluxui