#include "fluxui/core.h"
#include "fluxui/css_parser.h"
#include <iostream>
#include <cassert>

using namespace FluxUI;

void testCase1_horizontal_ltr() {
    Style style;
    StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
    StyleSheet::mergeProperty(style, "direction", "ltr");
    
    StyleSheet::mergeProperty(style, "margin-inline-start", "10px");
    StyleSheet::mergeProperty(style, "margin-inline-end", "20px");
    StyleSheet::mergeProperty(style, "margin-block-start", "30px");
    StyleSheet::mergeProperty(style, "margin-block-end", "40px");

    style.resolveLogicalProperties();

    assert(style.margin.left == 10.0f);
    assert(style.margin.right == 20.0f);
    assert(style.margin.top == 30.0f);
    assert(style.margin.bottom == 40.0f);
    std::cout << "Case 1 (Horizontal TB, LTR) passed!" << std::endl;
}

void testCase2_horizontal_rtl() {
    Style style;
    StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
    StyleSheet::mergeProperty(style, "direction", "rtl");
    
    StyleSheet::mergeProperty(style, "margin-inline-start", "10px");
    StyleSheet::mergeProperty(style, "margin-inline-end", "20px");
    StyleSheet::mergeProperty(style, "margin-block-start", "30px");
    StyleSheet::mergeProperty(style, "margin-block-end", "40px");

    style.resolveLogicalProperties();

    assert(style.margin.right == 10.0f);
    assert(style.margin.left == 20.0f);
    assert(style.margin.top == 30.0f);
    assert(style.margin.bottom == 40.0f);
    std::cout << "Case 2 (Horizontal TB, RTL) passed!" << std::endl;
}

void testCase3_vertical_rl_ltr() {
    Style style;
    StyleSheet::mergeProperty(style, "writing-mode", "vertical-rl");
    StyleSheet::mergeProperty(style, "direction", "ltr");
    
    StyleSheet::mergeProperty(style, "margin-inline-start", "10px");
    StyleSheet::mergeProperty(style, "margin-inline-end", "20px");
    StyleSheet::mergeProperty(style, "margin-block-start", "30px");
    StyleSheet::mergeProperty(style, "margin-block-end", "40px");

    style.resolveLogicalProperties();

    assert(style.margin.top == 10.0f);
    assert(style.margin.bottom == 20.0f);
    assert(style.margin.right == 30.0f);
    assert(style.margin.left == 40.0f);
    std::cout << "Case 3 (Vertical RL, LTR) passed!" << std::endl;
}

void testCase4_vertical_lr_rtl() {
    Style style;
    StyleSheet::mergeProperty(style, "writing-mode", "vertical-lr");
    StyleSheet::mergeProperty(style, "direction", "rtl");
    
    StyleSheet::mergeProperty(style, "margin-inline-start", "10px");
    StyleSheet::mergeProperty(style, "margin-inline-end", "20px");
    StyleSheet::mergeProperty(style, "margin-block-start", "30px");
    StyleSheet::mergeProperty(style, "margin-block-end", "40px");

    style.resolveLogicalProperties();

    assert(style.margin.bottom == 10.0f);
    assert(style.margin.top == 20.0f);
    assert(style.margin.left == 30.0f);
    assert(style.margin.right == 40.0f);
    std::cout << "Case 4 (Vertical LR, RTL) passed!" << std::endl;
}

void testCase5_overwrite_precedence() {
    // Physical overriding logical
    {
        Style style;
        StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
        StyleSheet::mergeProperty(style, "direction", "ltr");
        StyleSheet::mergeProperty(style, "margin-inline-start", "15px");
        StyleSheet::mergeProperty(style, "margin-left", "25px");

        style.resolveLogicalProperties();
        assert(style.margin.left == 25.0f);
    }

    // Logical overriding physical
    {
        Style style;
        StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
        StyleSheet::mergeProperty(style, "direction", "ltr");
        StyleSheet::mergeProperty(style, "margin-left", "25px");
        StyleSheet::mergeProperty(style, "margin-inline-start", "15px");

        style.resolveLogicalProperties();
        assert(style.margin.left == 15.0f);
    }
    std::cout << "Case 5 (Precedence Cascading) passed!" << std::endl;
}

void testCase6_dimensions() {
    // Horizontal TB
    {
        Style style;
        StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
        StyleSheet::mergeProperty(style, "inline-size", "100px");
        StyleSheet::mergeProperty(style, "block-size", "200px");

        style.resolveLogicalProperties();
        assert(style.width.resolve(0.0f) == 100.0f);
        assert(style.height.resolve(0.0f) == 200.0f);
    }

    // Vertical RL
    {
        Style style;
        StyleSheet::mergeProperty(style, "writing-mode", "vertical-rl");
        StyleSheet::mergeProperty(style, "inline-size", "100px");
        StyleSheet::mergeProperty(style, "block-size", "200px");

        style.resolveLogicalProperties();
        assert(style.width.resolve(0.0f) == 200.0f);
        assert(style.height.resolve(0.0f) == 100.0f);
    }
    std::cout << "Case 6 (Logical Dimensions) passed!" << std::endl;
}

void testCase7_shorthands() {
    Style style;
    StyleSheet::mergeProperty(style, "writing-mode", "horizontal-tb");
    StyleSheet::mergeProperty(style, "direction", "ltr");
    
    StyleSheet::mergeProperty(style, "margin-block", "15px 25px");
    StyleSheet::mergeProperty(style, "padding-inline", "5px");

    style.resolveLogicalProperties();

    assert(style.margin.top == 15.0f);
    assert(style.margin.bottom == 25.0f);
    assert(style.padding.left == 5.0f);
    assert(style.padding.right == 5.0f);
    std::cout << "Case 7 (Shorthands) passed!" << std::endl;
}

int main() {
    std::cout << "Running CSS Logical Properties Parity Tests..." << std::endl;
    testCase1_horizontal_ltr();
    testCase2_horizontal_rtl();
    testCase3_vertical_rl_ltr();
    testCase4_vertical_lr_rtl();
    testCase5_overwrite_precedence();
    testCase6_dimensions();
    testCase7_shorthands();
    std::cout << "All logical properties tests passed successfully!" << std::endl;
    return 0;
}
