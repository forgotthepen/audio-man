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
#include <cstring> // size_t

#include "miniaudio/miniaudio.h"


class AudioPlayback;
class PlaybackRequestsMan;

class AudioRequestImpl {
private:
    friend class AudioPlayback;
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

struct PlaybackDevice_t {
    ma_engine engine{};

    float volume = 1.0f;
};

class AudioPlayback
{
private:
    PlaybackRequestsMan playback_requests{};
    PlaybackDevice_t playback_device{};
    bool is_playback_inited = false;

public:
    ~AudioPlayback();

    bool InitPlayback();
    void UninitPlayback();

    AudioRequestImpl* SubmitAudio(const char *audio_data, size_t count);

    void SetPlaybackVolumePercent(float sound_volume_percent);
    float GetPlaybackVolumePercent() const;

    void CancelAllPlayback();
};
