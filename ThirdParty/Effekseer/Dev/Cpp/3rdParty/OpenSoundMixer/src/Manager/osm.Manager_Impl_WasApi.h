
#pragma once

#include "../Filter/osm.Resampler.h"
#include "../osm.Manager_Impl.h"

#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <thread>

namespace osm {
class Manager_Impl_WasApi : public Manager_Impl {
private:
    IMMDeviceEnumerator* m_context = nullptr;
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_audioRender = nullptr;
    WAVEFORMATEXTENSIBLE m_format;
    Resampler m_resampler;

    volatile bool m_threading;
    std::thread m_thread;

    HANDLE m_event = NULL;
    HANDLE m_audioProcessingDoneEvent = NULL;
    bool initializingCo = false;

	int32_t channels_ = 2;
    int32_t samplerPerBit_ = 16;

	//! buffer for converting
    std::vector<Sample> processingBuffer_;

    void Reset();

    static void ThreadFunc(void* p);

public:
    Manager_Impl_WasApi();
    virtual ~Manager_Impl_WasApi();

    bool InitializeInternal() override;
    void FinalizeInternal() override;
};
}  // namespace osm