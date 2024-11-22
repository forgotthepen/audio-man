/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
*/

#include <chrono>
#include <utility> // swap

#include "audio_man.hpp"
#include "private/playback/playback.hpp"
#include "private/recording/recording.hpp"


AudioMan::AudioMan()
{
    impl_playback = new AudioPlayback{};
    impl_recording = new AudioRecording{};
}

AudioMan::AudioMan(AudioMan &&other)
{
    std::swap(impl_playback, other.impl_playback);
    std::swap(impl_recording, other.impl_recording);
}

AudioMan::~AudioMan()
{
    if (impl_playback) {
        delete impl_playback;
        impl_playback = nullptr;
    }
    
    if (impl_recording) {
        delete impl_recording;
        impl_recording = nullptr;
    }
}


AudioMan& AudioMan::operator=(AudioMan &&other)
{
    if (&other != this) {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}


AudioRequest::AudioRequest(AudioRequestImpl *ptr)
{
    impl = ptr;
    if (ptr) {
        future_result = ptr->FutureResult();
    }
}

bool AudioRequest::IsValid() const
{
    return nullptr != impl && future_result.valid();
}

std::shared_future<bool> AudioRequest::FutureResult() const
{
    return future_result;
}

bool AudioRequest::Wait() const
{
    return future_result.valid() ? future_result.get() : false;
}

void AudioRequest::Cancel() const
{
    if (IsValid()) {
        auto is_still_running = future_result.wait_for(std::chrono::seconds(0)) == std::future_status::timeout;
        if (is_still_running) {
            impl->Cancel(false);
        }
    }
}



bool AudioMan::InitPlayback() const
{
    return impl_playback->InitPlayback();
}

void AudioMan::UninitPlayback() const
{
    impl_playback->UninitPlayback();
}

AudioRequest AudioMan::SubmitAudio(const std::vector<char> &audio_data) const
{
    return impl_playback->SubmitAudio(audio_data.data(), audio_data.size());
}

AudioRequest AudioMan::SubmitAudio(const char *audio_data, size_t count) const
{
    return impl_playback->SubmitAudio(audio_data, count);
}

void AudioMan::SetPlaybackVolumePercent(float sound_volume_percent) const
{
    impl_playback->SetPlaybackVolumePercent(sound_volume_percent);
}

float AudioMan::GetPlaybackVolumePercent() const
{
    return impl_playback->GetPlaybackVolumePercent();
}

void AudioMan::CancelAllPlayback() const
{
    impl_playback->CancelAllPlayback();
}



bool AudioMan::StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format) const
{
    return impl_recording->StartRecording(sample_rate, channels, format);
}

void AudioMan::StopRecording() const
{
    impl_recording->StopRecording();
}

bool AudioMan::IsRecording() const
{
    return impl_recording->IsRecording();
}

unsigned int AudioMan::GetRecordingSampleRate() const
{
    return impl_recording->GetRecordingSampleRate();
}

unsigned char AudioMan::GetRecordingChannelsCount() const
{
    return impl_recording->GetRecordingChannelsCount();
}

RecordingFormat_t AudioMan::GetRecordingRecordingFormat() const
{
    return impl_recording->GetRecordingRecordingFormat();
}

void AudioMan::SetRecordingSoundThresholdPercent(float sound_threshold_percent) const
{
    impl_recording->SetRecordingSoundThresholdPercent(sound_threshold_percent);
}

float AudioMan::GetRecordingSoundThresholdPercent() const
{
    return impl_recording->GetRecordingSoundThresholdPercent();
}

void AudioMan::SetRecordingSoundGainPercent(float sound_gain_percent) const // [0.0, >= 100.0]
{
    impl_recording->SetRecordingSoundGainPercent(sound_gain_percent);
}

float AudioMan::GetRecordingSoundGainPercent() const
{
    return impl_recording->GetRecordingSoundGainPercent();
}

void AudioMan::ClearRecording() const
{
    impl_recording->ClearRecording();
}

size_t AudioMan::SizeUnreadRecording() const
{
    return impl_recording->SizeUnreadRecording();
}

std::vector<char> AudioMan::GetUnreadRecording(size_t max_bytes) const
{
    return impl_recording->GetUnreadRecording(max_bytes);
}

std::vector<char> AudioMan::DecodeRecordingChunks(const std::vector<char> &chunks) const
{
    return impl_recording->DecodeRecordingChunks(chunks.data(), chunks.size());
}

std::vector<char> AudioMan::DecodeRecordingChunks(const char *chunks, size_t count) const
{
    return impl_recording->DecodeRecordingChunks(chunks, count);
}
