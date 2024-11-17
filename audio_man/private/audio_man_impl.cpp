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

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
#include "miniz/miniz.h"

#include "audio_man_impl.hpp"


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



static std::vector<char> compress_gzip(const char *data, uint32_t bytes) {
    auto max_compressed_bytes = mz_compressBound(bytes);
    std::vector<char> compressed_data(max_compressed_bytes);

    auto compressed_bytes = static_cast<mz_ulong>(max_compressed_bytes);
    auto status = mz_compress2(
        reinterpret_cast<unsigned char *>(compressed_data.data()),
        &compressed_bytes,
        reinterpret_cast<const unsigned char*>(data),
        bytes,
        MZ_BEST_COMPRESSION
    );

    if (status != MZ_OK) {
        return {};
    }

    compressed_data.resize(compressed_bytes);
    return compressed_data;
}

static std::vector<char> decompress_gzip(const std::vector<char> &compressed_data, size_t original_size) {
    std::vector<char> decompressed_data(original_size);

    auto decompressed_size = static_cast<mz_ulong>(original_size);
    auto status = mz_uncompress(
        reinterpret_cast<unsigned char*>(decompressed_data.data()), 
        &decompressed_size, 
        reinterpret_cast<const unsigned char*>(compressed_data.data()), 
        compressed_data.size());

    if (status != MZ_OK) {
        return {};
    }

    decompressed_data.resize(decompressed_size);
    return decompressed_data;
}

void RecordingBufferMan::PushData(const char *data, uint32_t bytes)
{
    std::lock_guard lock(mtx);

    auto &chunk = mic_buffer.emplace_back(MicChunk_t{});
    chunk.compressed_chunk = compress_gzip(data, bytes);
    chunk.original_bytes = bytes;
}

void RecordingBufferMan::Clear()
{
    std::lock_guard lock(mtx);

    mic_buffer.clear();
}

std::vector<MicChunk_t> RecordingBufferMan::GetUnreadChunks(size_t chunks_count)
{
    std::lock_guard lock(mtx);

    if (chunks_count > mic_buffer.size()) {
        chunks_count = mic_buffer.size();
    }

    auto ret = std::vector<MicChunk_t>(mic_buffer.begin(), mic_buffer.begin() + chunks_count);
    mic_buffer.erase(mic_buffer.begin(), mic_buffer.begin() + chunks_count);
    return ret;
}

size_t RecordingBufferMan::SizeUnread()
{
    std::lock_guard lock(mtx);

    return mic_buffer.size();
}



AudioManImpl::~AudioManImpl()
{
    CancelAllPlayback();
    UninitPlayback();

    StopRecording();
    recording_buffer_man.Clear();
}

bool AudioManImpl::InitPlayback()
{
    if (is_playback_inited) {
        return true;
    }

    if (ma_engine_init(NULL, &playback_engine) != MA_SUCCESS) {
        return false;
    }

    is_playback_inited = true;
    return true;
}

void AudioManImpl::UninitPlayback()
{
    if (!is_playback_inited) {
        return;
    }

    playback_requests.CancelAndRemoveAll();
    ma_engine_uninit(&playback_engine);
    is_playback_inited = false;
}

AudioRequestImpl* AudioManImpl::SubmitAudio(const std::vector<char> &audio_data)
{
    auto req = playback_requests.CreateNew();

    req->data.assign(audio_data.begin(), audio_data.end());

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
    if (ma_sound_init_ex(&playback_engine, &req->cfg.value(), &req->sound.value()) != MA_SUCCESS) {
        req->sound = {};
        req->Cancel(false);
        req->remove_from_requests_manager();
        return nullptr;
    }
    
    // ma_sound_set_spatialization_enabled(&sound, false);

    // lock this in case playback finished earlier than this function finishes execution
    std::lock_guard req_lock(req->req_mtx);

    if (ma_sound_start(&req->sound.value()) != MA_SUCCESS) {
        req->Cancel(false);
        req->remove_from_requests_manager();
        return nullptr;
    }

    return &*req;
}

void AudioManImpl::CancelAllPlayback()
{
    playback_requests.CancelAndRemoveAll();
}



bool AudioManImpl::StartRecording(unsigned int sample_rate, RecordingFormat_t format)
{
    if (is_recording_active) {
        return true;
    }
    
    recording_device.cfg = ma_device_config_init(ma_device_type_capture);
    
    switch (format) {
    case RecordingFormat_t::Float32: recording_device.cfg.capture.format = ma_format_f32; break;
    case RecordingFormat_t::Signed16: recording_device.cfg.capture.format = ma_format_s16; break;
    case RecordingFormat_t::Signed24: recording_device.cfg.capture.format = ma_format_s24; break;
    case RecordingFormat_t::Signed32: recording_device.cfg.capture.format = ma_format_s32; break;
    case RecordingFormat_t::Unsigned8: recording_device.cfg.capture.format = ma_format_u8; break;
    
    default: recording_device.cfg.capture.format = ma_format_s16; break;
    }

    recording_device.cfg.capture.channels = 1; // mono
    recording_device.cfg.sampleRate = sample_rate;
    recording_device.cfg.pUserData = &recording_buffer_man;
    recording_device.cfg.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount){
        if (!pInput) {
            return; // No input data available.
        }

        auto recording_buffer_man = static_cast<RecordingBufferMan *>(pDevice->pUserData);

        const char* inputData = static_cast<const char*>(pInput);
        auto frame_bytes = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels) * frameCount;
        recording_buffer_man->PushData(inputData, frame_bytes);
    };

    if (ma_device_init(nullptr, &recording_device.cfg, &recording_device.device) != MA_SUCCESS) {
        return false;
    }

    if (ma_device_start(&recording_device.device) != MA_SUCCESS) {
        ma_device_uninit(&recording_device.device);
        return false;
    }

    recording_device.format = format;
    recording_device.sample_rate = sample_rate;
    is_recording_active = true;
    return true;
}

void AudioManImpl::StopRecording()
{
    if (!is_recording_active) {
        return;
    }

    ma_device_uninit(&recording_device.device);
    recording_device.sample_rate = 0;
    is_recording_active = false;
}

void AudioManImpl::ClearRecording()
{
    recording_buffer_man.Clear();
}

RecordingDataChunks_t AudioManImpl::GetUnreadRecordingChunks(size_t chunks_count)
{
    RecordingDataChunks_t chunk{};
    chunk.chunks = recording_buffer_man.GetUnreadChunks(chunks_count);
    chunk.format = recording_device.format;
    chunk.sample_rate = recording_device.sample_rate;
    return chunk;
}

size_t AudioManImpl::SizeUnreadRecording()
{
    return recording_buffer_man.SizeUnread();
}

std::vector<char> AudioManImpl::DecodeRecordingChunks(const RecordingDataChunks_t& chunks)
{
    std::vector<char> data{};
    for (const auto &chunk : chunks.chunks) {
        auto deco = decompress_gzip(chunk.compressed_chunk, chunk.original_bytes);
        data.insert(data.end(), deco.begin(), deco.end());
    }
    return data;
}
