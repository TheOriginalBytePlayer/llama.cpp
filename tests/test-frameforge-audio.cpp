#include "../tools/frameforge/frameforge-audio.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace frameforge;

static void test_audio_config() {
    std::cout << "Testing audio configuration..." << std::endl;
    
    AudioConfig config;
    assert(config.sample_rate == 16000);
    assert(config.channels == 1);
    assert(config.frames_per_buffer == 512);
    
    // Custom config
    AudioConfig custom;
    custom.sample_rate = 44100;
    custom.channels = 2;
    custom.frames_per_buffer = 1024;
    
    assert(custom.sample_rate == 44100);
    assert(custom.channels == 2);
    assert(custom.frames_per_buffer == 1024);
    
    std::cout << "  ✓ Audio configuration passed" << std::endl;
}

static void test_audio_capture_initialization() {
    std::cout << "Testing audio capture initialization..." << std::endl;
    
    AudioConfig config;
    AudioCapture capture(config);
    
    // Just test that we can create an instance
    assert(!capture.is_capturing());
    
#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT
    std::cout << "  PortAudio support is available" << std::endl;
    
    // Try to initialize
    bool init_result = capture.initialize();
    if (init_result) {
        std::cout << "  ✓ Audio capture initialization succeeded" << std::endl;
        
        // Follow typical usage pattern: start capturing before testing buffer operations.
        capture.start();

        // Test buffer operations
        capture.clear_buffer();
        std::vector<float> buffer = capture.get_audio_buffer();
        assert(buffer.empty());
        std::cout << "  ✓ Buffer operations work" << std::endl;

        // Stop capturing to complete the typical lifecycle.
        capture.stop();
    } else {
        std::cout << "  ! Audio capture initialization failed (this is OK if no audio device is available)" << std::endl;
    }
#else
    std::cout << "  PortAudio support is not available" << std::endl;
    std::cout << "  ✓ Stub implementation works" << std::endl;
#endif
}

static void test_audio_callback() {
    std::cout << "Testing audio callback..." << std::endl;
    
#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT
    AudioConfig config;
    AudioCapture capture(config);
    
    bool callback_called = false;
    capture.set_callback([&callback_called](const std::vector<float> & data) {
        callback_called = true;
        std::cout << "  Callback received " << data.size() << " samples" << std::endl;
    });
    
    if (capture.initialize()) {
        if (capture.start()) {
            std::cout << "  Audio capture started, waiting for callback..." << std::endl;
            
            // Wait for a short time to see if we get audio data
            auto start_time = std::chrono::steady_clock::now();
            while (!callback_called) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time
                ).count();
                
                if (elapsed > 2) {
                    std::cout << "  ! No callback received after 2 seconds (no audio input?)" << std::endl;
                    break;
                }
            }
            
            capture.stop();
            
            if (callback_called) {
                std::cout << "  ✓ Audio callback test passed" << std::endl;
            } else {
                std::cout << "  ! Audio callback test completed (no audio detected)" << std::endl;
            }
        } else {
            std::cout << "  ! Could not start audio capture" << std::endl;
        }
    } else {
        std::cout << "  ! Could not initialize audio capture" << std::endl;
    }
#else
    std::cout << "  PortAudio support not available, skipping callback test" << std::endl;
#endif
}

int main() {
    std::cout << "Running FrameForge Audio Capture Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    
    test_audio_config();
    test_audio_capture_initialization();
    test_audio_callback();
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}
