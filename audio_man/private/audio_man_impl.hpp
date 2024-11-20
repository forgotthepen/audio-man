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

#include <optional>
#include <vector>
#include <list>
#include <future>
#include <atomic>
#include <list>
#include <cstring> // size_t
#include <cstdint> // uintxx_t

#include "../audio_man.hpp"
#include "miniaudio/miniaudio.h"


class AudioManImpl;
class PlaybackRequestsMan;

class AudioRequestImpl {
private:
    friend class AudioManImpl;
    friend class PlaybackRequestsMan;
    
     // used when reading files
    std::optional<ma_decoder> decoder{};
    std::optional<ma_sound_config> cfg{};
    // ----
    
    std::optional<ma_sound> sound{};

    std::vector<char> data{};

    std::promise<bool> promise{};
    std::shared_future<bool> future_result{};

    std::list<AudioRequestImpl>::iterator my_itr{};
    PlaybackRequestsMan *requests_man{};

    bool done = false;
    
    std::recursive_mutex req_mtx{};
    
    void remove_from_requests_manager();

public:
    AudioRequestImpl() noexcept;
    AudioRequestImpl(AudioRequestImpl &&other) noexcept;
    ~AudioRequestImpl() = default;

    AudioRequestImpl& operator=(AudioRequestImpl &&other) noexcept;
    
    AudioRequestImpl(const AudioRequestImpl &other) = delete;
    AudioRequestImpl& operator=(const AudioRequestImpl &other) = delete;

    std::shared_future<bool> FutureResult() const;
    void Cancel(bool success);
};


class PlaybackRequestsMan
{
private:
    std::list<AudioRequestImpl> requests{};
    std::recursive_mutex mtx{};

public:
    ~PlaybackRequestsMan();

    std::list<AudioRequestImpl>::iterator CreateNew();
    void Remove(std::list<AudioRequestImpl>::iterator itr);
    void CancelAndRemoveAll();
};


struct MicChunk_t
{
    uint32_t original_bytes{};
    std::vector<char> compressed_data{};
};

struct MicChunkHeaderSerialized_t
{
    uint32_t original_bytes{};
    uint32_t compressed_bytes{};
    // compressed data array is appended here
};


class RecordingBufferMan
{
private:
    std::list<MicChunk_t> mic_buffer{};
    
public:
    void PushData(const char *data, uint32_t bytes);
    void Clear();
    std::vector<char> GetUnreadChunks(size_t max_bytes);
    size_t SizeUnread();
};


class AudioManImpl
{
private:
    struct RecordingDevice_t {
        ma_device_config cfg{};
        ma_device device{};
        RecordingFormat_t format{};
        unsigned int sample_rate{};
        unsigned char channels{};
    };

    PlaybackRequestsMan playback_requests{};
    ma_engine playback_engine{};
    bool is_playback_inited = false;

    RecordingBufferMan recording_buffer_man{};
    RecordingDevice_t recording_device{}; 
    bool is_recording_active = false;   

public:
    ~AudioManImpl();

    bool InitPlayback();
    void UninitPlayback();
    AudioRequestImpl* SubmitAudio(const char *audio_data, size_t count);
    void CancelAllPlayback();

    bool StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format);
    void StopRecording();
    bool IsRecording() const;
    unsigned int GetRecordingSampleRate() const;
    unsigned char GetRecordingChannelsCount() const;
    RecordingFormat_t GetRecordingRecordingFormat() const;
    void ClearRecording();
    size_t SizeUnreadRecording();
    std::vector<char> GetUnreadRecording(size_t max_bytes);
    std::vector<char> DecodeRecordingChunks(const char *chunks, size_t count);

};
