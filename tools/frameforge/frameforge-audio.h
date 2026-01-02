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
    float vad_threshold = 0.01f; // Voice activity detection threshold (RMS)
    float min_speech_duration_ms = 500.0f;   // Minimum speech duration in milliseconds
    float silence_duration_ms = 250.0f;      // Silence duration to trigger processing
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
    
    // Check if ready to process (speech detected + silence after)
    bool is_ready_to_process() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(vad_mutex_));
        return ready_to_process_;
    }
    
    // Reset VAD state
    void reset_vad_state();

private:
    AudioConfig config_;
    AudioCallback callback_;
    std::atomic<bool> capturing_;
    std::vector<float> audio_buffer_;
    std::mutex buffer_mutex_;
    void * stream_;  // PaStream* (opaque pointer to avoid including portaudio.h here)
    bool initialized_;  // Track if this instance has initialized PortAudio
    
    // VAD state tracking (protected by vad_mutex_)
    std::mutex vad_mutex_;
    bool ready_to_process_;
    bool has_speech_;
    size_t speech_sample_count_;
    size_t silence_sample_count_;
    size_t min_speech_samples_;
    size_t silence_samples_threshold_;

    // PortAudio callback (static function)
    // Uses void* for time_info to avoid including portaudio.h in the header
    static int pa_callback(const void * input, void * output, unsigned long frame_count,
                          const void * time_info, unsigned long status_flags, void * user_data);

    // Instance callback handler
    void handle_audio_data(const float * data, unsigned long frame_count);
    
    // Calculate RMS (root mean square) of audio data
    float calculate_rms(const float * data, size_t sample_count) const;
    
    // Check if audio chunk contains speech
    bool is_speech(const float * data, size_t sample_count) const;
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
    bool is_ready_to_process() const { return false; }
    void reset_vad_state() {}
};

#endif

} // namespace frameforge

#endif // FRAMEFORGE_AUDIO_H
