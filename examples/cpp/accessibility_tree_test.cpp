#include "fluxui/widgets.h"
#include "fluxui/accessibility.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include <iostream>
#include <sstream>

using namespace FluxUI;

#define EXPECT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cout << "[FAIL] " << msg << std::endl; \
        return 1; \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

int main() {
    std::cout << "Starting Accessibility Tree verification test..." << std::endl;

    // 1. Initialize application
    Application app;
    Widget* root = app.root();
    EXPECT_TRUE(root != nullptr, "Root is not null");

    // 2. Build test DOM/Widget tree
    auto panel = std::make_shared<Panel>("test-panel");
    panel->id = "main-panel";
    
    auto welcomeText = std::make_shared<Text>("Welcome to DataLeakGuard", "welcome-title");
    auto scanButton = std::make_shared<Button>("Secure File Scan", "btn-primary");
    scanButton->focused = true; // Set focus to check state propagation

    auto agreeCheckbox = std::make_shared<Checkbox>(true, "agree-cb");
    
    auto keyInput = std::make_shared<TextInput>("Enter Security Key", "key-input");
    keyInput->value = "SECRET_TOKEN_XYZ";

    panel->addChild(welcomeText);
    panel->addChild(scanButton);
    panel->addChild(agreeCheckbox);
    panel->addChild(keyInput);
    root->addChild(panel);

    // 3. Trigger Style & Layout updates manually (headless-safe)
    std::cout << "Attaching layout tree..." << std::endl;
    root->attachLayoutTree();

    std::cout << "Resolving layout..." << std::endl;
    if (root->layoutObject) {
        LayoutConstraints constraints;
        constraints.availableWidth = 800.0f;
        constraints.availableHeight = 600.0f;
        constraints.parentWidth = 800.0f;
        constraints.parentHeight = 600.0f;
        constraints.emBase = 16.0f;
        root->layoutObject->layout(constraints);
    } else {
        root->layout({0.0f, 0.0f, 800.0f, 600.0f});
    }

    // 4. Retrieve AXObjectCache and update it
    std::cout << "Retrieving AXObjectCache..." << std::endl;
    AXObjectCache* cache = app.axObjectCache();
    EXPECT_TRUE(cache != nullptr, "AXObjectCache is not null");

    std::cout << "Updating cache..." << std::endl;
    cache->update(root);

    AXObject* axRoot = cache->root();
    EXPECT_TRUE(axRoot != nullptr, "Accessibility Root node is created");
    EXPECT_TRUE(axRoot->role() == AXRole::kRootWebArea, "Root role is kRootWebArea");

    // 5. Recursively search AX tree for created nodes and verify roles/states
    bool foundTitle = false;
    bool foundButton = false;
    bool foundCheckbox = false;
    bool foundInput = false;
    bool verificationPassed = true;

    auto verifyAXTree = [&](auto& self, AXObject* node) -> void {
        if (!node) return;

        if (node->role() == AXRole::kStaticText && node->name() == "Welcome to DataLeakGuard") {
            foundTitle = true;
        }
        if (node->role() == AXRole::kButton && node->name() == "Secure File Scan") {
            foundButton = true;
            if (!node->hasState(AXState::kFocused)) {
                std::cout << "[FAIL] Button is not marked FOCUSED" << std::endl;
                verificationPassed = false;
            }
            if (!node->hasState(AXState::kFocusable)) {
                std::cout << "[FAIL] Button is not marked FOCUSABLE" << std::endl;
                verificationPassed = false;
            }
        }
        if (node->role() == AXRole::kCheckbox) {
            foundCheckbox = true;
            if (!node->hasState(AXState::kChecked)) {
                std::cout << "[FAIL] Checkbox is not marked CHECKED" << std::endl;
                verificationPassed = false;
            }
            if (node->value() != "true") {
                std::cout << "[FAIL] Checkbox value is not 'true'" << std::endl;
                verificationPassed = false;
            }
        }
        if (node->role() == AXRole::kTextField) {
            foundInput = true;
            if (node->value() != "SECRET_TOKEN_XYZ") {
                std::cout << "[FAIL] TextField value does not match" << std::endl;
                verificationPassed = false;
            }
            if (node->description() != "Enter Security Key") {
                std::cout << "[FAIL] TextField placeholder does not match" << std::endl;
                verificationPassed = false;
            }
        }

        for (auto* child : node->children()) {
            self(self, child);
        }
    };

    std::cout << "Verifying AX tree nodes..." << std::endl;
    verifyAXTree(verifyAXTree, axRoot);

    EXPECT_TRUE(foundTitle, "Found StaticText node in AX Tree");
    EXPECT_TRUE(foundButton, "Found Button node in AX Tree");
    EXPECT_TRUE(foundCheckbox, "Found Checkbox node in AX Tree");
    EXPECT_TRUE(foundInput, "Found TextField node in AX Tree");
    EXPECT_TRUE(verificationPassed, "All custom node checks passed");

    // 6. Output the serialized accessibility tree structure
    std::cout << "\n--- Serialized Accessibility Tree (Blink AXObjectCache) ---" << std::endl;
    std::cout << cache->dumpTree() << std::endl;

    std::cout << "All Accessibility Tree tests passed successfully!" << std::endl;
    return 0;
}
