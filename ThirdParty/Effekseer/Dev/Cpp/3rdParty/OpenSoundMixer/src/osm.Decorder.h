
#pragma once

#include "OpenSoundMixer.h"
#include "OpenSoundMixerInternal.h"

namespace osm {
enum class eFileType {
    Unknown,
    OGG,
    WAVE,
};

class Decorder {
public:
    Decorder() {}
    virtual ~Decorder() {}
    virtual bool LoadHeader(uint8_t* data, int32_t size) = 0;
    virtual bool Load(uint8_t* data, int32_t size) = 0;
    virtual int32_t GetSamples(Sample* samples, int32_t offset, int32_t count) = 0;

    virtual bool GetAllSamples(Sample* samples, int32_t count, uint8_t* data, int32_t size) = 0;

    virtual int32_t GetSampleCount() = 0;

    virtual int32_t GetChannelCount() const = 0;

    virtual int32_t GetRate() const = 0;

    static eFileType GetFileType(const void* data, int32_t size);
};
}  // namespace osm
