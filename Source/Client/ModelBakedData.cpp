// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ModelBakedData.h"

#if FO_ENABLE_3D

FO_BEGIN_NAMESPACE

void VerifyModelBakedCountFitsData(const data_reader& reader, size_t count, size_t min_element_size, string_view field, string_view context)
{
    FO_STRONG_ASSERT(min_element_size != 0, "Minimum baked element size must be non-zero");

    if (count > reader.get_unread_size() / min_element_size) {
        throw DataReadingException("Baked model field element count cannot fit in unread bytes", context, field, count, reader.get_unread_size());
    }
}

FO_END_NAMESPACE

#endif
