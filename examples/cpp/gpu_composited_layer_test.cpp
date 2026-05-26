#include "fluxui/FluxUI.h"
#include "fluxui/layout.h"
#include "fluxui/layout_object.h"
#include "fluxui/gpu_paint_layer.h"
#include <iostream>
#include <cstdlib>

#define EXPECT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << msg << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

int main() {
    std::cout << "Starting GPU Composited Layer test..." << std::endl;
    
    FluxUI::Application app;
    std::cout << "Application initialized." << std::endl;
    
    auto root = std::make_shared<FluxUI::Widget>();
    root->className = "root";
    root->computedStyle->display = FluxUI::Display::Flex;
    root->computedStyle->visibility = FluxUI::Visibility::Visible;
    root->computedStyle->width = FluxUI::CSSValue::px(300.0f);
    root->computedStyle->height = FluxUI::CSSValue::px(300.0f);
    
    auto layered = std::make_shared<FluxUI::Widget>();
    layered->className = "layered";
    layered->computedStyle->display = FluxUI::Display::Flex;
    layered->computedStyle->visibility = FluxUI::Visibility::Visible;
    layered->computedStyle->width = FluxUI::CSSValue::px(200.0f);
    layered->computedStyle->height = FluxUI::CSSValue::px(200.0f);
    layered->useGPUCompositing = true;
    root->addChild(layered);
    
    auto grandchild = std::make_shared<FluxUI::Widget>();
    grandchild->className = "grandchild";
    grandchild->computedStyle->display = FluxUI::Display::Flex;
    grandchild->computedStyle->visibility = FluxUI::Visibility::Visible;
    grandchild->computedStyle->width = FluxUI::CSSValue::px(100.0f);
    grandchild->computedStyle->height = FluxUI::CSSValue::px(100.0f);
    layered->addChild(grandchild);

    root->attachLayoutTree();
    
    auto* rootLayout = root->layoutObject.get();
    auto* layeredLayout = layered->layoutObject.get();
    auto* grandchildLayout = grandchild->layoutObject.get();
    
    EXPECT(rootLayout != nullptr, "rootLayout is not null");
    EXPECT(layeredLayout != nullptr, "layeredLayout is not null");
    EXPECT(grandchildLayout != nullptr, "grandchildLayout is not null");
    
    EXPECT(layeredLayout->getPaintLayer() == nullptr, "layered paint layer is initially null");
    
    FluxUI::LayoutConstraints constraints;
    constraints.availableWidth = 800;
    constraints.availableHeight = 600;
    rootLayout->layout(constraints);
    
    FluxUI::Renderer renderer;
    std::cout << "Rendering first pass..." << std::endl;
    rootLayout->paint(renderer);
    
    auto* paintLayer = layeredLayout->getPaintLayer();
    EXPECT(paintLayer != nullptr, "layered paint layer is created after paint");
    if (paintLayer) {
        std::cout << "FBO ID: " << paintLayer->fbo() << ", Texture ID: " << paintLayer->textureId() << std::endl;
        EXPECT(paintLayer->fbo() != 0, "FBO ID is non-zero");
        EXPECT(paintLayer->textureId() != 0, "Texture ID is non-zero");
        EXPECT(paintLayer->width() == 200, "Layer width is 200");
        EXPECT(paintLayer->height() == 200, "Layer height is 200");
        EXPECT(!paintLayer->isDirty(), "Layer is clean after painting");
    }
    
    EXPECT(!layeredLayout->isPaintDirty(), "layeredLayout is clean after painting");
    EXPECT(!grandchildLayout->isPaintDirty(), "grandchildLayout is clean after painting");
    
    grandchildLayout->markPaintDirty();
    if (paintLayer) {
        EXPECT(paintLayer->isDirty(), "Layer is marked dirty after grandchild invalidation");
    }
    EXPECT(grandchildLayout->isPaintDirty(), "grandchildLayout is dirty");
    EXPECT(!layeredLayout->isPaintDirty(), "layeredLayout itself is not dirty");
    
    std::cout << "Rendering second pass..." << std::endl;
    rootLayout->paint(renderer);
    if (paintLayer) {
        EXPECT(!paintLayer->isDirty(), "Layer is clean again after repaint");
    }
    
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}
