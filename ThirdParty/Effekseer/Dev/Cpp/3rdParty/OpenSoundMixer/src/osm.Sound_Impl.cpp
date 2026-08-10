#define NOMINMAX
#include "osm.Sound_Impl.h"

#include "Decorder/osm.OggDecorder.h"
#include "Decorder/osm.WaveDecorder.h"
#include "Filter/osm.Resampler.h"

namespace osm {
Sound_Impl::Sound_Impl() : m_isDecompressed(false) {
    m_loopStart = 0.0f;
    m_loopEnd = -1.0;
}

Sound_Impl::~Sound_Impl() {}

bool Sound_Impl::Load(const void* data, int32_t size, bool isDecompressed) {
    if (data == nullptr) return false;
    if (size <= 0) return false;

    // check a file type and generate a decorder
    auto type = Decorder::GetFileType(data, size);

    if (type == eFileType::OGG) {
        m_decorder = std::make_shared<OggDecorder>();
    } else if (type == eFileType::WAVE) {
        m_decorder = std::make_shared<WaveDecorder>();
    }
    else
    {
        return false;
    }
    
    auto loaded = m_decorder->LoadHeader((uint8_t*)data, size);
    if (!loaded) return false;

    if (isDecompressed) {
        m_samples.resize(m_decorder->GetSampleCount());

        if (m_decorder->GetAllSamples(m_samples.data(), m_samples.size(), (uint8_t*)data, size)) {
        } else {
            auto loaded = m_decorder->Load((uint8_t*)data, size);
            if (!loaded) return false;

            auto count = m_decorder->GetSamples(m_samples.data(), 0, m_samples.size());
            assert(m_samples.size() == count);

            m_decorder.reset();
        }

        m_isDecompressed = true;
    } else {
        m_data.resize(size);
        memcpy(m_data.data(), data, size);

        auto loaded = m_decorder->Load(m_data.data(), size);
        if (!loaded) return false;

        m_isDecompressed = false;
    }

    if (m_loopEnd == -1.0f) {
        m_loopEnd = GetLength();
    }

    return true;
}

int32_t Sound_Impl::GetSamples(Sample* samples, int32_t offset, int32_t count) {
    if (m_isDecompressed) {
        count = std::min(static_cast<int32_t>(m_samples.size()) - offset, count);
        memcpy(samples, &(m_samples[offset]), count * sizeof(Sample));
        return count;
    }

    return m_decorder->GetSamples(samples, offset, count);
}

int32_t Sound_Impl::GetSampleCount() const {
    if (m_isDecompressed) return m_samples.size();

    return m_decorder->GetSampleCount();
}

float Sound_Impl::GetLength() const { return GetSampleCount() / 44100.0f; }
}  // namespace osm
