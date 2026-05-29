#include "fluxui/FluxUI.h"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    std::cout << "Running Video widget programmatic tests..." << std::endl;

    // Create an instance of the Video widget
    FluxUI::Video video;

    // Test Initial State
    assert(video.type == "video");
    assert(video.paused == true);
    assert(video.muted == false);
    assert(std::abs(video.volume - 1.0f) < 1e-5);
    assert(video.currentTime == 0.0f);
    assert(video.duration == 60.0f);
    assert(video.playbackRate == 1.0f);
    assert(video.controls == true);
    std::cout << "Initial state tests passed." << std::endl;

    // Test State Transitions & Setters
    bool playCallbackTriggered = false;
    video.onPlay = [&]() { playCallbackTriggered = true; };
    video.play();
    assert(video.paused == false);
    assert(playCallbackTriggered == true);
    std::cout << "Play state and callback tests passed." << std::endl;

    bool pauseCallbackTriggered = false;
    video.onPause = [&]() { pauseCallbackTriggered = true; };
    video.pause();
    assert(video.paused == true);
    assert(pauseCallbackTriggered == true);
    std::cout << "Pause state and callback tests passed." << std::endl;

    video.setVolume(0.75f);
    assert(std::abs(video.volume - 0.75f) < 1e-5);
    // Test clamp volume high
    video.setVolume(1.5f);
    assert(std::abs(video.volume - 1.0f) < 1e-5);
    // Test clamp volume low
    video.setVolume(-0.5f);
    assert(std::abs(video.volume - 0.0f) < 1e-5);
    std::cout << "Volume setter and clamping tests passed." << std::endl;

    video.setMuted(true);
    assert(video.muted == true);
    video.setMuted(false);
    assert(video.muted == false);
    std::cout << "Mute setter tests passed." << std::endl;

    bool timeUpdateTriggered = false;
    video.onTimeUpdate = [&]() { timeUpdateTriggered = true; };
    video.setCurrentTime(15.0f);
    assert(std::abs(video.currentTime - 15.0f) < 1e-5);
    assert(timeUpdateTriggered == true);
    // Test clamp currentTime high
    video.setCurrentTime(80.0f);
    assert(std::abs(video.currentTime - video.duration) < 1e-5);
    // Test clamp currentTime low
    video.setCurrentTime(-5.0f);
    assert(std::abs(video.currentTime - 0.0f) < 1e-5);
    std::cout << "Seek/Time setter, callback, and clamping tests passed." << std::endl;

    // Test advanced HTML5 states and callbacks
    assert(video.networkState == FluxUI::Video::NETWORK_EMPTY);
    assert(video.readyState == FluxUI::Video::HAVE_NOTHING);

    bool seekingCallback = false;
    bool seekedCallback = false;
    video.onSeeking = [&]() { seekingCallback = true; };
    video.onSeeked = [&]() { seekedCallback = true; };

    video.setCurrentTime(20.0f);
    assert(seekingCallback == true);
    assert(seekedCallback == true);
    assert(video.seeking == false);
    std::cout << "HTML5 seeking/seeked states and callbacks passed." << std::endl;

    bool volumeCallback = false;
    video.onVolumeChange = [&]() { volumeCallback = true; };
    video.setVolume(0.5f);
    assert(volumeCallback == true);
    volumeCallback = false;
    video.setMuted(true);
    assert(volumeCallback == true);
    std::cout << "HTML5 volumechange callbacks passed." << std::endl;

    // Test Attributes parsing
    video.setAttribute("src", "procedural_ocean.mp4");
    assert(video.source == "procedural_ocean.mp4");
    assert(video.networkState == FluxUI::Video::NETWORK_IDLE);
    assert(video.readyState == FluxUI::Video::HAVE_ENOUGH_DATA);

    video.setAttribute("autoplay", "true");
    assert(video.autoplay == true);

    video.setAttribute("loop", "true");
    assert(video.loop == true);

    video.setAttribute("muted", "true");
    assert(video.muted == true);

    video.setAttribute("volume", "0.4");
    assert(std::abs(video.volume - 0.4f) < 1e-5);

    video.setAttribute("controls", "false");
    assert(video.controls == false);
    std::cout << "HTML5-like attributes setAttribute tests passed." << std::endl;

    // Test Playback Updates via lifecycle loop
    video.setCurrentTime(0.0f);
    video.play();
    assert(video.paused == false);

    FluxUI::InputState input;
    // Simulate elapsed time update loop
    video.update(input);
    
    // Simulate time progression: 5 seconds elapsed
    // We mock the delta time logic inside update by using std::this_thread::sleep_for
    // or we can test the clock subtraction.
    // Let's sleep for 50 milliseconds to allow clock duration to capture difference.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    video.update(input);
    assert(video.currentTime > 0.0f);
    std::cout << "Real-time update tick and clock integration tests passed." << std::endl;

    // Test playback termination on ended
    video.loop = false;
    video.setCurrentTime(video.duration - 0.01f);
    bool endedCallbackTriggered = false;
    video.onEnded = [&]() { endedCallbackTriggered = true; };
    
    // Perform update tick to trigger ending
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    video.update(input);
    assert(video.paused == true);
    assert(std::abs(video.currentTime - video.duration) < 1e-5);
    assert(endedCallbackTriggered == true);
    std::cout << "Playback end event and loop disabled termination tests passed." << std::endl;

    std::cout << "\n==============================================" << std::endl;
    std::cout << "All Video Widget unit tests passed successfully!" << std::endl;
    std::cout << "==============================================" << std::endl;

    return 0;
}
