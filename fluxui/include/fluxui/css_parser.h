// FluxUI CSS Parser - Parses CSS files and applies styles to widgets
#pragma once

#include "core.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <fstream>

namespace FluxUI {

// ============================================================
//  CSS Rule: selector -> properties
// ============================================================

struct CSSProperty {
    std::string name;
    std::string value;
    uint32_t sourceOrder = 0;
};

struct CSSFontFace {
    std::string fontFamily;
    std::string src;
};

struct CSSRule {
    std::string selector; // e.g. ".sidebar", "#dashboard", "button"
    std::vector<CSSProperty> properties;
    std::string mediaQuery;
    std::string layer;
    int specificity = 0;

    // Cached pre-parsed selector data for faster matching
    std::string selectorWithoutPseudo;
    std::string pseudoState;
    std::vector<std::string> parts;
    std::vector<char> combinators;
};

class Widget;

struct CSSSelectorNode {
    std::string_view className;
    std::string_view id;
    std::string_view type;
    const Widget* widget = nullptr;
};

enum class CSSRuleBucket {
    Universal,
    Id,
    Class,
    Type
};

struct CSSRuleIndexKey {
    CSSRuleBucket bucket = CSSRuleBucket::Universal;
    std::string key;
};

struct StyleCacheKey {
    uint64_t h1 = 0;
    uint64_t h2 = 0;

    bool operator==(const StyleCacheKey& other) const {
        return h1 == other.h1 && h2 == other.h2;
    }
};

struct StyleCacheKeyHash {
    size_t operator()(const StyleCacheKey& k) const {
        return static_cast<size_t>(k.h1 ^ k.h2);
    }
};

// ============================================================
//  CSS Stylesheet
// ============================================================

class StyleSheet {
public:
    StyleSheet();

    std::vector<CSSRule> rules;
    std::vector<CSSFontFace> fontFaces;
    std::vector<std::string> layersOrder;

    // Load and parse a CSS file
    bool loadFile(const std::string& path);

    // Parse CSS string
    void parse(const std::string& css);
    bool setViewportSize(float width, float height);

    // Get style for a specific class/id/type
    Style resolve(std::string_view className,
                  std::string_view id = "",
                  std::string_view type = "") const;
    Style resolve(std::string_view className,
                  std::string_view id,
                  std::string_view type,
                  const std::vector<CSSSelectorNode>& ancestors) const;
    Style resolve(std::string_view className,
                  std::string_view id,
                  std::string_view type,
                  const std::vector<CSSSelectorNode>& ancestors,
                  const Style* parentStyle,
                  const Widget* widget = nullptr,
                  std::string_view targetPseudo = "") const;
    template <typename F>
    Style resolveLazy(std::string_view className,
                      std::string_view id,
                      std::string_view type,
                      uint64_t ancestorH1,
                      uint64_t ancestorH2,
                      const Style* parentStyle,
                      F&& getAncestors,
                      const Widget* widget = nullptr) const {
        uint64_t h1 = ancestorH1;
        uint64_t h2 = ancestorH2;

        auto hashStr = [&](std::string_view sv) {
            for (char c : sv) {
                h1 ^= static_cast<uint64_t>(c);
                h1 *= 1099511628211ULL;
            }
            for (char c : sv) {
                h2 = ((h2 << 5) + h2) + static_cast<uint64_t>(c);
            }
        };

        hashStr(className);
        h1 ^= 0xFFULL; h2 ^= 0xFFULL;
        hashStr(id);
        h1 ^= 0xEEULL; h2 ^= 0xEEULL;
        hashStr(type);

        if (parentStyle) {
            h1 ^= parentStyle->inheritedHash;
            h2 ^= ~parentStyle->inheritedHash;
        }

        StyleCacheKey key{h1, h2};

#if FLUXUI_STYLE_CACHE_SIZE > 0
        size_t cacheIdx = (key.h1 ^ key.h2) % FLUXUI_STYLE_CACHE_SIZE;
        if (resolvedCache_[cacheIdx].epoch == currentEpoch_ && resolvedCache_[cacheIdx].key == key) {
            return resolvedCache_[cacheIdx].style;
        }
#endif

        const auto& ancestors = getAncestors();
        return resolve(className, id, type, ancestors, parentStyle, widget);
    }
    std::string resolveValue(const std::string& value,
                             const std::unordered_map<std::string, std::string>& customProperties,
                             bool* valid = nullptr) const;

    // Merge a resolved style onto a base style
    static uint64_t computeInheritedHash(const Style& style);
    static void mergeProperty(Style& style, const std::string& name, const std::string& value, float emBase = 16.0f);
    static bool mergePropertyPart1(Style& style, const std::string& name, const std::string& value, float emBase = 16.0f);
    static void mergePropertyPart2(Style& style, const std::string& name, const std::string& value, float emBase = 16.0f);
    static std::string trim(const std::string& s);

    static bool selectorMatches(const CSSRule& rule,
                                std::string_view className,
                                std::string_view id,
                                std::string_view type,
                                const std::vector<CSSSelectorNode>& ancestors,
                                std::string_view* pseudo = nullptr,
                                const Widget* widget = nullptr);

    struct InvalidationSet {
        bool invalidateAllDescendants = false;
        bool invalidateAllSiblings = false;
        std::unordered_set<std::string> descendantClasses;
        std::unordered_set<std::string> descendantIds;
        std::unordered_set<std::string> descendantTypes;

        std::unordered_set<std::string> siblingClasses;
        std::unordered_set<std::string> siblingIds;
        std::unordered_set<std::string> siblingTypes;
    };

    const InvalidationSet* getClassInvalidationSet(const std::string& className) const;
    const InvalidationSet* getIdInvalidationSet(const std::string& id) const;
    void buildInvalidationSets();

private:
    std::unordered_map<std::string, InvalidationSet> classInvalidationSets_;
    std::unordered_map<std::string, InvalidationSet> idInvalidationSets_;
    std::unordered_map<std::string, InvalidationSet> typeInvalidationSets_;
    std::unordered_map<std::string, std::string> variables_;
    struct StyleCacheEntry {
        StyleCacheKey key;
        Style style;
        uint32_t epoch = 0;
    };
    mutable std::vector<StyleCacheEntry> resolvedCache_;
    mutable uint32_t currentEpoch_ = 1;
    std::unordered_map<std::string, std::vector<size_t>> idRuleIndex_;
    std::unordered_map<std::string, std::vector<size_t>> classRuleIndex_;
    std::unordered_map<std::string, std::vector<size_t>> typeRuleIndex_;
    std::vector<size_t> universalRuleIndex_;
    float viewportWidth_ = 1920.0f;
    float viewportHeight_ = 1080.0f;
    uint32_t nextPropertyOrder_ = 0;

    void registerLayer(const std::string& name);
    int getLayerPriority(const std::string& layerName) const;
    void parseRules(const std::string& css, const std::string& mediaQuery, const std::string& currentLayer = "");
    void parseRule(const std::string& selector, const std::string& body, const std::string& mediaQuery = "", const std::string& currentLayer = "");
    void parseFontFace(const std::string& body);
    void indexRule(size_t ruleIndex);
    std::string resolveValueInternal(const std::string& value,
                                     const std::unordered_map<std::string, std::string>* customProperties,
                                     bool* valid = nullptr,
                                     int depth = 0) const;
    static StyleCacheKey buildCacheKey(std::string_view className,
                                       std::string_view id,
                                       std::string_view type,
                                       const std::vector<CSSSelectorNode>& ancestors,
                                       const Style* parentStyle);
    static std::vector<std::string> splitTopLevel(const std::string& value, char delimiter);
    static int selectorSpecificity(const std::string& selector);
    static CSSRuleIndexKey selectorIndexKey(const std::string& selector);
    static void appendClassTokens(std::string_view className, std::vector<std::string_view>& out);
    static std::vector<std::string> splitDeclarations(const std::string& body);
    static bool stripImportant(std::string& value);
    static void applyUserAgentDefaults(Style& style,
                                       std::string_view type,
                                       const std::vector<CSSSelectorNode>& ancestors);
    void collectCandidateRules(std::string_view className,
                               std::string_view id,
                               std::string_view type,
                               std::vector<size_t>& out) const;
    bool mediaQueryMatches(const std::string& query) const;
    static void mergeHoverProperty(Style& style, const std::string& name, const std::string& value);
    static void mergeFocusProperty(Style& style, const std::string& name, const std::string& value);
    static void mergeActiveProperty(Style& style, const std::string& name, const std::string& value);
    static Color parseColor(const std::string& val);
    static CSSValue parseCSSValue(const std::string& val);
    static float parseLengthPixels(const std::string& val, float emBase = 16.0f);
    static float parseFontSizePixels(const std::string& val, float currentSize = 16.0f);
    static float parseLineHeight(const std::string& val, float fontSize);
    static EdgeInsets parseEdgeInsets(const std::string& val, float emBase = 16.0f);
    static BorderRadius parseBorderRadius(const std::string& val, float emBase = 16.0f);
    static Border parseBorder(const std::string& val, float emBase = 16.0f);
    static BoxShadow parseBoxShadow(const std::string& val, float emBase = 16.0f);
    static Gradient parseGradient(const std::string& val);
    static float parseFloat(const std::string& val);
    static float parseDuration(const std::string& val);
};

// ============================================================
//  CSS Variables / Theme
// ============================================================

class CSSVariables {
public:
    void set(const std::string& name, const std::string& value) {
        vars_[name] = value;
    }

    std::string get(const std::string& name) const {
        auto it = vars_.find(name);
        return (it != vars_.end()) ? it->second : "";
    }

    std::string resolve(const std::string& value) const {
        if (value.substr(0, 4) == "var(") {
            auto end = value.find(')');
            std::string varName = value.substr(4, end - 4);
            // Check for fallback
            auto comma = varName.find(',');
            if (comma != std::string::npos) {
                std::string name = trim(varName.substr(0, comma));
                std::string fallback = trim(varName.substr(comma + 1));
                auto it = vars_.find(name);
                return (it != vars_.end()) ? it->second : fallback;
            }
            auto it = vars_.find(varName);
            return (it != vars_.end()) ? it->second : value;
        }
        return value;
    }

private:
    std::unordered_map<std::string, std::string> vars_;

    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }
};

} // namespace FluxUI
