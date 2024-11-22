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

#include <thread>
#include <utility>
#include <memory>
#include <numeric>

#include "playback.hpp"



void AudioRequestImpl::remove_from_requests_manager()
{
    requests_man->Remove(my_itr);
}

AudioRequestImpl::AudioRequestImpl() noexcept
{
    future_result = promise.get_future();
}

AudioRequestImpl::AudioRequestImpl(AudioRequestImpl &&other) noexcept
    : AudioRequestImpl()
{
    std::swap(decoder, other.decoder);
    std::swap(cfg, other.cfg);

    std::swap(sound, other.sound);
    
    std::swap(data, other.data);

    std::swap(promise, other.promise);
    std::swap(future_result, other.future_result);
    
    std::swap(my_itr, other.my_itr);
    std::swap(requests_man, other.requests_man);
    
    std::swap(done, other.done);
}

AudioRequestImpl& AudioRequestImpl::operator=(AudioRequestImpl &&other) noexcept
{
    if (&other != this) {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}

std::shared_future<bool> AudioRequestImpl::FutureResult() const
{
    return future_result;
}

void AudioRequestImpl::Cancel(bool success)
{
    std::lock_guard lock(req_mtx);

    if (done) {
        return;
    }

    if (sound) {
        ma_sound_uninit(&sound.value());
    }

    if (decoder) {
        ma_decoder_uninit(&decoder.value());
    }

    promise.set_value(success);
    done = true;
}



std::list<AudioRequestImpl>::iterator PlaybackRequestsMan::CreateNew()
{
    std::lock_guard lock(mtx);

    auto &new_item = requests.emplace_back(AudioRequestImpl{});
    auto itr = --requests.end();
    
    new_item.requests_man = this;
    new_item.my_itr = itr;
    return itr;
}

void PlaybackRequestsMan::Remove(std::list<AudioRequestImpl>::iterator itr)
{
    std::lock_guard lock(mtx);

    requests.erase(itr);
}

void PlaybackRequestsMan::CancelAndRemoveAll()
{
    std::lock_guard lock(mtx);

    std::vector<std::shared_future<bool>> promises{};
    for (auto &req : requests) {
        req.Cancel(false);
        promises.emplace_back(req.future_result);
    }

    for (auto &promise : promises) {
        promise.get();
    }
    
    requests.clear();
}

PlaybackRequestsMan::~PlaybackRequestsMan()
{
    CancelAndRemoveAll();
}





AudioPlayback::~AudioPlayback()
{
    CancelAllPlayback();
    UninitPlayback();
}

bool AudioPlayback::InitPlayback()
{
    if (is_playback_inited) {
        return true;
    }

    if (ma_engine_init(nullptr, &playback_device.engine) != MA_SUCCESS) {
        return false;
    }

    is_playback_inited = true;
    return true;
}

void AudioPlayback::UninitPlayback()
{
    if (!is_playback_inited) {
        return;
    }

    playback_requests.CancelAndRemoveAll();
    ma_engine_uninit(&playback_device.engine);
    is_playback_inited = false;
}

AudioRequestImpl* AudioPlayback::SubmitAudio(const char *audio_data, size_t count)
{
    if (!is_playback_inited) {
        return {};
    }

    auto req = playback_requests.CreateNew();

    req->data.assign(audio_data, audio_data + count);

    req->decoder = ma_decoder{};
    if (ma_decoder_init_memory(req->data.data(), req->data.size(), nullptr, &req->decoder.value()) != MA_SUCCESS) {
        req->decoder = {};
        req->Cancel(false);
        req->remove_from_requests_manager();
        return nullptr;
    }

    req->cfg = ma_sound_config_init();
    req->cfg.value().pDataSource = static_cast<ma_data_source *>(&req->decoder.value());
    req->cfg.value().pEndCallbackUserData = &*req;
    req->cfg.value().endCallback = [](void *pUserData, ma_sound *pSound){
        std::thread([pUserData](){ // on a separate thread because in the docs it mentioned we can't call xxx_uninit() in the callback
            auto req = static_cast<AudioRequestImpl *>(pUserData);
            req->Cancel(true);
            req->remove_from_requests_manager();
        }).detach();
    };

    req->sound = ma_sound{};
    if (ma_sound_init_ex(&playback_device.engine, &req->cfg.value(), &req->sound.value()) != MA_SUCCESS) {
        req->sound = {};
        req->Cancel(false);
        req->remove_from_requests_manager();
        return nullptr;
    }
    
    // ma_sound_set_spatialization_enabled(&req->sound.value(), MA_FALSE);

    // lock this in case playback finished earlier than this function finishes execution
    std::lock_guard req_lock(req->req_mtx);

    if (ma_sound_start(&req->sound.value()) != MA_SUCCESS) {
        req->Cancel(false);
        req->remove_from_requests_manager();
        return nullptr;
    }

    return &*req;
}

void AudioPlayback::SetPlaybackVolumePercent(float sound_volume_percent)
{
    if (sound_volume_percent < 0) {
        sound_volume_percent = 0;
    }
    sound_volume_percent /= 100;
    
    if (ma_engine_set_volume(&playback_device.engine, sound_volume_percent) == MA_SUCCESS) {
        playback_device.volume = sound_volume_percent;
    }
}

float AudioPlayback::GetPlaybackVolumePercent() const
{
    return playback_device.volume * 100;
}

void AudioPlayback::CancelAllPlayback()
{
    playback_requests.CancelAndRemoveAll();
}


