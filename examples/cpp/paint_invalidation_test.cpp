#include "fluxui/FluxUI.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include <iostream>
#include <cassert>

int main() {
    FluxUI::Application app;
    
    // Create widgets manually
    auto root = std::make_shared<FluxUI::Widget>();
    root->className = "root";
    root->computedStyle->display = FluxUI::Display::Flex;
    root->computedStyle->visibility = FluxUI::Visibility::Visible;
    root->computedStyle->backgroundColor = FluxUI::Color(1.0f, 0.0f, 0.0f, 1.0f); // red
    root->computedStyle->width = FluxUI::CSSValue::px(100.0f);
    root->computedStyle->height = FluxUI::CSSValue::px(100.0f);
    
    auto child = std::make_shared<FluxUI::Widget>();
    child->className = "child";
    child->computedStyle->display = FluxUI::Display::Flex;
    child->computedStyle->visibility = FluxUI::Visibility::Visible;
    child->computedStyle->backgroundColor = FluxUI::Color(0.0f, 1.0f, 0.0f, 1.0f); // green
    child->computedStyle->width = FluxUI::CSSValue::px(50.0f);
    child->computedStyle->height = FluxUI::CSSValue::px(50.0f);
    root->addChild(child);

    // Build layout tree manually
    root->attachLayoutTree();
    
    auto* rootLayout = root->layoutObject.get();
    auto* childLayout = child->layoutObject.get();
    
    assert(rootLayout != nullptr);
    assert(childLayout != nullptr);
    
    // Initially, both should have paintDirty_ = true
    std::cout << "Initial root paintDirty: " << rootLayout->isPaintDirty() << " (expected 1)\n";
    std::cout << "Initial child paintDirty: " << childLayout->isPaintDirty() << " (expected 1)\n";
    
    assert(rootLayout->isPaintDirty());
    assert(childLayout->isPaintDirty());
    
    // Create a renderer
    FluxUI::Renderer renderer;
    
    // Solve layout first
    FluxUI::LayoutConstraints constraints;
    constraints.availableWidth = 800;
    constraints.availableHeight = 600;
    rootLayout->layout(constraints);
    
    // Painting root (which recursively paints child)
    rootLayout->paint(renderer);
    
    // After paint, paintDirty should be false
    std::cout << "After paint root paintDirty: " << rootLayout->isPaintDirty() << " (expected 0)\n";
    std::cout << "After paint child paintDirty: " << childLayout->isPaintDirty() << " (expected 0)\n";
    
    assert(!rootLayout->isPaintDirty());
    assert(!childLayout->isPaintDirty());
    
    // The command lists should have recorded background colors
    std::cout << "Root cached command count: " << rootLayout->cachedCommands().size() << "\n";
    std::cout << "Child cached command count: " << childLayout->cachedCommands().size() << "\n";
    
    assert(rootLayout->cachedCommands().size() > 0);
    assert(childLayout->cachedCommands().size() > 0);
    
    // Keep track of the command lists
    auto rootCommandsBefore = rootLayout->cachedCommands();
    auto childCommandsBefore = childLayout->cachedCommands();
    
    // Run paint again. Since paintDirty_ is false, it should play back and NOT re-record!
    rootLayout->paint(renderer);
    
    // Make sure they remained clean and cached commands did not change
    assert(!rootLayout->isPaintDirty());
    assert(!childLayout->isPaintDirty());
    assert(rootLayout->cachedCommands().size() == rootCommandsBefore.size());
    assert(childLayout->cachedCommands().size() == childCommandsBefore.size());
    
    std::cout << "Second paint succeeded with cache intact.\n";
    
    // Now invalidating child paint directly (granular invalidation!) should mark ONLY the child paint dirty
    childLayout->markPaintDirty();
    
    std::cout << "After child invalidation root paintDirty: " << rootLayout->isPaintDirty() << " (expected 0)\n";
    std::cout << "After child invalidation child paintDirty: " << childLayout->isPaintDirty() << " (expected 1)\n";
    
    assert(!rootLayout->isPaintDirty()); // Root remains clean!
    assert(childLayout->isPaintDirty()); // Child becomes dirty!
    
    // Re-resolve layout and paint
    rootLayout->layout(constraints);
    rootLayout->paint(renderer);
    
    assert(!rootLayout->isPaintDirty());
    assert(!childLayout->isPaintDirty());
    
    std::cout << "Granular Paint Invalidation Verification: PASS\n";
    return 0;
}
