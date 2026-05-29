// FluxUI - GPU-Accelerated UI Framework with CSS Styling
// Copyright (c) 2026 - MIT License
#pragma once
#include "fluxui/core.h"
#include <vector>
#include <unordered_map>

namespace FluxUI {

struct TransformNode {
    int id = 0;
    int parentId = -1;
    float scale = 1.0f;
    Vec2 translation = {0, 0};
    Vec2 pivot = {0, 0};
    bool isAnimated = false;

    // Combined/global transformations cached on node update
    float combinedScale = 1.0f;
    Vec2 combinedTranslation = {0, 0};
};

struct ClipNode {
    int id = 0;
    int parentId = -1;
    Rect clipRect;
    int transformNodeId = 0; // Target transform node applied to clip coordinate bounds

    // Combined/global clip rect cached on tree build (intersection of all ancestors)
    Rect combinedClipRect;
    bool hasCombinedClip = false;
};

struct EffectNode {
    int id = 0;
    int parentId = -1;
    float opacity = 1.0f;
    float blurRadius = 0.0f;
    int transformNodeId = 0;

    // Combined/global opacity cached on tree build (product of all ancestor opacities)
    float combinedOpacity = 1.0f;
};

class PropertyTrees {
private:
    std::vector<TransformNode> transformTree_;
    std::vector<ClipNode> clipTree_;
    std::vector<EffectNode> effectTree_;

public:
    PropertyTrees() {
        clear();
    }

    void clear() {
        transformTree_.clear();
        clipTree_.clear();
        effectTree_.clear();

        // Seed root elements as base defaults
        TransformNode rootTransform;
        rootTransform.id = 0;
        rootTransform.parentId = -1;
        rootTransform.scale = 1.0f;
        rootTransform.translation = {0, 0};
        rootTransform.combinedScale = 1.0f;
        rootTransform.combinedTranslation = {0, 0};
        transformTree_.push_back(rootTransform);

        ClipNode rootClip;
        rootClip.id = 0;
        rootClip.parentId = -1;
        rootClip.hasCombinedClip = false;
        clipTree_.push_back(rootClip);

        EffectNode rootEffect;
        rootEffect.id = 0;
        rootEffect.parentId = -1;
        rootEffect.combinedOpacity = 1.0f;
        effectTree_.push_back(rootEffect);
    }

    int insertTransformNode(int parentId, float scale, const Vec2& translation, const Vec2& pivot, bool isAnimated = false) {
        TransformNode node;
        node.id = static_cast<int>(transformTree_.size());
        node.parentId = parentId;
        node.scale = scale;
        node.translation = translation;
        node.pivot = pivot;
        node.isAnimated = isAnimated;

        updateCombinedTransform(node);
        transformTree_.push_back(node);
        return node.id;
    }

    int insertClipNode(int parentId, const Rect& clipRect, int transformNodeId) {
        ClipNode node;
        node.id = static_cast<int>(clipTree_.size());
        node.parentId = parentId;
        node.clipRect = clipRect;
        node.transformNodeId = transformNodeId;

        updateCombinedClip(node);
        clipTree_.push_back(node);
        return node.id;
    }

    int insertEffectNode(int parentId, float opacity, float blurRadius, int transformNodeId) {
        EffectNode node;
        node.id = static_cast<int>(effectTree_.size());
        node.parentId = parentId;
        node.opacity = opacity;
        node.blurRadius = blurRadius;
        node.transformNodeId = transformNodeId;

        updateCombinedEffect(node);
        effectTree_.push_back(node);
        return node.id;
    }

    // Dynamic effect update from compositor thread (opacity animations at 120 FPS)
    void updateEffectNode(int id, float opacity) {
        if (id >= 0 && id < static_cast<int>(effectTree_.size())) {
            effectTree_[id].opacity = opacity;
            updateCombinedEffect(effectTree_[id]);

            // Propagate to direct descendants
            for (auto& node : effectTree_) {
                if (node.parentId == id) {
                    updateCombinedEffect(node);
                }
            }
        }
    }

    void updateTransformNode(int id, float scale, const Vec2& translation) {
        if (id >= 0 && id < static_cast<int>(transformTree_.size())) {
            transformTree_[id].scale = scale;
            transformTree_[id].translation = translation;
            updateCombinedTransform(transformTree_[id]);

            // Propagate updates to direct descendants
            for (auto& node : transformTree_) {
                if (node.parentId == id) {
                    updateCombinedTransform(node);
                }
            }
        }
    }

    const TransformNode& getTransformNode(int id) const {
        if (id >= 0 && id < static_cast<int>(transformTree_.size())) {
            return transformTree_[id];
        }
        return transformTree_[0];
    }

    const ClipNode& getClipNode(int id) const {
        if (id >= 0 && id < static_cast<int>(clipTree_.size())) {
            return clipTree_[id];
        }
        return clipTree_[0];
    }

    const EffectNode& getEffectNode(int id) const {
        if (id >= 0 && id < static_cast<int>(effectTree_.size())) {
            return effectTree_[id];
        }
        return effectTree_[0];
    }

    const std::vector<TransformNode>& getTransformTree() const { return transformTree_; }
    const std::vector<ClipNode>& getClipTree() const { return clipTree_; }
    const std::vector<EffectNode>& getEffectTree() const { return effectTree_; }

    // Resolve combined clip rect by walking up the clip tree and intersecting
    Rect getCombinedClipRect(int clipNodeId) const {
        const auto& node = getClipNode(clipNodeId);
        if (!node.hasCombinedClip) {
            return node.clipRect;
        }
        return node.combinedClipRect;
    }

    // Resolve combined opacity by walking up the effect tree
    float getCombinedOpacity(int effectNodeId) const {
        const auto& node = getEffectNode(effectNodeId);
        return node.combinedOpacity;
    }

private:
    void updateCombinedTransform(TransformNode& node) {
        if (node.parentId == -1 || node.parentId == 0) {
            node.combinedScale = node.scale;
            node.combinedTranslation = node.translation;
        } else {
            const auto& parent = transformTree_[node.parentId];
            node.combinedScale = parent.combinedScale * node.scale;
            node.combinedTranslation = parent.combinedTranslation + node.translation;
        }
    }

    void updateCombinedClip(ClipNode& node) {
        if (node.parentId <= 0) {
            // Root or direct child of root: clip is just this node's rect
            node.combinedClipRect = node.clipRect;
            node.hasCombinedClip = (node.clipRect.w > 0 && node.clipRect.h > 0);
        } else {
            const auto& parent = clipTree_[node.parentId];
            if (parent.hasCombinedClip) {
                // Intersect this clip with the parent's combined clip
                node.combinedClipRect = rectIntersect(node.clipRect, parent.combinedClipRect);
            } else {
                node.combinedClipRect = node.clipRect;
            }
            node.hasCombinedClip = (node.combinedClipRect.w > 0 && node.combinedClipRect.h > 0);
        }
    }

    void updateCombinedEffect(EffectNode& node) {
        if (node.parentId == -1 || node.parentId == 0) {
            node.combinedOpacity = node.opacity;
        } else {
            const auto& parent = effectTree_[node.parentId];
            node.combinedOpacity = parent.combinedOpacity * node.opacity;
        }
    }

    // Axis-aligned rect intersection helper
    static Rect rectIntersect(const Rect& a, const Rect& b) {
        float x1 = std::max(a.x, b.x);
        float y1 = std::max(a.y, b.y);
        float x2 = std::min(a.x + a.w, b.x + b.w);
        float y2 = std::min(a.y + a.h, b.y + b.h);
        if (x2 <= x1 || y2 <= y1) {
            return Rect{0, 0, 0, 0};
        }
        return Rect{x1, y1, x2 - x1, y2 - y1};
    }
};

extern PropertyTrees g_activePropertyTrees;

} // namespace FluxUI
