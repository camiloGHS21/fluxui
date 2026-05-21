#include "fluxui/FluxUI.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace FluxUI;

struct Point3D {
    float x, y, z;
};

struct Edge {
    int u, v;
    Color color;
};

struct Star3D {
    float x, y, z;
};

class HologramState {
public:
    std::vector<Point3D> vertices;
    std::vector<Edge> edges;
    std::vector<Star3D> stars;
    float angleX = 0.0f;
    float angleY = 0.0f;
    float angleZ = 0.0f;
    float terrainOffset = 0.0f;
    float rippleTime = 0.0f;
    float time = 0.0f;

    void init() {
        // Create 3D Octahedron core vertices
        vertices = {
            { 0, -80, 0 },  // Top
            { 0, 80, 0 },   // Bottom
            { -60, 0, -60 }, // Middle ring
            { 60, 0, -60 },
            { 60, 0, 60 },
            { -60, 0, 60 }
        };

        // Create edges connecting the core
        Color coreColor = Color::fromHex("#C77CFF");
        edges = {
            { 0, 2, coreColor }, { 0, 3, coreColor }, { 0, 4, coreColor }, { 0, 5, coreColor },
            { 1, 2, coreColor }, { 1, 3, coreColor }, { 1, 4, coreColor }, { 1, 5, coreColor },
            { 2, 3, coreColor }, { 3, 4, coreColor }, { 4, 5, coreColor }, { 5, 2, coreColor }
        };

        // Create secondary orbiting rings vertices & edges
        int ringStart = vertices.size();
        int ringSegments = 16;
        float radius = 100.0f;
        for (int i = 0; i < ringSegments; ++i) {
            float theta = (float)i / ringSegments * 2.0f * 3.14159f;
            vertices.push_back({ radius * std::cos(theta), 0.0f, radius * std::sin(theta) });
        }
        for (int i = 0; i < ringSegments; ++i) {
            edges.push_back({ ringStart + i, ringStart + (i + 1) % ringSegments, Color::fromHex("#39B5FF") });
        }

        // Add 3D Starfield
        stars.clear();
        for (int i = 0; i < 60; ++i) {
            stars.push_back({
                (float)(rand() % 800 - 400),
                (float)(rand() % 600 - 300),
                (float)(100 + rand() % 900)
            });
        }
    }
};

static HologramState state;

void rotateX(Point3D& p, float angle) {
    float rad = angle;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float y = p.y * cosA - p.z * sinA;
    float z = p.y * sinA + p.z * cosA;
    p.y = y;
    p.z = z;
}

void rotateY(Point3D& p, float angle) {
    float rad = angle;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float x = p.x * cosA + p.z * sinA;
    float z = -p.x * sinA + p.z * cosA;
    p.x = x;
    p.z = z;
}

void rotateZ(Point3D& p, float angle) {
    float rad = angle;
    float cosA = std::cos(rad);
    float sinA = std::sin(rad);
    float x = p.x * cosA - p.y * sinA;
    float y = p.x * sinA + p.y * cosA;
    p.x = x;
    p.y = y;
}

void updateHologram(Application& app, float dt) {
    state.time += dt;

    // Automatic rotation
    state.angleX += 0.4f * dt;
    state.angleY += 0.6f * dt;
    state.angleZ += 0.2f * dt;

    // Add interactive rotation using mouse movement delta
    Vec2 mouseDelta = app.input().mouseDelta;
    if (app.input().mouseDown[0]) {
        state.angleY += mouseDelta.x * 0.01f;
        state.angleX += mouseDelta.y * 0.01f;
    }

    // Ripple effect timer
    if (state.rippleTime > 0.0f) {
        state.rippleTime -= dt;
    }
    if (app.input().mouseClicked[0]) {
        state.rippleTime = 1.0f; // Reset shockwave ripple
    }

    // Move synthwave terrain/stars
    state.terrainOffset += 80.0f * dt;
    for (auto& star : state.stars) {
        star.z -= 150.0f * dt;
        if (star.z < 20.0f) {
            star.z = 1000.0f;
            star.x = (float)(rand() % 800 - 400);
            star.y = (float)(rand() % 600 - 300);
        }
    }
}

void drawHologram(Widget* canvas, Renderer& r, const Rect& bounds) {
    // Elegant synthwave background gradient
    r.drawRoundedRect(bounds, Color::fromHex("#0b0914"), BorderRadius(0.0f));

    float centerX = bounds.x + bounds.w / 2.0f;
    float centerY = bounds.y + bounds.h / 2.0f;
    float fov = 400.0f;

    // ── Draw 3D Starfield ──
    for (const auto& star : state.stars) {
        if (star.z <= 0) continue;
        float sx = centerX + star.x * (fov / star.z);
        float sy = centerY + star.y * (fov / star.z);

        if (sx >= bounds.x && sx <= bounds.x + bounds.w && sy >= bounds.y && sy <= bounds.y + bounds.h) {
            float size = std::clamp(3.0f * (1.0f - star.z / 1000.0f), 0.5f, 4.0f);
            float opacity = std::clamp(1.0f - star.z / 1000.0f, 0.0f, 1.0f);
            r.drawRoundedRect(Rect(sx - size/2, sy - size/2, size, size), Color(1, 1, 1, opacity), BorderRadius(size/2));
        }
    }

    // ── Draw 3D Synthwave Grid (Ground Terrain) ──
    float gridZStart = 100.0f;
    float gridZEnd = 800.0f;
    float gridSpacingZ = 60.0f;
    float gridSpacingX = 80.0f;
    float groundY = 200.0f;

    // Draw horizontal perspective grid lines
    float currentZ = gridZStart - std::fmod(state.terrainOffset, gridSpacingZ);
    while (currentZ < gridZEnd) {
        if (currentZ > 20.0f) {
            float z = currentZ;
            float leftX = centerX + (-600.0f) * (fov / z);
            float rightX = centerX + (600.0f) * (fov / z);
            float y = centerY + groundY * (fov / z);

            float opacity = 0.3f * (1.0f - z / gridZEnd);
            r.drawRoundedRect(Rect(leftX, y, rightX - leftX, 1.0f), Color::fromHex("#C77CFF").withAlpha(opacity), BorderRadius(0.0f));
        }
        currentZ += gridSpacingZ;
    }

    // Draw perspective columns
    for (float gx = -600.0f; gx <= 600.0f; gx += gridSpacingX) {
        float x1 = centerX + gx * (fov / gridZStart);
        float y1 = centerY + groundY * (fov / gridZStart);
        float x2 = centerX + gx * (fov / gridZEnd);
        float y2 = centerY + groundY * (fov / gridZEnd);

        r.drawRoundedRect(Rect(x2, y2, 1.0f, 1.0f), Color::fromHex("#C77CFF").withAlpha(0.15f), BorderRadius(0.0f));
        // Note: For actual wireframe line drawing, we draw using rect segments since we have a fast fill rect
        // We will just draw a series of vertical connect segments or just keep it simple!
    }

    // ── Project and Draw 3D Hologram Core ──
    std::vector<Point3D> projected;
    for (const auto& v : state.vertices) {
        Point3D rotated = v;
        rotateX(rotated, state.angleX);
        rotateY(rotated, state.angleY);
        rotateZ(rotated, state.angleZ);

        // Move target core forward along Z axis in camera space
        float z = rotated.z + 400.0f;
        float px = centerX + rotated.x * (fov / z);
        float py = centerY + rotated.y * (fov / z);
        projected.push_back({ px, py, z });
    }

    // Draw edges
    for (const auto& edge : state.edges) {
        if (edge.u >= projected.size() || edge.v >= projected.size()) continue;
        const auto& p1 = projected[edge.u];
        const auto& p2 = projected[edge.v];

        // Draw a neat neon-colored connector using micro rects
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        int steps = std::max(5, (int)(dist / 3.0f));
        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / steps;
            float px = p1.x + dx * t;
            float py = p1.y + dy * t;
            
            float dotSize = 2.5f;
            float zCoeff = 400.0f / (p1.z + (p2.z - p1.z) * t);
            dotSize *= zCoeff;

            r.drawRoundedRect(Rect(px - dotSize/2, py - dotSize/2, dotSize, dotSize), edge.color, BorderRadius(dotSize/2));
        }
    }

    // Draw Glow Core points
    for (const auto& p : projected) {
        float size = 8.0f * (400.0f / p.z);
        r.drawRoundedRect(Rect(p.x - size/2, p.y - size/2, size, size), Color::fromHex("#EDF3F8"), BorderRadius(size/2));
    }

    // Shockwave ripple overlay
    if (state.rippleTime > 0.0f) {
        float radius = (1.0f - state.rippleTime) * 350.0f;
        float thickness = 4.0f;
        // Draw expanding ripple circle
        // We will approximate it with beautiful orbiting particles or an expanding outline
        r.drawRoundedRect(Rect(centerX - radius, centerY - radius, radius*2, thickness), Color::fromHex("#FF00AA").withAlpha(state.rippleTime), BorderRadius(2.0f));
        r.drawRoundedRect(Rect(centerX - radius, centerY + radius, radius*2, thickness), Color::fromHex("#FF00AA").withAlpha(state.rippleTime), BorderRadius(2.0f));
    }

    // Interface
    r.drawText("VECTOR ENGINE: 3D RENDER", Vec2(bounds.x + 30.0f, bounds.y + 40.0f), Color::fromHex("#C77CFF"), 20.0f);
    r.drawText("FPS: 60 - REALTIME WIREFRAME", Vec2(bounds.x + 30.0f, bounds.y + 65.0f), Color(1, 1, 1, 0.5f), 13.0f);
}

int main() {
    Application app;
    app.setBackend(RenderBackendType::Vulkan);

    if (!app.init("FluxUI 3D Vector Hologram", 1200, 750)) {
        return 1;
    }

    state.init();

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{12.0f, 13.0f, 18.0f, 20.0f, 24.0f, 28.0f});
    app.renderer().releaseFontSources();
    app.addStylesheet(
        ".root { display: flex; flex-direction: row; background-color: #0b0914; color: #edf3f8; }"
        ".sidebar { width: 300px; height: 100%; background-color: #120e22; padding: 24px; display: flex; flex-direction: column; gap: 20px; border-right: 1px solid #231b40; }"
        ".side-title { font-size: 24px; font-weight: 700; color: #C77CFF; }"
        ".side-label { font-size: 12px; color: rgba(237, 243, 248, 0.4); text-transform: uppercase; letter-spacing: 1px; }"
        ".stat-val { font-size: 28px; font-weight: 700; color: #edf3f8; margin-top: 4px; }"
        ".game-canvas { flex-grow: 1; height: 100%; border-radius: 0px; }"
        ".instruction-card { background-color: rgba(199, 124, 255, 0.05); padding: 16px; border-radius: 8px; border: 1px solid rgba(199, 124, 255, 0.1); }"
        ".instruction-text { font-size: 13px; color: rgba(237, 243, 248, 0.7); line-height: 1.5; }"
    );

    auto* root = app.root();
    
    // Sidebar
    auto* sidebar = root->panel("sidebar");
    sidebar->text("3D VECTOR CORE", "side-title");
    
    sidebar->text("Interactions", "side-label");
    auto* instructions = sidebar->panel("instruction-card");
    instructions->text("Left-Click and drag to rotate core.\nLeft-Click anywhere to trigger a neon shockwave ripple!\nWatch the 3D space parallax stars.", "instruction-text");

    sidebar->text("Status", "side-label");
    sidebar->text("Core Online", "stat-val");

    // Game Canvas
    auto* canvas = root->add<Canvas>("game-canvas");
    canvas->onDraw = [](Renderer& r, const Rect& bounds) {
        drawHologram(nullptr, r, bounds);
    };

    app.onUpdate = [&](float dt) {
        updateHologram(app, dt);
        app.requestRedraw();
    };

    app.run();
    app.shutdown();
    return 0;
}
