// FluxUI — Video widget (HTMLVideoElement-style) implementation.
// Extracted from the monolithic core/application.cpp. Self-contained: it only
// depends on the public Widget API plus the shared detail:: paint gate.
#include "fluxui/widgets.h"
#include "../widget_internal.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace FluxUI {

// ============================================================
//  Video Widget Implementation
// ============================================================
#ifdef _WIN32
#include <mmsystem.h>
typedef MMRESULT(WINAPI* PFN_waveOutOpen)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT(WINAPI* PFN_waveOutPrepareHeader)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT(WINAPI* PFN_waveOutUnprepareHeader)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT(WINAPI* PFN_waveOutWrite)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT(WINAPI* PFN_waveOutClose)(HWAVEOUT);
typedef MMRESULT(WINAPI* PFN_waveOutReset)(HWAVEOUT);

struct Win32AudioEngine {
    HMODULE winmm = nullptr;
    HWAVEOUT hWaveOut = nullptr;
    PFN_waveOutOpen pWaveOutOpen = nullptr;
    PFN_waveOutPrepareHeader pWaveOutPrepareHeader = nullptr;
    PFN_waveOutUnprepareHeader pWaveOutUnprepareHeader = nullptr;
    PFN_waveOutWrite pWaveOutWrite = nullptr;
    PFN_waveOutClose pWaveOutClose = nullptr;
    PFN_waveOutReset pWaveOutReset = nullptr;

    bool init() {
        if (winmm) return true;
        winmm = LoadLibraryA("winmm.dll");
        if (!winmm) return false;
        pWaveOutOpen = (PFN_waveOutOpen)GetProcAddress(winmm, "waveOutOpen");
        pWaveOutPrepareHeader = (PFN_waveOutPrepareHeader)GetProcAddress(winmm, "waveOutPrepareHeader");
        pWaveOutUnprepareHeader = (PFN_waveOutUnprepareHeader)GetProcAddress(winmm, "waveOutUnprepareHeader");
        pWaveOutWrite = (PFN_waveOutWrite)GetProcAddress(winmm, "waveOutWrite");
        pWaveOutClose = (PFN_waveOutClose)GetProcAddress(winmm, "waveOutClose");
        pWaveOutReset = (PFN_waveOutReset)GetProcAddress(winmm, "waveOutReset");
        return pWaveOutOpen && pWaveOutPrepareHeader && pWaveOutUnprepareHeader && pWaveOutWrite && pWaveOutClose && pWaveOutReset;
    }

    ~Win32AudioEngine() {
        close();
        if (winmm) {
            FreeLibrary(winmm);
        }
    }

    void close() {
        if (hWaveOut) {
            pWaveOutReset(hWaveOut);
            pWaveOutClose(hWaveOut);
            hWaveOut = nullptr;
        }
    }
};
#endif

Video::Video() {
    type = "video";
    style.cursor = CursorType::Default;
    lastUpdateTime_ = std::chrono::high_resolution_clock::now();
    networkState = NETWORK_EMPTY;
    readyState = HAVE_NOTHING;
    seeking = false;
}

Video::~Video() {
    stopAudioThread();
}

void Video::play() {
    if (!paused) return;
    paused = false;
    if (networkState == NETWORK_EMPTY && !source.empty()) {
        networkState = NETWORK_IDLE;
        readyState = HAVE_ENOUGH_DATA;
    }
    lastUpdateTime_ = std::chrono::high_resolution_clock::now();
    startAudioThread();
    if (onPlay) onPlay();
}

void Video::pause() {
    if (paused) return;
    paused = true;
    stopAudioThread();
    if (onPause) onPause();
}

void Video::setMuted(bool m) {
    bool oldMuted = muted;
    muted = m;
    if (oldMuted != muted && onVolumeChange) {
        onVolumeChange();
    }
}

void Video::setVolume(float v) {
    float oldVol = volume;
    volume = std::clamp(v, 0.0f, 1.0f);
    if (oldVol != volume && onVolumeChange) {
        onVolumeChange();
    }
}

void Video::setCurrentTime(float t) {
    float targetTime = std::clamp(t, 0.0f, duration);
    if (targetTime != currentTime) {
        seeking = true;
        if (onSeeking) onSeeking();

        currentTime = targetTime;

        seeking = false;
        if (onSeeked) onSeeked();
        if (onTimeUpdate) onTimeUpdate();
    }
}

void Video::layout(const Rect& parentBounds) {
    Widget::layout(parentBounds);
    const Style& s = *computedStyle;
    if (!s.width.isSet()) {
        bounds.w = 300.0f;
    }
    if (!s.height.isSet()) {
        bounds.h = 150.0f;
    }
}

#ifdef _WIN32
void Video::startAudioThread() {
    std::lock_guard<std::mutex> lock(audioMutex_);
    if (audioThreadRunning_) return;
    audioThreadRunning_ = true;
    audioThread_ = std::thread([this]() {
        Win32AudioEngine engine;
        if (!engine.init()) return;

        WAVEFORMATEX wfx;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;
        wfx.nSamplesPerSec = 44100;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = 2;
        wfx.nAvgBytesPerSec = 44100 * 2;
        wfx.cbSize = 0;

        if (engine.pWaveOutOpen(&engine.hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            return;
        }

        const int BUF_SIZE = 2048;
        int16_t bufData[2][BUF_SIZE];
        WAVEHDR headers[2];
        memset(headers, 0, sizeof(headers));
        for (int i = 0; i < 2; ++i) {
            headers[i].lpData = (char*)bufData[i];
            headers[i].dwBufferLength = BUF_SIZE * 2;
        }

        int currentBuf = 0;
        double phase = 0.0;
        double arpeggioTimer = 0.0;
        int arpeggioIndex = 0;
        double frequencies[] = { 261.63, 293.66, 329.63, 349.23, 392.00, 440.00 }; // C, D, E, F, G, A
        int numFreqs = 6;

        engine.pWaveOutPrepareHeader(engine.hWaveOut, &headers[0], sizeof(WAVEHDR));
        engine.pWaveOutPrepareHeader(engine.hWaveOut, &headers[1], sizeof(WAVEHDR));
        headers[0].dwFlags |= WHDR_DONE;
        headers[1].dwFlags |= WHDR_DONE;

        while (true) {
            {
                std::lock_guard<std::mutex> innerLock(audioMutex_);
                if (!audioThreadRunning_) break;
            }

            WAVEHDR& hdr = headers[currentBuf];
            while (!(hdr.dwFlags & WHDR_DONE)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                std::lock_guard<std::mutex> innerLock(audioMutex_);
                if (!audioThreadRunning_) break;
            }

            {
                std::lock_guard<std::mutex> innerLock(audioMutex_);
                if (!audioThreadRunning_) break;
            }

            float curVol = volume;
            bool isMuted = muted;
            bool isPaused = paused;

            double freq = frequencies[arpeggioIndex];
            if (isMuted || isPaused) {
                freq = 0.0;
            }

            for (int i = 0; i < BUF_SIZE; ++i) {
                double sample = 0.0;
                if (freq > 0.0) {
                    sample = std::sin(phase);
                    sample += 0.25 * std::sin(phase * 2.0);
                    sample += 0.1 * std::sin(phase * 3.0);
                    sample *= 0.25;
                }
                int16_t intSample = (int16_t)(sample * curVol * 32767.0);
                bufData[currentBuf][i] = intSample;
                phase += (2.0 * 3.141592653589793 * freq) / 44100.0;
                if (phase > 2.0 * 3.141592653589793) {
                    phase -= 2.0 * 3.141592653589793;
                }
            }

            if (!isPaused) {
                arpeggioTimer += (double)BUF_SIZE / 44100.0;
                if (arpeggioTimer >= 0.2) {
                    arpeggioTimer = 0.0;
                    arpeggioIndex = (arpeggioIndex + 1) % numFreqs;
                }
            }

            hdr.dwFlags &= ~WHDR_DONE;
            engine.pWaveOutWrite(engine.hWaveOut, &hdr, sizeof(WAVEHDR));
            currentBuf = 1 - currentBuf;
        }

        engine.pWaveOutReset(engine.hWaveOut);
        engine.pWaveOutUnprepareHeader(engine.hWaveOut, &headers[0], sizeof(WAVEHDR));
        engine.pWaveOutUnprepareHeader(engine.hWaveOut, &headers[1], sizeof(WAVEHDR));
    });
}

void Video::stopAudioThread() {
    {
        std::lock_guard<std::mutex> lock(audioMutex_);
        if (!audioThreadRunning_) return;
        audioThreadRunning_ = false;
    }
    if (audioThread_.joinable()) {
        audioThread_.join();
    }
}
#else
void Video::startAudioThread() {}
void Video::stopAudioThread() {}
#endif

void Video::update(const InputState& input) {
    Widget::update(input);

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - lastUpdateTime_).count();
    lastUpdateTime_ = now;

    if (!paused) {
        float nextTime = currentTime + dt * playbackRate;
        if (nextTime >= duration) {
            if (loop) {
                setCurrentTime(0.0f);
            } else {
                setCurrentTime(duration);
                paused = true;
                stopAudioThread();
                if (onEnded) onEnded();
            }
        } else {
            setCurrentTime(nextTime);
        }
    }

    if (hovered) {
        controlsTimer_ = 3.0f;
    } else if (controlsTimer_ > 0.0f) {
        controlsTimer_ -= dt;
    }

    // Controls hitboxes & interactions
    float mx = input.mousePos.x - bounds.x;
    float my = input.mousePos.y - bounds.y;

    if (controls && controlsTimer_ > 0.0f) {
        playButtonHovered_ = (mx >= 10.0f && mx <= 34.0f && my >= bounds.h - 32.0f && my <= bounds.h - 8.0f);
        volumeButtonHovered_ = (mx >= bounds.w - 80.0f && mx <= bounds.w - 56.0f && my >= bounds.h - 32.0f && my <= bounds.h - 8.0f);
        progressBarHovered_ = (mx >= 40.0f && mx <= bounds.w - 90.0f && my >= bounds.h - 35.0f && my <= bounds.h - 27.0f);
        volumeSliderHovered_ = (mx >= bounds.w - 50.0f && mx <= bounds.w - 10.0f && my >= bounds.h - 26.0f && my <= bounds.h - 18.0f);

        if (input.mouseClicked[0]) {
            if (playButtonHovered_) {
                if (paused) play();
                else pause();
            } else if (volumeButtonHovered_) {
                setMuted(!muted);
            } else if (progressBarHovered_) {
                draggingProgress_ = true;
            } else if (volumeSliderHovered_) {
                draggingVolume_ = true;
            }
        }

        if (draggingProgress_ && input.mouseDown[0]) {
            float frac = (input.mousePos.x - (bounds.x + 40.0f)) / (bounds.w - 130.0f);
            setCurrentTime(duration * std::clamp(frac, 0.0f, 1.0f));
        }

        if (draggingVolume_ && input.mouseDown[0]) {
            float frac = (input.mousePos.x - (bounds.x + bounds.w - 50.0f)) / 40.0f;
            setVolume(std::clamp(frac, 0.0f, 1.0f));
            if (muted && volume > 0.0f) setMuted(false);
        }

        if (input.mouseReleased[0]) {
            draggingProgress_ = false;
            draggingVolume_ = false;
        }
    }
}

void Video::render(Renderer& renderer) {
    if (!detail::canPaintWidget(this)) return;
    renderBackground(renderer);

    // Dynamic color visualization
    float r = 0.15f + 0.05f * std::sin(currentTime * 2.0f);
    float g = 0.08f + 0.05f * std::cos(currentTime * 1.5f);
    float b = 0.2f + 0.05f * std::sin(currentTime * 1.0f);
    renderer.drawRoundedRect(bounds, Color(r, g, b, 1.0f), BorderRadius(0.0f));

    // Bouncing spectrum bars
    float barW = (bounds.w * 0.6f) / 15.0f;
    for (int i = 0; i < 15; ++i) {
        float factor = 0.5f + 0.5f * std::sin(currentTime * (2.0f + i * 0.5f));
        if (paused) factor = 0.05f;
        float barH = bounds.h * 0.25f * factor;
        renderer.drawRoundedRect({bounds.x + bounds.w * 0.2f + i * barW + 2.0f, bounds.y + bounds.h * 0.7f - barH, barW - 4.0f, barH}, Color::fromHex("#6C5CE7"), BorderRadius(2.0f));
    }

    // 3D wireframe double pyramid center visualization
    struct Point3D { float x, y, z; };
    Point3D vertices[] = {
        {0, 1.2f, 0}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {0, -1.2f, 0}
    };
    int edges[][2] = {
        {0, 1}, {0, 2}, {0, 3}, {0, 4},
        {1, 2}, {2, 3}, {3, 4}, {4, 1},
        {5, 1}, {5, 2}, {5, 3}, {5, 4}
    };
    float rotY = currentTime * 1.5f;
    float rotX = currentTime * 0.8f;
    Vec2 center = {bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.35f};
    float scale = std::min(bounds.w, bounds.h) * 0.15f;

    for (int i = 0; i < 12; ++i) {
        Point3D p1 = vertices[edges[i][0]];
        Point3D p2 = vertices[edges[i][1]];

        float y1_1 = p1.y * std::cos(rotX) - p1.z * std::sin(rotX);
        float z1_1 = p1.y * std::sin(rotX) + p1.z * std::cos(rotX);
        float x1_2 = p1.x * std::cos(rotY) + z1_1 * std::sin(rotY);

        float y2_1 = p2.y * std::cos(rotX) - p2.z * std::sin(rotX);
        float z2_1 = p2.y * std::sin(rotX) + p2.z * std::cos(rotX);
        float x2_2 = p2.x * std::cos(rotY) + z2_1 * std::sin(rotY);

        renderer.drawRoundedRect({center.x + x1_2 * scale - 1.0f, center.y + y1_1 * scale - 1.0f, 2.0f, 2.0f}, Color::fromHex("#00CEC9"), BorderRadius(1.0f));
    }

    // Top-left text overlay
    renderer.drawText("Blink HTMLVideoElement: " + (source.empty() ? "No Source" : source),
                      {bounds.x + 10.0f, bounds.y + 20.0f},
                      Color(1.0f, 1.0f, 1.0f, 0.8f), 12.0f);

    // Controls Overlay rendering
    if (controls && controlsTimer_ > 0.0f) {
        float alpha = std::clamp(controlsTimer_, 0.0f, 1.0f);
        Rect ctrlRect = { bounds.x, bounds.y + bounds.h - 40.0f, bounds.w, 40.0f };
        renderer.drawRoundedRect(ctrlRect, Color(0.05f, 0.05f, 0.05f, 0.8f * alpha), BorderRadius(0.0f));

        Rect track = { bounds.x + 40.0f, bounds.y + bounds.h - 33.0f, bounds.w - 130.0f, 4.0f };
        renderer.drawRoundedRect(track, Color(0.3f, 0.3f, 0.3f, alpha), BorderRadius(2.0f));

        float progressFrac = currentTime / duration;
        renderer.drawRoundedRect({track.x, track.y, track.w * progressFrac, track.h}, Color::fromHex("#1a73e8"), BorderRadius(2.0f));

        if (progressBarHovered_ || draggingProgress_) {
            renderer.drawRoundedRect({track.x + track.w * progressFrac - 4.0f, track.y - 2.0f, 8.0f, 8.0f}, Color::fromHex("#1a73e8"), BorderRadius(4.0f));
        }

        Color playColor = playButtonHovered_ ? Color(1.0f, 1.0f, 1.0f, alpha) : Color(0.8f, 0.8f, 0.8f, alpha);
        renderer.drawText(paused ? "▶" : "⏸", {bounds.x + 15.0f, bounds.y + bounds.h - 26.0f}, playColor, 14.0f);

        auto formatTime = [](float sec) {
            int m = (int)(sec / 60.0f);
            int s = (int)std::fmod(sec, 60.0f);
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
            return std::string(buf);
        };
        std::string timeStr = formatTime(currentTime) + " / " + formatTime(duration);
        renderer.drawText(timeStr, {bounds.x + 40.0f, bounds.y + bounds.h - 15.0f}, Color(0.9f, 0.9f, 0.9f, alpha), 10.0f);

        Color volColor = volumeButtonHovered_ ? Color(1.0f, 1.0f, 1.0f, alpha) : Color(0.8f, 0.8f, 0.8f, alpha);
        renderer.drawText(muted ? "🔇" : (volume > 0.5f ? "🔊" : "🔉"), {bounds.x + bounds.w - 80.0f, bounds.y + bounds.h - 26.0f}, volColor, 14.0f);

        Rect volSlider = { bounds.x + bounds.w - 50.0f, bounds.y + bounds.h - 23.0f, 40.0f, 4.0f };
        renderer.drawRoundedRect(volSlider, Color(0.3f, 0.3f, 0.3f, alpha), BorderRadius(2.0f));
        renderer.drawRoundedRect({volSlider.x, volSlider.y, volSlider.w * (muted ? 0.0f : volume), volSlider.h}, Color::fromHex("#1a73e8"), BorderRadius(2.0f));
    }
}

void Video::setAttribute(const std::string& name, const std::string& value) {
    Widget::setAttribute(name, value);
    if (name == "src" || name == "source") {
        source = value;
        if (!source.empty()) {
            networkState = NETWORK_LOADING;
            readyState = HAVE_METADATA;
            // Transition quickly to simulated ready state
            networkState = NETWORK_IDLE;
            readyState = HAVE_ENOUGH_DATA;
        } else {
            networkState = NETWORK_EMPTY;
            readyState = HAVE_NOTHING;
        }
    } else if (name == "autoplay") {
        autoplay = (value == "true" || value == "autoplay" || value == "1");
        if (autoplay) play();
    } else if (name == "loop") {
        loop = (value == "true" || value == "loop" || value == "1");
    } else if (name == "muted") {
        setMuted(value == "true" || value == "muted" || value == "1");
    } else if (name == "volume") {
        try {
            setVolume(std::stof(value));
        } catch (...) {}
    } else if (name == "controls") {
        controls = (value != "false" && value != "0");
    }
}

} // namespace FluxUI
