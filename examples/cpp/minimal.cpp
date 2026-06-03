// FluxUI Multimedia Parity Sandbox — modern HTML/Blink-named declarative DSL.
#include <fluxui/dsl.h>
#include <cstdio>
#include <string>
using namespace fluxui;

int main() {
    App app(960, 640, "FluxUI Multimedia Engine - Parity Sandbox");
    app.addCSS(
        ".root { display: flex; flex-direction: column; background-color: #0c0f12; padding: 40px; gap: 20px; align-items: center; justify-content: center; }"
        ".header-container { display: flex; flex-direction: column; align-items: center; gap: 6px; }"
        ".title { font-size: 28px; font-weight: 700; color: #ffffff; letter-spacing: 0.5px; }"
        ".subtitle { font-size: 14px; color: #a0aec0; text-align: center; max-width: 600px; }"
        ".video-container { display: flex; flex-direction: column; align-items: center; padding: 16px; background: rgba(255, 255, 255, 0.03); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 16px; box-shadow: 0 20px 40px rgba(0,0,0,0.6); }"
        ".video-player { width: 480px; height: 270px; border-radius: 8px; overflow: hidden; }"
        ".api-controls { display: flex; flex-direction: row; gap: 12px; margin-top: 16px; }"
        ".btn { padding: 8px 18px; height: 38px; border-radius: 6px; font-size: 13px; font-weight: 600; cursor: pointer; }"
        ".btn-primary { background-color: #6c5ce7; color: #ffffff; }"
        ".btn-secondary { background-color: rgba(255, 255, 255, 0.08); color: #ffffff; border: 1px solid rgba(255, 255, 255, 0.15); }"
        ".btn-danger { background-color: #ff7675; color: #ffffff; }"
        ".status-text { font-size: 13px; color: #718096; margin-top: 10px; }"
    );

    // The video player is shared between the reactive status text and the
    // control handlers. It is captured once the <video> element is mounted.
    auto player = std::make_shared<FluxUI::Video*>(nullptr);

    auto statusText = [player]() -> std::string {
        FluxUI::Video* v = *player;
        if (!v) return "State: Paused | Vol: 80% | Muted: No";
        char buf[160];
        snprintf(buf, sizeof(buf), "State: %s | Vol: %d%% | Muted: %s | Time: %.1f / %.1f",
                 v->paused ? "Paused" : "Playing",
                 (int)(v->volume * 100),
                 v->muted ? "Yes" : "No",
                 v->currentTime, v->duration);
        return buf;
    };

    app.setRoot(
        Div({
            Div({
                H1("Blink Multimedia Engine Parity Sandbox").className("title"),
                Span("Procedural frame renderer, real-time audio synthesis, and "
                     "interactive controls matching HTML5 <video> behavior.").className("subtitle")
            }).className("header-container"),

            Div({
                Video("high_fidelity_demo_stream.mp4")
                    .className("video-player")
                    .onMount<FluxUI::Video>([player](FluxUI::Video* v) {
                        v->duration = 120.0f;
                        v->volume = 0.8f;
                        v->paused = true;
                        v->controls = true;
                        *player = v;
                    }),

                Div({
                    Button("Play").className("btn btn-primary")
                        .onClick([player]{ if (*player) (*player)->play(); }),
                    Button("Pause").className("btn btn-secondary")
                        .onClick([player]{ if (*player) (*player)->pause(); }),
                    Button("Toggle Mute").className("btn btn-secondary")
                        .onClick([player]{ if (*player) (*player)->setMuted(!(*player)->muted); }),
                    Button("Vol +").className("btn btn-secondary")
                        .onClick([player]{ if (*player) (*player)->setVolume((*player)->volume + 0.1f); }),
                    Button("Vol -").className("btn btn-secondary")
                        .onClick([player]{ if (*player) (*player)->setVolume((*player)->volume - 0.1f); })
                }).className("api-controls"),

                Text(statusText).className("status-text")
            }).className("video-container"),

            Button("Exit Sandbox").className("btn btn-danger")
                .onClick([&app]{ app.raw().running = false; })
        }).className("root")
    );

    return app.run();
}
