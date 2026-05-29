#include "fluxui/FluxUI.h"
#include <iostream>
#include <vector>

int main() {
    FluxUI::Application app;
    if (!app.init("FluxUI Multimedia Engine - Parity Sandbox", 960, 640)) {
        return 1;
    }

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{14.0f, 16.0f, 26.0f});
    app.renderer().releaseFontSources();
    
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #0c0f12; padding: 40px; gap: 20px; align-items: center; justify-content: center; }"
        ".header-container { display: flex; flex-direction: column; align-items: center; gap: 6px; }"
        ".title { font-size: 28px; font-weight: 700; color: #ffffff; letter-spacing: 0.5px; }"
        ".subtitle { font-size: 14px; color: #a0aec0; text-align: center; max-width: 600px; }"
        ".video-container { display: flex; flex-direction: column; align-items: center; padding: 16px; background: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 16px; box-shadow: 0 20px 40px rgba(0,0,0,0.6); }"
        ".video-player { width: 480px; height: 270px; border-radius: 8px; overflow: hidden; }"
        ".api-controls { display: flex; flex-direction: row; gap: 12px; margin-top: 16px; }"
        ".btn { padding: 8px 18px; height: 38px; border-radius: 6px; font-size: 13px; font-weight: 600; cursor: pointer; transition: all 0.2s; }"
        ".btn-primary { background-color: #6c5ce7; color: #ffffff; }"
        ".btn-secondary { background-color: rgba(255, 255, 255, 0.08); color: #ffffff; border: 1px solid rgba(255, 255, 255, 0.15); }"
        ".btn-danger { background-color: #ff7675; color: #ffffff; }"
        ".status-text { font-size: 13px; color: #718096; margin-top: 10px; }"
    );

    auto* root = app.root();
    
    // Header
    auto* header = root->element("div", "", "header-container");
    header->add<FluxUI::Text>("Blink Multimedia Engine Parity Sandbox", "title");
    header->add<FluxUI::Text>("Procedural frame renderer, real-time waveOut audio synthesis, and interactive controls matching HTML5 <video> behavior.", "subtitle");

    // Video Player Box
    auto* videoContainer = root->element("div", "", "video-container");
    auto* player = videoContainer->add<FluxUI::Video>();
    player->className = "video-player";
    player->source = "high_fidelity_demo_stream.mp4";
    player->duration = 120.0f; // 2 minutes duration
    player->volume = 0.8f;
    player->paused = true;
    player->controls = true; // Enable overlay controls

    // API Controls
    auto* controlsBox = videoContainer->element("div", "", "api-controls");
    auto* playBtn = controlsBox->add<FluxUI::Button>("Play", "btn btn-primary");
    auto* pauseBtn = controlsBox->add<FluxUI::Button>("Pause", "btn btn-secondary");
    auto* toggleMuteBtn = controlsBox->add<FluxUI::Button>("Toggle Mute", "btn btn-secondary");
    auto* volUpBtn = controlsBox->add<FluxUI::Button>("Vol +", "btn btn-secondary");
    auto* volDownBtn = controlsBox->add<FluxUI::Button>("Vol -", "btn btn-secondary");
    
    // Live Info
    auto* status = videoContainer->add<FluxUI::Text>("State: Paused | Vol: 80% | Muted: No", "status-text");

    // Interactivity logic
    auto updateStatus = [=]() {
        char buf[128];
        snprintf(buf, sizeof(buf), "State: %s | Vol: %d%% | Muted: %s | Time: %.1f / %.1f",
                 player->paused ? "Paused" : "Playing",
                 (int)(player->volume * 100),
                 player->muted ? "Yes" : "No",
                 player->currentTime,
                 player->duration);
        status->content = buf;
    };

    playBtn->onClick = [=, &app]() {
        player->play();
        updateStatus();
    };

    pauseBtn->onClick = [=, &app]() {
        player->pause();
        updateStatus();
    };

    toggleMuteBtn->onClick = [=, &app]() {
        player->setMuted(!player->muted);
        updateStatus();
    };

    volUpBtn->onClick = [=, &app]() {
        player->setVolume(player->volume + 0.1f);
        updateStatus();
    };

    volDownBtn->onClick = [=, &app]() {
        player->setVolume(player->volume - 0.1f);
        updateStatus();
    };

    // Keep status updated periodically
    player->onTimeUpdate = [=]() {
        updateStatus();
    };

    auto* closeBtn = root->add<FluxUI::Button>("Exit Sandbox", "btn btn-danger");
    closeBtn->onClick = [&]() { app.running = false; };

    app.run();
    app.shutdown();
    return 0;
}
