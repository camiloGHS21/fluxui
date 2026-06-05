// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// Copyright (c) 2026 - MIT License
#include "fluxui/gc.h"

namespace FluxUI {

std::vector<GarbageCollectedBase*> GCHeap::allocations_;
std::unordered_set<GarbageCollectedBase**> GCHeap::roots_;
bool GCHeap::isCollecting_ = false;

void GCHeap::collectGarbage() {
    if (isCollecting_) return;
    isCollecting_ = true;

    // 1. Reset mark flags of all active allocations
    for (auto* obj : allocations_) {
        if (obj) {
            obj->gcMarked_ = false;
        }
    }

    // 2. Mark phase: Trace from all registered persistent roots
    GCVisitor visitor;
    for (auto** rootPtr : roots_) {
        if (rootPtr && *rootPtr) {
            visitor.trace(*rootPtr);
        }
    }

    // Drain the worklist recursively to complete the graph marking pass
    visitor.drain();

    // 3. Sweep phase: Free memory for any unmarked objects
    std::vector<GarbageCollectedBase*> survivors;
    survivors.reserve(allocations_.size());

    size_t sweptCount = 0;
    for (auto* obj : allocations_) {
        if (obj) {
            if (obj->gcMarked_) {
                survivors.push_back(obj);
            } else {
                delete obj;
                sweptCount++;
            }
        }
    }

    allocations_ = std::move(survivors);
    isCollecting_ = false;
}

void GCHeap::cleanupHeap() {
    for (auto* obj : allocations_) {
        delete obj;
    }
    allocations_.clear();
    roots_.clear();
}

} // namespace FluxUI
