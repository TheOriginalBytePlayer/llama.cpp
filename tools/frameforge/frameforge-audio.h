#ifndef FRAMEFORGE_AUDIO_H
#define FRAMEFORGE_AUDIO_H

#include <functional>
#include <vector>
#include <atomic>
#include <mutex>

namespace frameforge {

// Audio capture configuration
struct AudioConfig {
    int sample_rate = 16000;     // Sample rate in Hz (16kHz is standard for Whisper)
    int channels = 1;            // Number of channels (1 = mono)
    int frames_per_buffer = 512; // Number of frames per buffer
};

// Audio capture callback function type
// Called when audio data is available
// Parameters: PCM float data, number of samples
using AudioCallback = std::function<void(const std::vector<float> &)>;

#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT

// Audio capture class using PortAudio
class AudioCapture {
public:
    AudioCapture(const AudioConfig & config = AudioConfig());
    ~AudioCapture();

    // Initialize the audio capture system
    bool initialize();

    // Start capturing audio
    bool start();

    // Stop capturing audio
    void stop();

    // Check if currently capturing
    bool is_capturing() const { return capturing_; }

    // Set callback for audio data
    void set_callback(AudioCallback callback);

    // Get captured audio buffer (for accumulated audio)
    std::vector<float> get_audio_buffer();

    // Clear the audio buffer
    void clear_buffer();

private:
    AudioConfig config_;
    AudioCallback callback_;
    std::atomic<bool> capturing_;
    std::vector<float> audio_buffer_;
    std::mutex buffer_mutex_;
    void * stream_;  // PaStream* (opaque pointer to avoid including portaudio.h here)

    // PortAudio callback (static function)
    static int pa_callback(const void * input, void * output, unsigned long frame_count,
                          const void * time_info, unsigned long status_flags, void * user_data);

    // Instance callback handler
    void handle_audio_data(const float * data, unsigned long frame_count);
};

#else

// Stub implementation when PortAudio is not available
class AudioCapture {
public:
    AudioCapture(const AudioConfig & config = AudioConfig()) { (void) config; }
    ~AudioCapture() {}

    bool initialize() { return false; }
    bool start() { return false; }
    void stop() {}
    bool is_capturing() const { return false; }
    void set_callback(AudioCallback callback) { (void) callback; }
    std::vector<float> get_audio_buffer() { return std::vector<float>(); }
    void clear_buffer() {}
};

#endif

} // namespace frameforge

#endif // FRAMEFORGE_AUDIO_H
