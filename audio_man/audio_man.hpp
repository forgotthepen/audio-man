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
class AudioManImpl;


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


enum class RecordingFormat_t {
    Float32,
    Signed16,
    Signed24,
    Signed32,
    Unsigned8,
};

struct MicChunk_t
{
    std::vector<char> compressed_chunk{};
    uint32_t original_bytes{};
};

struct RecordingDataChunks_t
{
    std::vector<MicChunk_t> chunks{};
    RecordingFormat_t format{};
    unsigned int sample_rate{};
};


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
    void CancelAllPlayback() const;
    
    bool StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format) const;
    void StopRecording() const;
    void ClearRecording() const;
    RecordingDataChunks_t GetUnreadRecordingChunks(size_t bytes = static_cast<size_t>(-1)) const;
    size_t SizeUnreadRecording() const;
    std::vector<char> DecodeRecordingChunks(const RecordingDataChunks_t& chunks) const;
};
