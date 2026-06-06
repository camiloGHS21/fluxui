// FluxUI - User-Agent stylesheet + UA default application.
// Extracted from css_parser.cpp: StyleSheet::getUaSheet (the built-in browser
// default stylesheet, Blink html.css parity) and applyUserAgentDefaults.
#include "fluxui/css_parser.h"
#include "fluxui/widgets.h"
#include "css_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace FluxUI {
using detail::trimLocal;
using detail::lowerAscii;

const StyleSheet& StyleSheet::getUaSheet() {
    static StyleSheet uaSheet;
    static bool initialized = false;
    if (!initialized) {
        uaSheet.parse(R"CSS(
            html { display: block; }
            body { display: block; margin: 8px; }
            p { display: block; margin-block-start: 1em; margin-block-end: 1em; }
            div, article, aside, footer, header, main, nav, section, address, fieldset, details, summary, dialog { display: block; }
            blockquote { display: block; margin-block-start: 1em; margin-block-end: 1em; margin-inline-start: 40px; margin-inline-end: 40px; }
            figure { display: block; margin-block-start: 1em; margin-block-end: 1em; margin-inline-start: 40px; margin-inline-end: 40px; }
            figcaption, dt, form, layer, hgroup, search, frameset, frame { display: block; }
            address { font-style: italic; }
            dl { display: block; margin-block-start: 1em; margin-block-end: 1em; }
            dd { display: block; margin-inline-start: 40px; }
            center { display: block; text-align: center; }
            hr { display: block; overflow: hidden; margin-block-start: 0.5em; margin-block-end: 0.5em; border: 1px solid rgb(128, 128, 128); }
            h1 { display: block; font-size: 2em; margin-block-start: 0.67em; margin-block-end: 0.67em; font-weight: bold; }
            h2 { display: block; font-size: 1.5em; margin-block-start: 0.83em; margin-block-end: 0.83em; font-weight: bold; }
            h3 { display: block; font-size: 1.17em; margin-block-start: 1em; margin-block-end: 1em; font-weight: bold; }
            h4 { display: block; font-size: 1em; margin-block-start: 1.33em; margin-block-end: 1.33em; font-weight: bold; }
            h5 { display: block; font-size: 0.83em; margin-block-start: 1.67em; margin-block-end: 1.67em; font-weight: bold; }
            h6 { display: block; font-size: 0.67em; margin-block-start: 2.33em; margin-block-end: 2.33em; font-weight: bold; }
            article h1, aside h1, nav h1, section h1 { font-size: 1.5em; margin-block-start: 0.83em; margin-block-end: 0.83em; }
            article article h1, article aside h1, article nav h1, article section h1,
            aside article h1, aside aside h1, aside nav h1, aside section h1,
            nav article h1, nav aside h1, nav nav h1, nav section h1,
            section article h1, section aside h1, section nav h1, section section h1 { font-size: 1.17em; margin-block-start: 1em; margin-block-end: 1em; }
            ul, ol, menu, dir { display: block; margin-block-start: 1em; margin-block-end: 1em; padding-inline-start: 40px; }
            ol { list-style-type: decimal; }
            ul { list-style-type: disc; }
            ul ul, ol ul { list-style-type: circle; }
            ul ul ul, ol ul ul, ul ol ul, ol ol ul { list-style-type: square; }
            li { display: list-item; }
            table { display: table; border: 0px solid rgb(128, 128, 128); }
            thead { display: table-header-group; vertical-align: middle; }
            tbody { display: table-row-group; vertical-align: middle; }
            tfoot { display: table-footer-group; vertical-align: middle; }
            tr { display: table-row; vertical-align: middle; }
            td, th { display: table-cell; vertical-align: middle; }
            th { font-weight: bold; text-align: center; }
            caption { display: table-caption; text-align: center; }
            col { display: table-column; }
            colgroup { display: table-column-group; }
            strong, b { display: inline; font-weight: bold; }
            a { display: inline; color: rgb(0, 0, 238); text-decoration: underline; cursor: pointer; }
            u, ins { display: inline; text-decoration: underline; }
            s, strike, del { display: inline; text-decoration: line-through; }
            mark { display: inline; background-color: rgb(255, 255, 0); color: rgb(0, 0, 0); }
            big { display: inline; font-size: 19.2px; }
            i, cite, em, var, dfn { display: inline; font-style: italic; }
            sub { display: inline; vertical-align: sub; font-size: 13.333px; }
            sup { display: inline; vertical-align: super; font-size: 13.333px; }
            small { display: inline; font-size: 13.333px; }
            pre, xmp, plaintext, listing { display: block; margin-block-start: 1em; margin-block-end: 1em; font-family: monospace; white-space: pre; }
            code, kbd, samp, tt { display: inline; font-family: monospace; }
            nobr { display: inline; white-space: nowrap; }
            img, svg, picture { display: inline-block; object-fit: fill; }
            canvas { display: inline-block; width: 300px; height: 150px; }
            video { display: inline-block; object-fit: contain; }
            rp, noframes { display: none; }
            span, q, map, area, abbr, acronym, bdi, bdo, data, time, output, rb, rtc, ruby, audio, embed, iframe, object { display: inline; }
            iframe { border: 2px solid rgb(118, 118, 118); }
            bdi, output { unicode-bidi: isolate; }
            bdo { unicode-bidi: bidi-override; }
            slot { display: contents; }
            rt { display: block; font-size: 8px; }
            label { display: inline; cursor: default; }
            fieldset { display: block; margin: 0px 2px; padding: 5.6px 12px 10px 12px; border: 2px solid rgb(192, 192, 192); min-width: 0px; }
            legend { display: block; padding: 0px 2px; }
            input[type="hidden"] {
                display: none !important;
                appearance: auto;
                cursor: default;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                width: 0px;
                height: 0px;
            }
            input[type="file"] {
                display: inline-block;
                appearance: auto;
                cursor: default;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                width: 253px;
                height: 21px;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="image"] {
                display: inline-block;
                appearance: auto;
                cursor: pointer;
                padding: 0px;
                border: 0px solid rgba(0, 0, 0, 0);
                background-color: rgba(0, 0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="color"] {
                display: inline-block;
                appearance: square-button;
                cursor: default;
                width: 44px;
                height: 23px;
                padding: 1px 2px;
                border: 1px solid rgb(169, 169, 169);
                background-color: rgb(239, 239, 239);
                box-sizing: border-box;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="text"], input[type="password"], input[type="search"], input[type="number"], input[type="email"], input[type="url"], input[type="tel"], input[type="date"], input[type="time"], input[type="month"], input[type="week"], input[type="datetime-local"] {
                display: inline-block;
                appearance: textfield;
                cursor: text;
                padding: 1px 2px;
                border: 2px solid rgb(118, 118, 118);
                border-radius: 2px;
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                width: 170px;
                box-sizing: border-box;
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="search"] {
                appearance: searchfield;
            }
            textarea {
                display: inline-block;
                appearance: auto;
                cursor: text;
                overflow: auto;
                white-space: pre-wrap;
                word-break: break-word;
                font-family: monospace;
                padding: 2px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            select {
                display: inline-block;
                appearance: menulist;
                cursor: default;
                padding: 1px 22px 1px 4px;
                border: 1px solid rgb(0, 0, 0);
                border-radius: 5px;
                background-color: rgb(255, 255, 255);
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            option {
                display: none;
                padding: 0px 2px 1px 2px;
                white-space: pre;
                color: rgb(0, 0, 0);
                margin: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="checkbox"] {
                display: inline-block;
                appearance: checkbox;
                cursor: default;
                margin: 3px 4px 3px 4px;
                width: 13px;
                height: 13px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                border-radius: 0px;
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="radio"] {
                display: inline-block;
                appearance: radio;
                cursor: default;
                margin: 3px 4px 3px 4px;
                width: 13px;
                height: 13px;
                border: 1px solid rgb(118, 118, 118);
                background-color: rgb(255, 255, 255);
                border-radius: 50%;
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            input[type="range"] {
                display: inline-block;
                appearance: slider-horizontal;
                cursor: default;
                margin: 2px;
                width: 129px;
                height: 16px;
                border: none;
                color: rgb(144, 144, 144);
                padding: 0px;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: left;
            }
            button, input[type="button"], input[type="submit"], input[type="reset"] {
                display: inline-block;
                appearance: button;
                cursor: default;
                box-sizing: border-box;
                background-color: rgb(239, 239, 239);
                color: rgb(0, 0, 0);
                border: 2px solid rgb(118, 118, 118);
                border-radius: 2px;
                padding: 2px 6px 3px 6px;
                font-family: system-ui;
                font-size: 13.333px;
                font-weight: normal;
                font-style: normal;
                line-height: 1.2;
                letter-spacing: 0px;
                word-spacing: 0px;
                text-transform: none;
                text-align: center;
            }
            details { display: block; }
            summary { display: block; padding-left: 20px; cursor: pointer; }
            dialog { display: none; position: absolute; background-color: rgb(255, 255, 255); color: rgb(0, 0, 0); border: 1px solid rgb(0, 0, 0); padding: 20px; }
            meter { display: inline-block; width: 80px; height: 16px; background-color: rgb(229, 229, 229); border: 1px solid rgb(179, 179, 179); border-radius: 4px; }
            progress { display: inline-block; width: 160px; height: 16px; background-color: rgba(26, 26, 26, 0.1); border: 1px solid rgba(76, 76, 76, 0.3); border-radius: 8px; }
            br { display: block; width: 100%; height: 0px; }
            [dir="rtl"] { direction: rtl; unicode-bidi: isolate; }
            [dir="ltr"] { direction: ltr; unicode-bidi: isolate; }
        )CSS");
        initialized = true;
    }
    return uaSheet;
}
void StyleSheet::applyUserAgentDefaults(Style& style,
                                        std::string_view type,
                                        const std::vector<CSSSelectorNode>& ancestors,
                                        const Widget* widget) {
    const StyleSheet& uaSheet = getUaSheet();
    std::vector<size_t> candidateRules;
    uaSheet.collectCandidateRules("", "", type, candidateRules);
    struct WinningProperty {
        const CSSProperty* property = nullptr;
        int specificity = 0;
        int sourceOrder = 0;
        bool isWorseThan(const WinningProperty& o) const {
            if (specificity != o.specificity) return specificity < o.specificity;
            return sourceOrder < o.sourceOrder;
        }
    };
    std::unordered_map<std::string, WinningProperty> winningProperties;
    for (size_t ruleIndex : candidateRules) {
        if (ruleIndex >= uaSheet.rules.size()) continue;
        const auto& rule = uaSheet.rules[ruleIndex];
        std::string_view pseudo;
        if (selectorMatches(rule, "", "", type, ancestors, &pseudo, widget)) {
            for (const auto& prop : rule.properties) {
                WinningProperty wp{&prop, rule.specificity, (int)prop.sourceOrder};
                auto it = winningProperties.find(prop.name.getString());
                if (it == winningProperties.end() || it->second.isWorseThan(wp)) {
                    winningProperties[prop.name.getString()] = wp;
                }
            }
        }
    }
    for (const auto& entry : winningProperties) {
        StyleSheet::mergeProperty(style, entry.first, entry.second.property->value, style.fontSize);
    }
}


} // namespace FluxUI