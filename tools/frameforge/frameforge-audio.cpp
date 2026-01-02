#include "frameforge-audio.h"

#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT
#include <portaudio.h>
#endif

#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>

namespace frameforge {

#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT

// Global PortAudio initialization tracking
static bool g_portaudio_initialized = false;
static int  g_portaudio_ref_count   = 0;

AudioCapture::AudioCapture(const AudioConfig & config)
    : config_(config)
    , callback_(nullptr)
    , capturing_(false)
    , stream_(nullptr)
    , ready_to_process_(false)
    , has_speech_(false)
    , speech_sample_count_(0)
    , silence_sample_count_(0) {
    // Calculate sample thresholds
    min_speech_samples_ = static_cast<size_t>(
        (config_.min_speech_duration_ms / 1000.0f) * config_.sample_rate
    );
    silence_samples_threshold_ = static_cast<size_t>(
        (config_.silence_duration_ms / 1000.0f) * config_.sample_rate
    );
}

AudioCapture::~AudioCapture() {
    stop();
    if (stream_) {
        Pa_CloseStream(static_cast<PaStream *>(stream_));
        stream_ = nullptr;
    }
    // Note: We don't call Pa_Terminate() here as it affects global PortAudio state
    // In a production application, Pa_Terminate() should be called once at application shutdown
}

bool AudioCapture::initialize() {
    // Initialize PortAudio if not already initialized
    if (!g_portaudio_initialized) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }
        g_portaudio_initialized = true;
    }
    g_portaudio_ref_count++;

    // Get default input device
    PaDeviceIndex device = Pa_GetDefaultInputDevice();
    if (device == paNoDevice) {
        std::cerr << "Error: No default input device found" << std::endl;
        return false;
    }

    // Print device info
    const PaDeviceInfo * device_info = Pa_GetDeviceInfo(device);
    if (device_info) {
        std::cout << "Using audio device: " << device_info->name << std::endl;
        std::cout << "  Sample rate: " << config_.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << config_.channels << std::endl;
    }

    // Set up stream parameters
    PaStreamParameters input_params;
    input_params.device                    = device;
    input_params.channelCount              = config_.channels;
    input_params.sampleFormat              = paFloat32;
    input_params.suggestedLatency          = device_info->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    // Open audio stream
    PaError err = Pa_OpenStream(
        reinterpret_cast<PaStream **>(&stream_), &input_params,
        nullptr,  // no output
        config_.sample_rate, config_.frames_per_buffer, paClipOff, 
        reinterpret_cast<PaStreamCallback *>(pa_callback), this);

    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    return true;
}

bool AudioCapture::start() {
    if (!stream_) {
        std::cerr << "Error: Audio stream not initialized" << std::endl;
        return false;
    }

    if (capturing_) {
        return true;  // Already capturing
    }

    PaError err = Pa_StartStream(static_cast<PaStream *>(stream_));
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    capturing_ = true;
    std::cout << "Audio capture started" << std::endl;
    return true;
}

void AudioCapture::stop() {
    if (!capturing_) {
        return;
    }

    if (stream_) {
        PaError err = Pa_StopStream(static_cast<PaStream *>(stream_));
        if (err != paNoError) {
            std::cerr << "PortAudio error when stopping stream: " << Pa_GetErrorText(err) << std::endl;
            return;
        }
    }

    capturing_ = false;
    std::cout << "Audio capture stopped" << std::endl;
}

void AudioCapture::set_callback(AudioCallback callback) {
    callback_ = callback;
}

std::vector<float> AudioCapture::get_audio_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    std::vector<float> buffer_copy;
    buffer_copy.swap(audio_buffer_);
    return buffer_copy;
}

void AudioCapture::clear_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    audio_buffer_.clear();
}

void AudioCapture::reset_vad_state() {
    std::lock_guard<std::mutex> lock(vad_mutex_);
    ready_to_process_ = false;
    has_speech_ = false;
    speech_sample_count_ = 0;
    silence_sample_count_ = 0;
}

float AudioCapture::calculate_rms(const float * data, size_t sample_count) const {
    if (!data || sample_count == 0) {
        return 0.0f;
    }
    
    float sum_squares = 0.0f;
    for (size_t i = 0; i < sample_count; ++i) {
        sum_squares += data[i] * data[i];
    }
    
    return std::sqrt(sum_squares / sample_count);
}

bool AudioCapture::is_speech(const float * data, size_t sample_count) const {
    float rms = calculate_rms(data, sample_count);
    return rms > config_.vad_threshold;
}

int AudioCapture::pa_callback(const void * input, void * output, unsigned long frame_count,
                              const void * time_info, unsigned long status_flags, void * user_data) {
    (void) output;
    (void) time_info;
    (void) status_flags;

    AudioCapture * capture = static_cast<AudioCapture *>(user_data);
    const float *  in      = static_cast<const float *>(input);

    if (in && capture) {
        capture->handle_audio_data(in, frame_count);
    }

    return paContinue;
}

void AudioCapture::handle_audio_data(const float * data, unsigned long frame_count) {
    if (!data || frame_count == 0) {
        return;
    }

    // Calculate total samples (frames * channels)
    size_t total_samples = frame_count * config_.channels;

    // Store in buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        audio_buffer_.insert(audio_buffer_.end(), data, data + total_samples);
    }

    // Perform voice activity detection
    bool current_is_speech = is_speech(data, total_samples);
    
    // Update VAD state atomically
    {
        std::lock_guard<std::mutex> lock(vad_mutex_);
        
        if (current_is_speech) {
            // We have speech
            speech_sample_count_ += total_samples;
            silence_sample_count_ = 0;  // Reset silence counter
            
            // Mark that we've detected speech
            if (speech_sample_count_ >= min_speech_samples_) {
                has_speech_ = true;
            }
        } else {
            // We have silence
            if (has_speech_) {
                // We had speech before, now counting silence
                silence_sample_count_ += total_samples;
                
                // Check if we've had enough silence to trigger processing
                if (silence_sample_count_ >= silence_samples_threshold_) {
                    ready_to_process_ = true;
                }
            }
            // If we don't have speech yet, keep resetting counters
            else {
                speech_sample_count_ = 0;
                silence_sample_count_ = 0;
            }
        }
    }

    // Call user callback if set
    if (callback_) {
        std::vector<float> callback_data(data, data + total_samples);
        callback_(callback_data);
    }
}

#endif  // FRAMEFORGE_PORTAUDIO_SUPPORT

}  // namespace frameforge
