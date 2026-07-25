
#include "osm.Decorder.h"

namespace osm {
eFileType Decorder::GetFileType(const void* data, int32_t size) {
    if (size < 4) return eFileType::Unknown;

    char head[5];
    head[4] = 0;
    memcpy(head, data, 4);

    if (STRICMP("RIFF", head) == 0) {
        return eFileType::WAVE;
    } else if (STRICMP("OggS", head) == 0) {
        return eFileType::OGG;
    }

    return eFileType::Unknown;
}
}  // namespace osm
