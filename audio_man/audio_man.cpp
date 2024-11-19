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

#include "audio_man.hpp"
#include "private/audio_man_impl.hpp"



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



AudioMan::AudioMan()
{
    impl = new AudioManImpl{};
}

AudioMan::~AudioMan()
{
    if (impl) {
        delete impl;
        impl = nullptr;
    }
}

bool AudioMan::InitPlayback() const
{
    return impl->InitPlayback();
}

void AudioMan::UninitPlayback() const
{
    impl->UninitPlayback();
}

AudioRequest AudioMan::SubmitAudio(const std::vector<char> &audio_data) const
{
    return impl->SubmitAudio(audio_data);
}

void AudioMan::CancelAllPlayback() const
{
    impl->CancelAllPlayback();
}



bool AudioMan::StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format) const
{
    return impl->StartRecording(sample_rate, channels, format);
}

void AudioMan::StopRecording() const
{
    return impl->StopRecording();
}

void AudioMan::ClearRecording() const
{
    return impl->ClearRecording();
}

RecordingDataChunks_t AudioMan::GetUnreadRecordingChunks(size_t bytes) const
{
    return impl->GetUnreadRecordingChunks(bytes);
}

size_t AudioMan::SizeUnreadRecording() const
{
    return impl->SizeUnreadRecording();
}

std::vector<char> AudioMan::DecodeRecordingChunks(const RecordingDataChunks_t& chunks) const
{
    return impl->DecodeRecordingChunks(chunks);
}
