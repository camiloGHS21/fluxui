#include "fluxui/widgets.h"
#include "fluxui/core.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include <iostream>
#include <sstream>
#include <cassert>

using namespace FluxUI;

#define EXPECT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cout << "[FAIL] " << msg << std::endl; \
        return 1; \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

int main() {
    std::cout << "Starting Chromium Blink Engine Features verification test..." << std::endl;

    // =========================================================================
    // 1. CSS Containment & CSS Nesting Parser Verification
    // =========================================================================
    std::cout << "\n--- Testing CSS Parser Enhancements ---" << std::endl;
    StyleSheet sheet;

    // Test CSS Containment parsing
    std::string cssContain = ".contain-box { contain: strict; }";
    sheet.parse(cssContain);
    
    Style testStyle = sheet.resolve("contain-box");
    
    // Check that style contain property contains size, layout, paint
    bool containParsed = (testStyle.contain & ContainmentFlags::kContainSize) &&
                         (testStyle.contain & ContainmentFlags::kContainLayout) &&
                         (testStyle.contain & ContainmentFlags::kContainPaint);
    EXPECT_TRUE(containParsed, "contain: strict parsed size, layout, paint flags correctly");

    // Test CSS Nesting parser (nested brace resolution)
    std::string cssNesting = ".parent { color: rgb(255, 0, 0); .child { font-size: 20px; } & .sibling { width: 100px; } }";
    sheet.parse(cssNesting);

    bool foundNestedChild = false;
    bool foundNestedSibling = false;
    for (const auto& rule : sheet.rules) {
        if (rule.selector == ".parent .child") {
            for (const auto& prop : rule.properties) {
                if (prop.name == "font-size" && prop.value == "20px") {
                    foundNestedChild = true;
                }
            }
        }
        if (rule.selector == ".parent .sibling") {
            for (const auto& prop : rule.properties) {
                if (prop.name == "width" && prop.value == "100px") {
                    foundNestedSibling = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundNestedChild, "CSS Nesting resolved sub-selectors recursively (.parent .child)");
    EXPECT_TRUE(foundNestedSibling, "CSS Nesting resolved '&' parent references correctly (.parent .sibling)");

    // =========================================================================
    // 2. CSS Containment Layout Sizing Override Verification
    // =========================================================================
    std::cout << "\n--- Testing CSS Containment Sizing Overrides ---" << std::endl;
    auto parentWidget = std::make_shared<Panel>("parent-box");
    parentWidget->style.contain = ContainmentFlags::kContainSize;
    parentWidget->style.width = 150.0f;
    parentWidget->style.height = 100.0f;

    auto childWidget = std::make_shared<Panel>("child-large");
    childWidget->style.width = 500.0f;
    childWidget->style.height = 400.0f;
    parentWidget->addChild(childWidget);

    parentWidget->resolveStyles(sheet);
    childWidget->resolveStyles(sheet);

    // Sizing under contain: size should completely ignore child bounds
    parentWidget->layout({0.0f, 0.0f, 800.0f, 600.0f});
    
    EXPECT_TRUE(parentWidget->bounds.w == 150.0f, "contain: size ignored child width bounds");
    EXPECT_TRUE(parentWidget->bounds.h == 100.0f, "contain: size ignored child height bounds");

    // =========================================================================
    // 3. View Transitions Verification
    // =========================================================================
    std::cout << "\n--- Testing SPA View Transitions Engine ---" << std::endl;
    Application app;
    Widget* root = app.root();
    EXPECT_TRUE(root != nullptr, "Application root is valid");

    auto transitionPanel = std::make_shared<Panel>("transition-panel");
    root->addChild(transitionPanel);

    bool mutationExecuted = false;
    app.startViewTransition([&]() {
        mutationExecuted = true;
        transitionPanel->style.width = 300.0f;
    });

    EXPECT_TRUE(mutationExecuted, "View transition mutation callback executed");
    
    // Check that style was correctly dirtied and layout resolved
    EXPECT_TRUE(transitionPanel->bounds.w == 300.0f, "View transition updated layout of new state");

    // =========================================================================
    // 4. OffscreenCanvas Decoupled Sizing & API Verification
    // =========================================================================
    std::cout << "\n--- Testing Decoupled OffscreenCanvas API ---" << std::endl;
    OffscreenCanvas offscreen(400, 300);
    EXPECT_TRUE(offscreen.width == 400 && offscreen.height == 300, "OffscreenCanvas instantiated with valid bounds");

    offscreen.startPaint();
    offscreen.drawRoundedRect({10, 10, 100, 50}, Color(0, 1, 0, 1), BorderRadius(5));
    offscreen.drawText("Headless Render", {15, 20}, Color(1, 1, 1, 1), 14.0f);
    offscreen.drawImage("logo.png", {0, 0, 400, 300});

    EXPECT_TRUE(offscreen.commands.size() == 3, "OffscreenCanvas recorded 2D draw instructions into command buffer");
    EXPECT_TRUE(offscreen.commands[0].type == RenderCommandType::RoundedRect, "First command is RoundedRect");
    EXPECT_TRUE(offscreen.commands[1].type == RenderCommandType::Text, "Second command is Text");
    EXPECT_TRUE(offscreen.commands[2].type == RenderCommandType::TexturedQuad, "Third command is TexturedQuad");

    // =========================================================================
    // 5. Dialog Close Click Event Bubbling Verification
    // =========================================================================
    std::cout << "\n--- Testing Dialog Close & Click Bubbling ---" << std::endl;
    auto mainPanel = std::make_shared<Panel>("main-panel");
    auto dl = mainPanel->add<Dialog>("dialog");
    auto dialogButtons = dl->add<Panel>("dialog-buttons");
    auto declineBtn = dialogButtons->add<Button>("Decline", "btn btn-danger btn-small");
    
    bool dialogClosed = false;
    declineBtn->onClick = [&]() {
        std::cout << "[Test] Decline button clicked! Closing dialog..." << std::endl;
        dl->close();
        dialogClosed = true;
    };
    
    // Check initial state
    dl->showModal();
    EXPECT_TRUE(dl->open, "Dialog is open initially");
    EXPECT_TRUE(dl->style.display == Display::Block, "Dialog style display is Block");
    
    // Simulate click
    Event clickEv;
    clickEv.type = "click";
    clickEv.target = declineBtn;
    clickEv.bubbles = true;
    
    declineBtn->dispatchEvent(clickEv);
    
    // Resolve styles and layout after dialog is closed to ensure no layout/styling crashes
    mainPanel->resolveStyles(sheet);
    mainPanel->layout({0, 0, 800, 600});
    
    EXPECT_TRUE(dialogClosed, "Decline button onClick handler executed");
    EXPECT_TRUE(!dl->open, "Dialog is closed after click");
    EXPECT_TRUE(dl->style.display == Display::None, "Dialog style display is None");

    std::cout << "\n[SUCCESS] All Chromium Blink features verified successfully!" << std::endl;
    return 0;
}

