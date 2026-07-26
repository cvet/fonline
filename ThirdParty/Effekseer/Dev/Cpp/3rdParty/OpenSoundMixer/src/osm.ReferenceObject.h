
#pragma once

#include "OpenSoundMixer.h"
#include "OpenSoundMixerInternal.h"

namespace osm {
/**
@brief	Reference Counted
*/
class ReferenceObject : public IReference {
private:
    mutable std::atomic<int32_t> m_reference;

public:
    ReferenceObject();

    virtual ~ReferenceObject();

    virtual int AddRef();

    virtual int GetRef();

    virtual int Release();
};
}  // namespace osm
