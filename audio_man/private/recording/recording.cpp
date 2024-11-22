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

#include <utility>
#include <memory>
#include <numeric>
#include <unordered_map>

#include "miniz/miniz.h"

#include "silence_filter/silence_filter.hpp"
#include "mic_gain/mic_gain.hpp"
#include "recording.hpp"


static const std::unordered_map<RecordingFormat_t, std::unique_ptr<IMicSilenceFilter>> silence_filters = [] {
    std::unordered_map<RecordingFormat_t, std::unique_ptr<IMicSilenceFilter>> filters{};

    filters.try_emplace( RecordingFormat_t::Float32,   std::move( std::make_unique<MicSilenceFilterPcmF32>() ) );
    filters.try_emplace( RecordingFormat_t::Signed16,  std::move( std::make_unique<MicSilenceFilterPcmS16>() ) );
    filters.try_emplace( RecordingFormat_t::Signed24,  std::move( std::make_unique<MicSilenceFilterPcmS24>() ) );
    filters.try_emplace( RecordingFormat_t::Signed32,  std::move( std::make_unique<MicSilenceFilterPcmS32>() ) );
    filters.try_emplace( RecordingFormat_t::Unsigned8, std::move( std::make_unique<MicSilenceFilterPcmU8>()  ) );

    return filters;
}();

static const std::unordered_map<RecordingFormat_t, std::unique_ptr<IMicGain>> gain_filters = [] {
    std::unordered_map<RecordingFormat_t, std::unique_ptr<IMicGain>> filters{};

    filters.try_emplace( RecordingFormat_t::Float32,   std::move( std::make_unique<MicGainPcmF32>() ) );
    filters.try_emplace( RecordingFormat_t::Signed16,  std::move( std::make_unique<MicGainPcmS16>() ) );
    filters.try_emplace( RecordingFormat_t::Signed24,  std::move( std::make_unique<MicGainPcmS24>() ) );
    filters.try_emplace( RecordingFormat_t::Signed32,  std::move( std::make_unique<MicGainPcmS32>() ) );
    filters.try_emplace( RecordingFormat_t::Unsigned8, std::move( std::make_unique<MicGainPcmU8>()  ) );

    return filters;
}();


static std::vector<char> compress_gzip(const char *data, uint32_t bytes)
{
    auto max_compressed_bytes = mz_compressBound(bytes);
    std::vector<char> compressed_data(max_compressed_bytes);

    auto compressed_bytes = static_cast<mz_ulong>(max_compressed_bytes);
    auto status = mz_compress2(
        reinterpret_cast<unsigned char *>(compressed_data.data()),
        &compressed_bytes,
        reinterpret_cast<const unsigned char*>(data),
        bytes,
        MZ_BEST_SPEED
    );

    if (status != MZ_OK) {
        return std::vector<char>(data, data + bytes);
    }

    compressed_data.resize(compressed_bytes);
    return compressed_data;
}

static std::vector<char> decompress_gzip(const char *compressed_data, size_t compressed_data_size, size_t original_size)
{
    if (compressed_data_size == original_size) { // previous compression failed
        return std::vector<char>(compressed_data, compressed_data + compressed_data_size);
    }

    std::vector<char> decompressed_data(original_size);

    auto decompressed_size = static_cast<mz_ulong>(original_size);
    auto status = mz_uncompress(
        reinterpret_cast<unsigned char*>(decompressed_data.data()), 
        &decompressed_size, 
        reinterpret_cast<const unsigned char*>(compressed_data), 
        compressed_data_size);

    if (status != MZ_OK) {
        return std::vector<char>(compressed_data, compressed_data + compressed_data_size);
    }

    decompressed_data.resize(decompressed_size);
    return decompressed_data;
}



void RecordingBufferMan::PushData(const char *data, uint32_t bytes)
{
    if (!bytes) {
        return;
    }

    auto chunk = MicChunk_t{};
    chunk.original_bytes = bytes;
    chunk.compressed_data = compress_gzip(data, bytes);
    mic_buffer.emplace_back(std::move(chunk));
}

void RecordingBufferMan::Clear()
{
    mic_buffer.clear();
}

std::vector<char> RecordingBufferMan::GetUnreadChunks(size_t max_bytes)
{
    if (mic_buffer.empty() || !max_bytes) {
        return {};
    }

    size_t bytes_to_copy = 0;
    const auto mic_buffer_begin = mic_buffer.begin();
    const auto mic_buffer_end = mic_buffer.end();
    auto last_chunk_it = mic_buffer_begin;
    for (; last_chunk_it != mic_buffer_end; ++last_chunk_it) {
        auto chunk_size = sizeof(MicChunkHeaderSerialized_t) + last_chunk_it->compressed_data.size();
        bytes_to_copy += chunk_size;
        if (bytes_to_copy > max_bytes) {
            bytes_to_copy -= chunk_size;
            break;
        }

    }

    if (!bytes_to_copy) {
        return {};
    }

    std::vector<char> ret{};
    ret.reserve(bytes_to_copy);

    for (auto chunk_it = mic_buffer_begin; chunk_it != last_chunk_it; ++chunk_it) {
        auto header = MicChunkHeaderSerialized_t{};
        header.original_bytes = chunk_it->original_bytes;
        header.compressed_bytes = static_cast<uint32_t>(chunk_it->compressed_data.size());
        ret.insert(ret.end(), reinterpret_cast<const char *>(&header), reinterpret_cast<const char *>(&header) + sizeof(header)); // header
        ret.insert(ret.end(), chunk_it->compressed_data.begin(), chunk_it->compressed_data.end()); // compressed data
    }

    mic_buffer.erase(mic_buffer_begin, last_chunk_it);

    return ret;
}

size_t RecordingBufferMan::SizeUnread() const
{
    if (mic_buffer.empty()) {
        return {};
    }

    return std::accumulate(mic_buffer.begin(), mic_buffer.end(), static_cast<size_t>(0), [](size_t acc, const MicChunk_t &item){
        return acc + sizeof(MicChunkHeaderSerialized_t) + item.compressed_data.size();
    });
}



AudioRecording::~AudioRecording()
{
    StopRecording();
    recording_buffer_man.Clear();
}



bool AudioRecording::StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format)
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

    recording_device.cfg.capture.channels = channels;
    recording_device.cfg.sampleRate = sample_rate;
    recording_device.cfg.pUserData = this;
    recording_device.cfg.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount){
        if (!pInput) {
            return; // no input data
        }

        auto input_data = static_cast<const char*>(pInput);
        auto frame_bytes = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels) * frameCount;
        
        auto self_ref = static_cast<AudioRecording *>(pDevice->pUserData);

        std::vector<char> pcm_data{};
        auto gain_it = gain_filters.find(self_ref->GetRecordingRecordingFormat());
        if (gain_filters.end() != gain_it) {
            pcm_data = gain_it->second->ApplyGain(input_data, frame_bytes, self_ref->GetRecordingSoundGainPercentUnscaled());
        } else {
            pcm_data = std::vector<char>(input_data, input_data + frame_bytes);
        }
        
        auto filter_it = silence_filters.find(self_ref->GetRecordingRecordingFormat());
        if (silence_filters.end() != filter_it) {
            if (filter_it->second->IsSilencePcmData(pcm_data.data(), pcm_data.size(), self_ref->GetRecordingSoundThresholdPercentUnscaled())) {
                return;
            }
        }

        self_ref->GetRecordingBufferMan()->PushData(pcm_data.data(), static_cast<uint32_t>(pcm_data.size()));
    };

    if (ma_device_init(nullptr, &recording_device.cfg, &recording_device.device) != MA_SUCCESS) {
        return false;
    }

    recording_device.sample_rate = sample_rate;
    recording_device.channels = channels;
    recording_device.format = format;

    if (ma_device_start(&recording_device.device) != MA_SUCCESS) {
        ma_device_uninit(&recording_device.device);
        return false;
    }

    is_recording_active = true;
    return true;
}

void AudioRecording::StopRecording()
{
    if (!is_recording_active) {
        return;
    }

    ma_device_uninit(&recording_device.device);
    recording_device.sample_rate = 0;
    is_recording_active = false;
}

bool AudioRecording::IsRecording() const
{
    return is_recording_active;
}

unsigned int AudioRecording::GetRecordingSampleRate() const
{
    return recording_device.sample_rate;
}

unsigned char AudioRecording::GetRecordingChannelsCount() const
{
    return recording_device.channels;
}

RecordingFormat_t AudioRecording::GetRecordingRecordingFormat() const
{
    return recording_device.format;
}

void AudioRecording::SetRecordingSoundThresholdPercent(float sound_threshold_percent) // [0.0, 100.0]
{
    if (sound_threshold_percent < 0) {
        sound_threshold_percent = 0;
    } else if (sound_threshold_percent > 100) {
        sound_threshold_percent = 100;
    }
    recording_device.sound_threshold = sound_threshold_percent / 100; // [0.0, 1.0]
}

float AudioRecording::GetRecordingSoundThresholdPercent() const
{
    return recording_device.sound_threshold * 100;
}

float AudioRecording::GetRecordingSoundThresholdPercentUnscaled() const
{
    return recording_device.sound_threshold;
}

void AudioRecording::SetRecordingSoundGainPercent(float sound_gain_percent) // [0.0, >= 100.0]
{
    if (sound_gain_percent < 0) {
        sound_gain_percent = 0;
    }
    recording_device.sound_gain = sound_gain_percent / 100; // [0.0, >= 1.0]
}

float AudioRecording::GetRecordingSoundGainPercent() const
{
    return recording_device.sound_gain * 100;
}

float AudioRecording::GetRecordingSoundGainPercentUnscaled() const
{
    return recording_device.sound_gain;
}

void AudioRecording::ClearRecording()
{
    recording_buffer_man.Clear();
}

size_t AudioRecording::SizeUnreadRecording()
{
    return recording_buffer_man.SizeUnread();
}

std::vector<char> AudioRecording::GetUnreadRecording(size_t max_bytes)
{
    return recording_buffer_man.GetUnreadChunks(max_bytes);
}

std::vector<char> AudioRecording::DecodeRecordingChunks(const char *chunks, size_t count)
{
    if (!chunks || !count) {
        return {};
    }

    std::vector<char> data{};
    data.reserve(count + count / 2);

    const auto chunks_end = chunks + count;
    while (chunks < chunks_end) {
        auto chunk = reinterpret_cast<const MicChunkHeaderSerialized_t *>(chunks);
        auto compressed_chunk = chunks + sizeof(MicChunkHeaderSerialized_t);
        if (chunk->original_bytes != chunk->compressed_bytes) {
            auto deco = decompress_gzip(compressed_chunk, chunk->compressed_bytes, chunk->original_bytes);
            data.insert(data.end(), deco.begin(), deco.end());
        } else { // previous compression failed
            data.insert(data.end(), compressed_chunk, compressed_chunk + chunk->compressed_bytes);
        }
        chunks += sizeof(MicChunkHeaderSerialized_t) + chunk->compressed_bytes;
    }

    return data;
}

RecordingBufferMan* AudioRecording::GetRecordingBufferMan()
{
    return &recording_buffer_man;
}
