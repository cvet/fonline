
#pragma once

#include <math.h>
#include <memory>
#include <string>
#include <vector>

#include "../OpenSoundMixer.h"
#include "../osm.Decorder.h"

namespace osm {
class PCM {
private:
    uint8_t* m_data = nullptr;
    int32_t m_size = 0;
    int32_t m_channelCount = 0;
    int32_t m_samplesPerSec = 0;
    int32_t m_bitsPerSample = 0;

public:
    PCM(void* data, int32_t size, int32_t channelCount, int32_t samplesPerSec, int32_t bitsPerSample);
    double GetLength() const;

    int32_t GetSampleCountAs44100Stereo16bit() const;

    void GetSampleAs44100Stereo16bit(int32_t frame, Sample& sample) const;
};

class WaveDecorder : public Decorder {
private:
    std::vector<uint8_t> m_data;

    std::shared_ptr<PCM> m_pcm;

    struct WAVEFormat {
        uint16_t FormatID;
        uint16_t ChannelCount;
        uint32_t SamplesPerSec;
        uint32_t AvgBytesPerSec;
        uint16_t BlockAlign;
        uint16_t BitsPerSample;
    };

    WAVEFormat fmt;

    int32_t bufferDataSize = 0;

    static bool Read(void* dst, void* src, int copySize, int& offset, int dataSize);

public:
    WaveDecorder();
    virtual ~WaveDecorder();
    bool LoadHeader(uint8_t* data, int32_t size) override;
    bool Load(uint8_t* data, int32_t size) override;

    int32_t GetSamples(Sample* samples, int32_t offset, int32_t count) override;

    bool GetAllSamples(Sample* samples, int32_t count, uint8_t* data, int32_t size) override;

    int32_t GetSampleCount() override;

    int32_t GetChannelCount() const override;

    int32_t GetRate() const override;
};
}  // namespace osm
