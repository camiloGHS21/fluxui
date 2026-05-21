// FluxUI CSS Parser - Parses CSS files and applies styles to widgets
#pragma once

#include "core.h"
#include <string>
#include <vector>
#include <unordered_map>
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

struct CSSRule {
    std::string selector; // e.g. ".sidebar", "#dashboard", "button"
    std::vector<CSSProperty> properties;
    std::string mediaQuery;
    int specificity = 0;
};

struct CSSSelectorNode {
    std::string className;
    std::string id;
    std::string type;
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

// ============================================================
//  CSS Stylesheet
// ============================================================

class StyleSheet {
public:
    StyleSheet();

    std::vector<CSSRule> rules;

    // Load and parse a CSS file
    bool loadFile(const std::string& path);

    // Parse CSS string
    void parse(const std::string& css);
    bool setViewportSize(float width, float height);

    // Get style for a specific class/id/type
    Style resolve(const std::string& className,
                  const std::string& id = "",
                  const std::string& type = "") const;
    Style resolve(const std::string& className,
                  const std::string& id,
                  const std::string& type,
                  const std::vector<CSSSelectorNode>& ancestors) const;
    Style resolve(const std::string& className,
                  const std::string& id,
                  const std::string& type,
                  const std::vector<CSSSelectorNode>& ancestors,
                  const Style* parentStyle) const;
    std::string resolveValue(const std::string& value,
                             const std::unordered_map<std::string, std::string>& customProperties,
                             bool* valid = nullptr) const;

    // Merge a resolved style onto a base style
    static void mergeProperty(Style& style, const std::string& name, const std::string& value);

private:
    std::unordered_map<std::string, std::string> variables_;
    mutable std::unordered_map<std::string, Style> resolvedCache_;
    std::unordered_map<std::string, std::vector<size_t>> idRuleIndex_;
    std::unordered_map<std::string, std::vector<size_t>> classRuleIndex_;
    std::unordered_map<std::string, std::vector<size_t>> typeRuleIndex_;
    std::vector<size_t> universalRuleIndex_;
    float viewportWidth_ = 1920.0f;
    float viewportHeight_ = 1080.0f;
    uint32_t nextPropertyOrder_ = 0;

    void parseRules(const std::string& css, const std::string& mediaQuery);
    void parseRule(const std::string& selector, const std::string& body, const std::string& mediaQuery = "");
    void indexRule(size_t ruleIndex);
    std::string resolveValueInternal(const std::string& value,
                                     const std::unordered_map<std::string, std::string>* customProperties,
                                     bool* valid = nullptr,
                                     int depth = 0) const;
    static std::string cacheKey(const std::string& className,
                                const std::string& id,
                                const std::string& type,
                                const std::vector<CSSSelectorNode>& ancestors = {});
    static std::string trim(const std::string& s);
    static std::vector<std::string> splitTopLevel(const std::string& value, char delimiter);
    static bool selectorMatches(const std::string& selector,
                                const std::string& className,
                                const std::string& id,
                                const std::string& type,
                                std::string* pseudo = nullptr);
    static bool selectorMatches(const std::string& selector,
                                const std::string& className,
                                const std::string& id,
                                const std::string& type,
                                const std::vector<CSSSelectorNode>& ancestors,
                                std::string* pseudo = nullptr);
    static int selectorSpecificity(const std::string& selector);
    static CSSRuleIndexKey selectorIndexKey(const std::string& selector);
    static void appendClassTokens(const std::string& className, std::vector<std::string>& out);
    static std::vector<std::string> splitDeclarations(const std::string& body);
    static bool stripImportant(std::string& value);
    static void applyUserAgentDefaults(Style& style,
                                       const std::string& type,
                                       const std::vector<CSSSelectorNode>& ancestors);
    void collectCandidateRules(const std::string& className,
                               const std::string& id,
                               const std::string& type,
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
    static EdgeInsets parseEdgeInsets(const std::string& val);
    static BorderRadius parseBorderRadius(const std::string& val);
    static Border parseBorder(const std::string& val);
    static BoxShadow parseBoxShadow(const std::string& val);
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
