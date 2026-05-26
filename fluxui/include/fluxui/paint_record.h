// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// Copyright (c) 2026 - MIT License
#pragma once
#include "fluxui/renderer.h"
#include "fluxui/property_trees.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace FluxUI {

// PaintRecord behaves exactly like Blink's cc::PaintRecord / DisplayList,
// containing a list of recorded RenderCommands and its associated PropertyTrees.
class PaintRecord {
private:
    std::vector<RenderCommand> commands_;
    PropertyTrees propertyTrees_;

public:
    PaintRecord() = default;
    ~PaintRecord() = default;

    void addCommand(const RenderCommand& cmd) {
        commands_.push_back(cmd);
    }

    void clear() {
        commands_.clear();
        propertyTrees_.clear();
    }

    const std::vector<RenderCommand>& getCommands() const {
        return commands_;
    }

    std::vector<RenderCommand>& getCommands() {
        return commands_;
    }

    PropertyTrees& getPropertyTrees() {
        return propertyTrees_;
    }

    const PropertyTrees& getPropertyTrees() const {
        return propertyTrees_;
    }

    bool empty() const {
        return commands_.empty();
    }

    void swap(PaintRecord& other) {
        commands_.swap(other.commands_);
        std::swap(propertyTrees_, other.propertyTrees_);
    }
};

// ThreadedCompositor runs a dedicated compositor thread that executes
// OpenGL/Vulkan GPU drawing calls from committed PaintRecords,
// dynamically resolving transforms from its PropertyTrees.
class ThreadedCompositor {
private:
    std::thread compositorThread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;

    // Double buffering for thread-safe display list commits
    PaintRecord activeRecord_;
    PaintRecord pendingRecord_;
    bool hasNewRecord_ = false;

    Renderer* renderer_ = nullptr;
    void* windowHandle_ = nullptr;

public:
    ThreadedCompositor() = default;
    ~ThreadedCompositor() {
        stop();
    }

    static ThreadedCompositor& instance() {
        static ThreadedCompositor inst;
        return inst;
    }

    void init(Renderer* renderer, void* windowHandle) {
        renderer_ = renderer;
        windowHandle_ = windowHandle;
    }

    void start() {
        if (running_.exchange(true)) return;
        compositorThread_ = std::thread(&ThreadedCompositor::compositorLoop, this);
    }

    void stop() {
        if (!running_.exchange(false)) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cv_.notify_one();
        }
        if (compositorThread_.joinable()) {
            compositorThread_.join();
        }
    }

    // Main thread commits a completed frame recording and its property trees to the compositor
    void commitFrame(PaintRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRecord_.clear();
        pendingRecord_.swap(record); // O(1) swap of commands and property trees
        hasNewRecord_ = true;
        cv_.notify_one();
    }

    // Dynamic transform adjustment directly on the compositor (for thread-safe 120 FPS scrolling/animations)
    void updateTransformNode(int nodeId, float scale, const Vec2& translation) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRecord_.getPropertyTrees().updateTransformNode(nodeId, scale, translation);
        activeRecord_.getPropertyTrees().updateTransformNode(nodeId, scale, translation);
    }

private:
    void compositorLoop() {
        while (running_) {
            PaintRecord currentFrame;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !running_ || hasNewRecord_; });

                if (!running_) break;

                // Swap pending frame to active display list
                activeRecord_.swap(pendingRecord_);
                hasNewRecord_ = false;

                // Make a thread-local copy or swap so we release the mutex instantly
                currentFrame.swap(activeRecord_);
            }

            if (renderer_ && !currentFrame.empty()) {
                int w = 0, h = 0;
                Vec2 size = renderer_->getWindowSize();
                w = static_cast<int>(size.x);
                h = static_cast<int>(size.y);

                renderer_->beginFrame(w, h);

                // Playback recorded PaintRecord while mapping to dynamic PropertyTrees
                for (const auto& cmd : currentFrame.getCommands()) {
                    playbackCommand(cmd, currentFrame.getPropertyTrees());
                }

                renderer_->endFrame();
            }

            // Target ~120 FPS presentation loop
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }

    void playbackCommand(const RenderCommand& cmd, const PropertyTrees& trees) {
        if (!renderer_) return;

        // 1. Resolve combined transforms recursively from the Transform Tree
        const auto& transNode = trees.getTransformNode(cmd.transformNodeId);
        
        bool hasScale = (transNode.combinedScale != 1.0f);
        bool hasTranslation = (transNode.combinedTranslation.x != 0.0f || transNode.combinedTranslation.y != 0.0f);

        if (hasTranslation) {
            renderer_->pushTranslation(transNode.combinedTranslation);
        }
        if (hasScale) {
            renderer_->pushScale(transNode.combinedScale, transNode.pivot);
        }

        // 2. Resolve visual effect properties
        const auto& effectNode = trees.getEffectNode(cmd.effectNodeId);
        float activeOpacity = cmd.opacity * effectNode.opacity;

        // 3. Playback batched operations
        switch (cmd.type) {
            case RenderCommandType::RoundedRect:
                if (cmd.hasGradient) {
                    renderer_->drawRoundedRectGradient(cmd.rect, cmd.gradient, cmd.radius, activeOpacity);
                } else {
                    renderer_->drawRoundedRect(cmd.rect, cmd.color, cmd.radius, activeOpacity);
                }
                if (cmd.hasBorder) {
                    renderer_->drawBorder(cmd.rect, cmd.border, cmd.radius);
                }
                break;
            case RenderCommandType::Text:
                renderer_->drawText(cmd.text, cmd.rect.position(), cmd.color, cmd.fontSize, cmd.fontWeight);
                break;
            case RenderCommandType::TexturedQuad:
                renderer_->drawImage("", cmd.rect, activeOpacity, cmd.color);
                break;
            case RenderCommandType::Scissor:
                // Scissor rectangles can optionally be mapped with the current transform pivot offsets
                renderer_->pushScissor(cmd.scissorRect);
                break;
            case RenderCommandType::ScissorPop:
                renderer_->popScissor();
                break;
        }

        // 4. Restore scale/translation stacks for clean rendering cycles
        if (hasScale) {
            renderer_->popScale();
        }
        if (hasTranslation) {
            renderer_->popTranslation();
        }
    }
};

} // namespace FluxUI
