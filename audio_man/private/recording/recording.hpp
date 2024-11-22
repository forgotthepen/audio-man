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
#include <list>
#include <memory>
#include <cstring> // size_t
#include <cstdint> // uintxx_t

#include "../../audio_man.hpp"
#include "miniaudio/miniaudio.h"


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
    size_t SizeUnread() const;
};

struct RecordingDevice_t {
    ma_device_config cfg{};
    ma_device device{};
    unsigned int sample_rate{};
    unsigned char channels{};
    RecordingFormat_t format{};
    float sound_threshold = 0; // allow anything
};

class AudioRecording
{
private:
    RecordingBufferMan recording_buffer_man{};
    RecordingDevice_t recording_device{}; 
    bool is_recording_active = false;   

public:
    ~AudioRecording();

    bool StartRecording(unsigned int sample_rate, unsigned char channels, RecordingFormat_t format);
    void StopRecording();
    bool IsRecording() const;

    unsigned int GetRecordingSampleRate() const;
    unsigned char GetRecordingChannelsCount() const;
    RecordingFormat_t GetRecordingRecordingFormat() const;

    void SetRecordingSoundThresholdPercent(float sound_threshold_percent); // [0.0, 100.0]
    float GetRecordingSoundThresholdPercent() const;
    float GetRecordingSoundThresholdPercentUnscaled() const;

    void ClearRecording();
    size_t SizeUnreadRecording();
    std::vector<char> GetUnreadRecording(size_t max_bytes);
    std::vector<char> DecodeRecordingChunks(const char *chunks, size_t count);

    RecordingBufferMan* GetRecordingBufferMan();

};
