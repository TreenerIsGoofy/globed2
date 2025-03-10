#pragma once
#include <defs.hpp>

#if GLOBED_VOICE_SUPPORT

#include "decoder.hpp"

class AudioSampleQueue {
public:
    AudioSampleQueue() {};
    // enable moving
    AudioSampleQueue(AudioSampleQueue&&) = default;
    AudioSampleQueue& operator=(AudioSampleQueue&&) = default;

    void writeData(const DecodedOpusData& data);
    void writeData(float* pcm, size_t length);
    // contrary to the name, this will erase the samples from this queue after copying them to `dest`
    size_t copyTo(float* dest, size_t samples);
    size_t size() const;
    void clear();
    float* data();

private:
    std::vector<float> buf;
};

#endif // GLOBED_VOICE_SUPPORT
