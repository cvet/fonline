
#include "osm.ReferenceObject.h"

namespace osm {
ReferenceObject::ReferenceObject() : m_reference(1) {}

ReferenceObject::~ReferenceObject() {}

int ReferenceObject::AddRef() {
    std::atomic_fetch_add_explicit(&m_reference, 1, std::memory_order_consume);
    return m_reference;
}

int ReferenceObject::GetRef() { return m_reference; }

int ReferenceObject::Release() {
    assert(m_reference > 0);

    bool destroy = std::atomic_fetch_sub_explicit(&m_reference, 1, std::memory_order_consume) == 1;
    if (destroy) {
        delete this;
        return 0;
    }

    return m_reference;
}
}  // namespace osm
