#include "frameforge-audio.h"

#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT
#include <portaudio.h>
#endif

#include <iostream>
#include <cstring>

namespace frameforge {

#ifdef FRAMEFORGE_PORTAUDIO_SUPPORT

AudioCapture::AudioCapture(const AudioConfig & config)
    : config_(config)
    , callback_(nullptr)
    , capturing_(false)
    , stream_(nullptr) {
}

AudioCapture::~AudioCapture() {
    stop();
    if (stream_) {
        Pa_CloseStream(static_cast<PaStream *>(stream_));
        stream_ = nullptr;
    }
    Pa_Terminate();
}

bool AudioCapture::initialize() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    // Get default input device
    PaDeviceIndex device = Pa_GetDefaultInputDevice();
    if (device == paNoDevice) {
        std::cerr << "Error: No default input device found" << std::endl;
        Pa_Terminate();
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
    err = Pa_OpenStream(
        reinterpret_cast<PaStream **>(&stream_), &input_params,
        nullptr,  // no output
        config_.sample_rate, config_.frames_per_buffer, paClipOff, pa_callback, this);

    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
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
        Pa_StopStream(static_cast<PaStream *>(stream_));
    }

    capturing_ = false;
    std::cout << "Audio capture stopped" << std::endl;
}

void AudioCapture::set_callback(AudioCallback callback) {
    callback_ = callback;
}

std::vector<float> AudioCapture::get_audio_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return audio_buffer_;
}

void AudioCapture::clear_buffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    audio_buffer_.clear();
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

    // Call user callback if set
    if (callback_) {
        std::vector<float> callback_data(data, data + total_samples);
        callback_(callback_data);
    }
}

#endif  // FRAMEFORGE_PORTAUDIO_SUPPORT

}  // namespace frameforge
