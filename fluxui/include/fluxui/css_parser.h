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
};

struct CSSRule {
    std::string selector; // e.g. ".sidebar", "#dashboard", "button"
    std::vector<CSSProperty> properties;
};

// ============================================================
//  CSS Stylesheet
// ============================================================

class StyleSheet {
public:
    std::vector<CSSRule> rules;

    // Load and parse a CSS file
    bool loadFile(const std::string& path);

    // Parse CSS string
    void parse(const std::string& css);

    // Get style for a specific class/id/type
    Style resolve(const std::string& className,
                  const std::string& id = "",
                  const std::string& type = "") const;

    // Merge a resolved style onto a base style
    static void mergeProperty(Style& style, const std::string& name, const std::string& value);

private:
    std::unordered_map<std::string, std::string> variables_;
    mutable std::unordered_map<std::string, Style> resolvedCache_;

    void parseRule(const std::string& selector, const std::string& body);
    std::string resolveValue(const std::string& value) const;
    static std::string cacheKey(const std::string& className,
                                const std::string& id,
                                const std::string& type);
    static std::string trim(const std::string& s);
    static std::vector<std::string> splitTopLevel(const std::string& value, char delimiter);
    static bool selectorMatches(const std::string& selector,
                                const std::string& className,
                                const std::string& id,
                                const std::string& type,
                                std::string* pseudo = nullptr);
    static void mergeHoverProperty(Style& style, const std::string& name, const std::string& value);
    static void mergeFocusProperty(Style& style, const std::string& name, const std::string& value);
    static void mergeActiveProperty(Style& style, const std::string& name, const std::string& value);
    static Color parseColor(const std::string& val);
    static CSSValue parseCSSValue(const std::string& val);
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
