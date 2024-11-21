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

#pragma once

#include <vector>
#include <future>


class AudioRequestImpl;
class AudioRequest {
private:
    AudioRequestImpl *impl{};
    std::shared_future<bool> future_result{};

public:
    AudioRequest(AudioRequestImpl *ptr);
    AudioRequest(AudioRequest &&other) = default;
    AudioRequest(const AudioRequest &other) = default;

    bool IsValid() const;
    std::shared_future<bool> FutureResult() const;
    bool Wait() const;
    void Cancel() const;
};


enum class RecordingFormat_t : uint32_t {
    Float32,
    Signed16 = 16,
    Signed24 = 24,
    Signed32 = 32,
    Unsigned8 = 8,
};


class AudioManImpl;
class AudioMan
{
private:
    AudioManImpl *impl{};

public:
    AudioMan();
    ~AudioMan();

    bool InitPlayback() const;
    void UninitPlayback() const;
    AudioRequest SubmitAudio(const std::vector<char> &audio_data) const;
    AudioRequest SubmitAudio(const char *audio_data, size_t count) const;
    void CancelAllPlayback() const;
    
    bool StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format, unsigned char sound_threshold = 2) const;
    void StopRecording() const;
    bool IsRecording() const;
    unsigned int GetRecordingSampleRate() const;
    unsigned char GetRecordingChannelsCount() const;
    RecordingFormat_t GetRecordingRecordingFormat() const;
    unsigned char GetRecordingSoundThreshold() const;
    void ClearRecording() const;
    size_t SizeUnreadRecording() const;
    std::vector<char> GetUnreadRecording(size_t max_bytes = static_cast<size_t>(-1)) const;
    std::vector<char> DecodeRecordingChunks(const std::vector<char> &chunks) const;
    std::vector<char> DecodeRecordingChunks(const char *chunks, size_t count) const;
};
