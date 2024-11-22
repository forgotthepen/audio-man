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

#include "silence_filter.hpp"
#include <cmath>
#include <cstdint> // intxx_t


// https://en.wikipedia.org/wiki/Audio_bit_depth


bool MicSilenceFilterPcmF32::IsSilencePcmData(const char *data, size_t count, float sound_threshold)
{
    if (!data || !count) {
        return true;
    }

    // PCM float32 stores values in range [-1.0, 1.0]

    auto samples = reinterpret_cast<const float *>(data);
    const auto samples_end = samples + (count / sizeof(float)); // each 4 bytes represent a sample

    for (; samples < samples_end; ++samples) {
        if (std::fabs(*samples) >= sound_threshold) {
            return false;
        }
    }

    return true;
}

bool MicSilenceFilterPcmS16::IsSilencePcmData(const char *data, size_t count, float sound_threshold)
{
    if (!data || !count) {
        return true;
    }

    // PCM signed16 stores values in range [-32768, 32767]
    const auto threshold = static_cast<int16_t>(32767L * sound_threshold);

    auto samples = reinterpret_cast<const int16_t *>(data);
    const auto samples_end = samples + (count / sizeof(int16_t));

    for (; samples < samples_end; ++samples) {
        if (std::abs(*samples) >= threshold) {
            return false;
        }
    }

    return true;
}

bool MicSilenceFilterPcmS24::IsSilencePcmData(const char *data, size_t count, float sound_threshold)
{
    if (!data || !count) {
        return true;
    }
    
    // each sample is 3 bytes in 24-bit PCM
    if (count % 3 != 0) {
        return false; // invalid data size
    }

    // PCM signed24 stores values in range [-8,388,608, 8,388,607]
    const int32_t threshold = static_cast<int32_t>(8388607L) * sound_threshold;

    const auto data_end = data + count;

    for (; data < data_end; data += 3) {
        int32_t sample =
            static_cast<int32_t>( data[0] ) | 
            (static_cast<int32_t>( data[1] ) << 8) | 
            (static_cast<int32_t>( data[2] ) << 16);
        
        // sign-extend the 24-bit value to 32 bits
        if (sample & 0x00800000L) { // notice the digit '8'
            sample |= 0xFF000000L; // set the upper bits if the sign bit is set
        }

        if (std::abs(sample) >= threshold) {
            return false;
        }
    }

    return true;
}

bool MicSilenceFilterPcmS32::IsSilencePcmData(const char *data, size_t count, float sound_threshold)
{
    if (!data || !count) {
        return true;
    }

    // PCM signed32 stores values in range [-2,147,483,648, 2,147,483,647]
    const auto threshold = static_cast<int32_t>(2147483647LL * sound_threshold);

    auto samples = reinterpret_cast<const int32_t *>(data);
    const auto samples_end = samples + (count / sizeof(int32_t));

    for (; samples < samples_end; ++samples) {
        if (std::abs(*samples) >= threshold) {
            return false;
        }
    }

    return true;
}

bool MicSilenceFilterPcmU8::IsSilencePcmData(const char *data, size_t count, float sound_threshold)
{
    if (!data || !count) {
        return true;
    }

    // PCM signed8 stores values in range [-128, 127]
    // PCM unsigned8 stores values in range [0, 255] (silence midpoint = 128)
    const auto threshold = static_cast<int8_t>(127L * sound_threshold);
    constexpr int8_t midpoint = 128;

    auto samples = reinterpret_cast<const uint8_t *>(data);
    const auto samples_end = samples + (count / sizeof(uint8_t));

    for (; samples < samples_end; ++samples) {
        const int8_t sample_normalized = static_cast<int8_t>(*samples) - midpoint;
        if (std::abs(sample_normalized) >= threshold) {
            return false;
        }
    }

    return true;
}
