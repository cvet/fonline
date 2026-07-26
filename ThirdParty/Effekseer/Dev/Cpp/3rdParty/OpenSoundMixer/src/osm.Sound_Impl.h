
#pragma once

#include "OpenSoundMixer.h"
#include "OpenSoundMixerInternal.h"

#include "osm.Decorder.h"
#include "osm.ReferenceObject.h"

#include "Filter/osm.Resampler.h"

namespace osm {

// "SoundSource"
class Sound_Impl : public Sound, public ReferenceObject {
private:
    bool m_isDecompressed;

    //! a buffer to store decompressed data
    std::vector<Sample> m_samples;

    //! a buffer to store compressed data
    std::vector<uint8_t> m_data;

    //! decode a compressed data
    std::shared_ptr<Decorder> m_decorder;

    float m_loopStart;
    float m_loopEnd;
    bool isLoopMode = false;

public:
    Sound_Impl();
    virtual ~Sound_Impl();

    bool Load(const void* data, int32_t size, bool isDecompressed);
    int32_t GetSamples(Sample* samples, int32_t offset, int32_t count);
    int32_t GetSampleCount() const;

    float GetLoopStartingPoint() const override { return m_loopStart; }

    void SetLoopStartingPoint(float startingPoint) override { m_loopStart = startingPoint; }

    float GetLoopEndPoint() const override { return m_loopEnd; }

    void SetLoopEndPoint(float endPoint) override { m_loopEnd = endPoint; }

    bool GetIsLoopingMode() const override { return isLoopMode; }

    void SetIsLoopingMode(bool isLoopingMode) override { isLoopMode = isLoopingMode; }

    float GetLength() const override;

public:
    virtual int GetRef() override { return ReferenceObject::GetRef(); }
    virtual int AddRef() override { return ReferenceObject::AddRef(); }
    virtual int Release() override { return ReferenceObject::Release(); }
};
}  // namespace osm
