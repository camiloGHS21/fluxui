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

    // Dynamic effect adjustment directly on the compositor (for thread-safe 120 FPS opacity animations)
    void updateEffectNode(int nodeId, float opacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRecord_.getPropertyTrees().updateEffectNode(nodeId, opacity);
        activeRecord_.getPropertyTrees().updateEffectNode(nodeId, opacity);
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

        // 2. Resolve clip hierarchy recursively from the Clip Tree (cc::ClipTree parity)
        //    Walk up the clip-node chain and intersect all ancestor clip rects into one combined clip.
        bool hasClip = false;
        if (cmd.clipNodeId > 0) {
            const auto& clipNode = trees.getClipNode(cmd.clipNodeId);
            if (clipNode.hasCombinedClip) {
                Rect combinedClip = trees.getCombinedClipRect(cmd.clipNodeId);
                if (combinedClip.w > 0 && combinedClip.h > 0) {
                    renderer_->pushScissor(combinedClip);
                    hasClip = true;
                }
            }
        }

        // 3. Resolve cumulative opacity from the Effect Tree (cc::EffectTree parity)
        //    Walks up the effect-node parent chain, multiplying opacities at each level.
        float activeOpacity = cmd.opacity * trees.getCombinedOpacity(cmd.effectNodeId);

        // 4. Playback batched draw operations
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
                // Explicit scissor commands still work for inline pushScissor() calls
                renderer_->pushScissor(cmd.scissorRect);
                break;
            case RenderCommandType::ScissorPop:
                renderer_->popScissor();
                break;
            // ── Compositing operations (Blink cc::PaintOp parity) ──
            case RenderCommandType::BackdropFilterBlur:
                renderer_->drawBackdropFilterBlur(cmd.rect, cmd.blurRadius, cmd.radius);
                break;
            case RenderCommandType::FilterEffect:
                // Filter effects are applied during SaveLayer/RestoreLayer compositing.
                // Individual filter commands serve as markers for the effect tree.
                break;
            case RenderCommandType::SaveLayer:
                // GPU compositing: push an offscreen render target for filter/blend isolation.
                // In software mode, this is a conceptual layer that gets composited on restore.
                // The actual blend/filter is applied during RestoreLayer.
                renderer_->pushRenderTarget(0, (int)cmd.rect.w, (int)cmd.rect.h);
                break;
            case RenderCommandType::RestoreLayer:
                // Pop the render target and composite back with blend mode and filters.
                renderer_->popRenderTarget();
                break;
            case RenderCommandType::BlendModeBegin:
            case RenderCommandType::IsolationBegin:
                // Markers for begin/end pairs — actual compositing handled by Save/RestoreLayer
                break;
            case RenderCommandType::BlendModeEnd:
            case RenderCommandType::IsolationEnd:
                break;
            case RenderCommandType::Border:
                renderer_->drawBorder(cmd.rect, cmd.border, cmd.radius);
                break;
            case RenderCommandType::BoxShadow:
                renderer_->drawBoxShadow(cmd.rect, cmd.shadow, cmd.radius);
                break;
        }

        // 5. Restore clip/scale/translation stacks for clean rendering cycles
        if (hasClip) {
            renderer_->popScissor();
        }
        if (hasScale) {
            renderer_->popScale();
        }
        if (hasTranslation) {
            renderer_->popTranslation();
        }
    }
};

} // namespace FluxUI
