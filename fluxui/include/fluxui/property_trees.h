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
};

struct EffectNode {
    int id = 0;
    int parentId = -1;
    float opacity = 1.0f;
    float blurRadius = 0.0f;
    int transformNodeId = 0;
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
        clipTree_.push_back(rootClip);

        EffectNode rootEffect;
        rootEffect.id = 0;
        rootEffect.parentId = -1;
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

        effectTree_.push_back(node);
        return node.id;
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
};

} // namespace FluxUI
