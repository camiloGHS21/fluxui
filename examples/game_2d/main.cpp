#include "fluxui/FluxUI.h"
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace FluxUI;

struct Particle {
    Vec2 pos;
    Vec2 vel;
    Color color;
    float size;
    float lifetime;
    float maxLifetime;
};

struct Asteroid {
    Vec2 pos;
    float size;
    float speed;
    float angle;
    float rotSpeed;
};

struct Laser {
    Vec2 pos;
    float speed;
};

struct Star {
    Vec2 pos;
    float speed;
    float size;
};

class GameState {
public:
    Vec2 playerPos;
    float playerSize = 18.0f;
    std::vector<Asteroid> asteroids;
    std::vector<Laser> lasers;
    std::vector<Particle> particles;
    std::vector<Star> stars;
    int score = 0;
    int highScore = 0;
    bool gameOver = false;
    float time = 0.0f;
    float shootCooldown = 0.0f;

    void initStars() {
        stars.clear();
        for (int i = 0; i < 40; ++i) {
            stars.push_back({
                Vec2((float)(rand() % 900), (float)(rand() % 600)),
                20.0f + (float)(rand() % 80),
                1.0f + (float)(rand() % 2)
            });
        }
    }

    void reset() {
        playerPos = { 450.0f, 500.0f };
        asteroids.clear();
        lasers.clear();
        particles.clear();
        score = 0;
        gameOver = false;
        shootCooldown = 0.0f;
    }
};

static GameState state;

void updateGame(Application& app, float dt) {
    state.time += dt;

    // Update Stars (background parallax)
    for (auto& star : state.stars) {
        star.pos.y += star.speed * dt;
        if (star.pos.y > 600.0f) {
            star.pos.y = -10.0f;
            star.pos.x = (float)(rand() % 900);
        }
    }

    if (state.gameOver) {
        // Update particles only
        for (auto it = state.particles.begin(); it != state.particles.end(); ) {
            it->pos.x += it->vel.x * dt;
            it->pos.y += it->vel.y * dt;
            it->lifetime -= dt;
            if (it->lifetime <= 0.0f) {
                it = state.particles.erase(it);
            } else {
                ++it;
            }
        }
        return;
    }

    // Input handling: player position follows mouse X position smoothly
    Vec2 mousePos = app.input().mousePos;
    // Map mouse position within canvas bounds (say canvas is 900x600)
    // We restrict player spaceship to canvas boundaries
    float targetX = std::clamp(mousePos.x, 30.0f, 870.0f);
    state.playerPos.x += (targetX - state.playerPos.x) * 12.0f * dt;
    state.playerPos.y = 520.0f; // Fixed Y position

    // Shooting
    if (state.shootCooldown > 0.0f) {
        state.shootCooldown -= dt;
    }

    if (app.input().mouseDown[0] && state.shootCooldown <= 0.0f) {
        state.lasers.push_back({
            Vec2(state.playerPos.x, state.playerPos.y - 15.0f),
            500.0f
        });
        state.shootCooldown = 0.15f; // Fire rate cooldown
    }

    // Spawn asteroids
    if (rand() % 30 == 0 && state.asteroids.size() < 15) {
        state.asteroids.push_back({
            Vec2((float)(30 + rand() % 840), -40.0f),
            15.0f + (float)(rand() % 20),
            80.0f + (float)(rand() % 120),
            (float)(rand() % 360),
            (float)((rand() % 100 - 50) / 50.0f)
        });
    }

    // Update Lasers
    for (auto it = state.lasers.begin(); it != state.lasers.end(); ) {
        it->pos.y -= it->speed * dt;
        if (it->pos.y < -10.0f) {
            it = state.lasers.erase(it);
        } else {
            ++it;
        }
    }

    // Update Asteroids
    for (auto it = state.asteroids.begin(); it != state.asteroids.end(); ) {
        it->pos.y += it->speed * dt;
        it->angle += it->rotSpeed * dt;

        // Check laser collision
        bool asteroidDestroyed = false;
        for (auto lit = state.lasers.begin(); lit != state.lasers.end(); ) {
            float dx = it->pos.x - lit->pos.x;
            float dy = it->pos.y - lit->pos.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < (it->size + 4.0f)) {
                // Explode!
                for (int i = 0; i < 15; ++i) {
                    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                    float speed = 50.0f + (float)(rand() % 100);
                    state.particles.push_back({
                        it->pos,
                        Vec2(std::cos(angle) * speed, std::sin(angle) * speed),
                        Color::lerp(Color::fromHex("#FFAE5D"), Color::fromHex("#FF5D6C"), (float)rand() / RAND_MAX),
                        2.0f + (float)(rand() % 3),
                        0.6f + (float)rand() / RAND_MAX * 0.4f,
                        1.0f
                    });
                }
                state.score += 100;
                if (state.score > state.highScore) {
                    state.highScore = state.score;
                }
                lit = state.lasers.erase(lit);
                asteroidDestroyed = true;
                break;
            } else {
                ++lit;
            }
        }

        if (asteroidDestroyed) {
            it = state.asteroids.erase(it);
            continue;
        }

        // Check player collision
        float dx = it->pos.x - state.playerPos.x;
        float dy = it->pos.y - state.playerPos.y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < (state.playerSize + it->size - 4.0f)) {
            // Player explodes!
            state.gameOver = true;
            for (int i = 0; i < 40; ++i) {
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float speed = 80.0f + (float)(rand() % 150);
                state.particles.push_back({
                    state.playerPos,
                    Vec2(std::cos(angle) * speed, std::sin(angle) * speed),
                    Color::lerp(Color::fromHex("#37C6A3"), Color::fromHex("#6AA9FF"), (float)rand() / RAND_MAX),
                    3.0f + (float)(rand() % 4),
                    1.0f + (float)rand() / RAND_MAX * 0.5f,
                    1.5f
                });
            }
            break;
        }

        if (it->pos.y > 650.0f) {
            it = state.asteroids.erase(it);
        } else {
            ++it;
        }
    }

    // Update Particles
    for (auto it = state.particles.begin(); it != state.particles.end(); ) {
        it->pos.x += it->vel.x * dt;
        it->pos.y += it->vel.y * dt;
        it->lifetime -= dt;
        if (it->lifetime <= 0.0f) {
            it = state.particles.erase(it);
        } else {
            ++it;
        }
    }
}

void drawGame(Widget* canvas, Renderer& r, const Rect& bounds) {
    // Clear canvas background with dark slate space color
    r.drawRoundedRect(bounds, Color::fromHex("#080a0c"), BorderRadius(0.0f));

    // Draw Stars
    for (const auto& star : state.stars) {
        // Compute absolute position
        float ax = bounds.x + star.pos.x;
        float ay = bounds.y + star.pos.y;
        r.drawRoundedRect(Rect(ax, ay, star.size, star.size), Color(1, 1, 1, 0.4f), BorderRadius(0.5f));
    }

    // Draw Particles
    for (const auto& p : state.particles) {
        float alpha = p.lifetime / p.maxLifetime;
        float ax = bounds.x + p.pos.x;
        float ay = bounds.y + p.pos.y;
        r.drawRoundedRect(Rect(ax - p.size/2, ay - p.size/2, p.size, p.size), p.color.withAlpha(alpha), BorderRadius(p.size/2));
    }

    // Draw Lasers
    for (const auto& laser : state.lasers) {
        float ax = bounds.x + laser.pos.x;
        float ay = bounds.y + laser.pos.y;
        r.drawRoundedRect(Rect(ax - 2, ay - 8, 4, 16), Color::fromHex("#FF5D6C"), BorderRadius(2.0f));
    }

    // Draw Player Spaceship
    if (!state.gameOver) {
        float ax = bounds.x + state.playerPos.x;
        float ay = bounds.y + state.playerPos.y;

        // Draw futuristic fighter: main body (cyan glow) and wings
        r.drawRoundedRect(Rect(ax - 6, ay - 16, 12, 32), Color::fromHex("#37C6A3"), BorderRadius(3.0f)); // Fuselage
        r.drawRoundedRect(Rect(ax - 20, ay + 2, 40, 6), Color::fromHex("#6AA9FF"), BorderRadius(2.0f));  // Wings
        r.drawRoundedRect(Rect(ax - 3, ay - 20, 6, 8), Color::fromHex("#EDF3F8"), BorderRadius(1.0f));   // Nose cone
        
        // Thrust flame particles
        if (rand() % 2 == 0) {
            float flameSize = 6.0f + (float)(rand() % 6);
            r.drawRoundedRect(Rect(ax - 3, ay + 16, 6, flameSize), Color::fromHex("#FFB64B"), BorderRadius(2.0f));
        }
    }

    // Draw Asteroids
    for (const auto& ast : state.asteroids) {
        float ax = bounds.x + ast.pos.x;
        float ay = bounds.y + ast.pos.y;
        // Simple octagonal/rounded asteroid shape
        r.drawRoundedRect(Rect(ax - ast.size, ay - ast.size, ast.size*2, ast.size*2), Color::fromHex("#4b5662"), BorderRadius(ast.size * 0.4f));
        // Crashing details
        r.drawRoundedRect(Rect(ax - ast.size*0.4f, ay - ast.size*0.4f, ast.size*0.3f, ast.size*0.3f), Color::fromHex("#323b46"), BorderRadius(2.0f));
    }

    // Game Info Overlay (Neon glow texts)
    std::string scoreStr = "SCORE: " + std::to_string(state.score);
    std::string hiStr = "HI-SCORE: " + std::to_string(state.highScore);
    
    r.drawText(scoreStr, Vec2(bounds.x + 20.0f, bounds.y + 35.0f), Color::fromHex("#37C6A3"), 18.0f);
    r.drawText(hiStr, Vec2(bounds.x + bounds.w - 180.0f, bounds.y + 35.0f), Color::fromHex("#6AA9FF"), 18.0f);

    if (state.gameOver) {
        // Semi-transparent game over overlay
        r.drawRoundedRect(bounds, Color(0, 0, 0, 0.75f), BorderRadius(0.0f));
        r.drawText("MISSION FAILED", Vec2(bounds.x + bounds.w / 2.0f - 130.0f, bounds.y + bounds.h / 2.0f - 30.0f), Color::fromHex("#FF5D6C"), 32.0f);
        r.drawText("Click to Restart", Vec2(bounds.x + bounds.w / 2.0f - 80.0f, bounds.y + bounds.h / 2.0f + 20.0f), Color::fromHex("#EDF3F8"), 18.0f);
    }
}

int main() {
    Application app;
    // Set Vulkan backend for high-performance rendering
    app.setBackend(RenderBackendType::Vulkan);

    if (!app.init("FluxUI 2D Asteroid Shooter", 1200, 750)) {
        return 1;
    }

    state.initStars();
    state.reset();

    app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    app.addStylesheet(
        ".root { display: flex; flex-direction: row; background-color: #080a0c; color: #edf3f8; }"
        ".sidebar { width: 280px; height: 100%; background-color: #101418; padding: 24px; display: flex; flex-direction: column; gap: 20px; border-right: 1px solid #202830; }"
        ".side-title { font-size: 24px; font-weight: 700; color: #37C6A3; }"
        ".side-label { font-size: 12px; color: rgba(237, 243, 248, 0.4); text-transform: uppercase; letter-spacing: 1px; }"
        ".stat-val { font-size: 28px; font-weight: 700; color: #edf3f8; margin-top: 4px; }"
        ".game-canvas { flex-grow: 1; height: 100%; border-radius: 0px; }"
        ".instruction-card { background-color: rgba(237, 243, 248, 0.05); padding: 16px; border-radius: 8px; }"
        ".instruction-text { font-size: 13px; color: rgba(237, 243, 248, 0.7); line-height: 1.5; }"
    );

    auto* root = app.root();
    
    // Sidebar
    auto* sidebar = root->panel("sidebar");
    sidebar->text("NEON FLYER", "side-title");
    
    sidebar->text("Controls", "side-label");
    auto* instructions = sidebar->panel("instruction-card");
    instructions->text("Move mouse horizontally to fly.\nClick LEFT mouse button to fire cyan lasers.\nDestroy asteroids to earn score!", "instruction-text");

    // Live Stats
    sidebar->text("Active Engine", "side-label");
    sidebar->text("Vulkan GPU", "stat-val");

    // Game Canvas
    auto* canvas = root->add<Canvas>("game-canvas");
    canvas->onDraw = [](Renderer& r, const Rect& bounds) {
        drawGame(nullptr, r, bounds);
    };

    // Main Loop hooks
    app.onUpdate = [&](float dt) {
        updateGame(app, dt);
        
        // Handle Game restart
        if (state.gameOver && app.input().mouseClicked[0]) {
            state.reset();
        }

        app.requestRedraw();
    };

    app.run();
    app.shutdown();
    return 0;
}
