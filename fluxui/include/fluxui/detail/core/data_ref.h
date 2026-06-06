#pragma once
// FluxUI public API - DataRef<T>: copy-on-write shared handle.
//
// Mirrors Blink's DataRef<T> (third_party/blink/renderer/core/style/data_ref.h):
// ComputedStyle is partitioned into groups (StyleBoxData, StyleRareNonInheritedData,
// ...) each held behind a DataRef so that copying a ComputedStyle only bumps the
// refcount of unchanged groups instead of deep-copying every field. A group is
// cloned lazily the first time it is mutated through a non-const access (access()),
// which is the copy-on-write step.
//
// FluxUI uses this for the "rare" (cold) half of Style: masking, scroll, advanced
// typography, UI-misc and animation/transition lists. The hot fields stay inline
// in Style, so copying a Style (e.g. ComputedStyle::ensureMutable, sibling style
// sharing) copies the hot fields by value and shares the cold group by pointer.
//
// Auto-split from core.h; do not include directly, use <fluxui/core.h>.
#include "fluxui/config.h"
#include <memory>
#include <utility>

namespace FluxUI {

template <typename T>
class DataRef {
public:
    DataRef() : data_(std::make_shared<const T>()) {}

    // Read access: never copies. Shared between all owners until a write happens.
    const T* operator->() const { return data_.get(); }
    const T& operator*() const { return *data_; }
    const T* get() const { return data_.get(); }

    // Write access (copy-on-write): clones the group if it is currently shared,
    // so the caller gets a uniquely-owned, mutable instance. Matches
    // Blink DataRef<T>::Access().
    T* access() {
        if (data_.use_count() != 1) {
            data_ = std::make_shared<T>(*data_);
        }
        // Safe: after the use_count()==1 guarantee we are the sole owner.
        return const_cast<T*>(data_.get());
    }

    // Value-equality: identical pointer is trivially equal; otherwise deep compare.
    bool operator==(const DataRef& other) const {
        return data_ == other.data_ || *data_ == *other.data_;
    }
    bool operator!=(const DataRef& other) const { return !(*this == other); }

private:
    std::shared_ptr<const T> data_;
};

} // namespace FluxUI
