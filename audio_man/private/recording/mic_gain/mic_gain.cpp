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

#include "mic_gain.hpp"
#include <cmath>
#include <cstdint> // intxx_t


std::vector<char> MicGainPcmF32::ApplyGain(const char *data, size_t count, float sound_gain)
{
    if (!data || !count) {
        return {};
    }

    auto samples = reinterpret_cast<const float *>(data);
    const auto samples_end = samples + (count / sizeof(float));
    
    std::vector<char> result(count);
    auto result_buff = reinterpret_cast<float *>(result.data());

    for (; samples < samples_end; ++samples, ++result_buff) {
        *result_buff = std::min(std::max(*samples * sound_gain, -1.0f), 1.0f);
    }

    return result;
}

std::vector<char> MicGainPcmS16::ApplyGain(const char *data, size_t count, float sound_gain)
{
    if (!data || !count) {
        return {};
    }

    auto samples = reinterpret_cast<const int16_t *>(data);
    const auto samples_end = samples + (count / sizeof(int16_t));
    
    std::vector<char> result(count);
    auto result_buff = reinterpret_cast<int16_t *>(result.data());

    for (; samples < samples_end; ++samples, ++result_buff) {
        *result_buff = static_cast<int16_t>(
            std::min(std::max(*samples * sound_gain, -32768.0f), 32767.0f)
        );
    }

    return result;
}

std::vector<char> MicGainPcmS24::ApplyGain(const char *data, size_t count, float sound_gain)
{
    if (!data || !count) {
        return {};
    }

    // each sample is 3 bytes in 24-bit PCM
    if (count % 3 != 0) {
        return std::vector<char>(data, data + count);
    }

    const auto data_end = data + count;
    
    std::vector<char> result(count);
    auto result_buff = result.data();

    for (; data < data_end; data += 3, result_buff += 3) {
        int32_t sample =
            static_cast<int32_t>( data[0] ) | 
            (static_cast<int32_t>( data[1] ) << 8) | 
            (static_cast<int32_t>( data[2] ) << 16);
        
        // sign-extend the 24-bit value to 32 bits
        if (sample & 0x00800000L) { // notice the digit '8'
            sample |= 0xFF000000L; // set the upper bits if the sign bit is set
        }

        sample = static_cast<int32_t>(
            std::min(std::max(sample * static_cast<double>(sound_gain), -8388608.0), 8388607.0)
        );
        auto sample_buff = reinterpret_cast<char *>(&sample);

        result_buff[0] = sample_buff[0];
        result_buff[1] = sample_buff[1];
        result_buff[2] = sample_buff[2];
    }

    return result;
}

std::vector<char> MicGainPcmS32::ApplyGain(const char *data, size_t count, float sound_gain)
{
    if (!data || !count) {
        return {};
    }

    auto samples = reinterpret_cast<const int32_t *>(data);
    const auto samples_end = samples + (count / sizeof(int32_t));
    
    std::vector<char> result(count);
    auto result_buff = reinterpret_cast<int32_t *>(result.data());

    for (; samples < samples_end; ++samples, ++result_buff) {
        *result_buff = static_cast<int32_t>(
            std::min(std::max(*samples * static_cast<double>(sound_gain), -2147483648.0), 2147483647.0)
        );
    }

    return result;
}

std::vector<char> MicGainPcmU8::ApplyGain(const char *data, size_t count, float sound_gain)
{
    if (!data || !count) {
        return {};
    }

    auto samples = reinterpret_cast<const uint8_t *>(data);
    const auto samples_end = samples + (count / sizeof(uint8_t));
    
    std::vector<char> result(count);
    auto result_buff = reinterpret_cast<uint8_t *>(result.data());

    for (; samples < samples_end; ++samples, ++result_buff) {
        *result_buff = static_cast<uint8_t>(
            std::min(std::max(*samples * sound_gain, 0.0f), 255.0f)
        );
    }

    return result;
}
