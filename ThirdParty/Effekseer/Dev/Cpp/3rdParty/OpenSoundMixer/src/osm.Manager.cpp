
#include "OpenSoundMixer.h"

#if defined(_OTHER)
#include "Manager/osm.Manager_Impl_Other.h"
#elif _WIN32
#include "Manager/osm.Manager_Impl_WasApi.h"
#elif defined(__APPLE__)
#include "Manager/osm.Manager_Impl_OpenAL.h"
#else
#include "Manager/osm.Manager_Impl_PulseAudio.h"
#endif

namespace osm {

static std::function<void(LogType, const char*)> g_logger;

void SetLogger(const std::function<void(LogType, const char*)>& logger) { g_logger = logger; }

void Log(LogType logType, const char* message) {
    if (g_logger != nullptr) {
        g_logger(logType, message);
    }
}

Manager* Manager::Create() {
#if defined(_OTHER)
    auto manager = new osm::Manager_Impl_Other();
#elif _WIN32
    auto manager = new osm::Manager_Impl_WasApi();
#elif defined(__APPLE__)
    auto manager = new osm::Manager_Impl_OpenAL();
#else
    auto manager = new osm::Manager_Impl_PulseAudio();
#endif
    return manager;
}
}  // namespace osm
