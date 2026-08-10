
#include "osm.Manager_Impl_WasApi.h"

namespace osm {
void Manager_Impl_WasApi::Reset() {
    if (m_threading) {
        m_threading = false;
        m_thread.join();
    }

    if (m_audioRender) {
        m_audioRender->Release();
        m_audioRender = nullptr;
    }

    if (m_audioClient) {
        m_audioClient->Stop();
        m_audioClient->Release();
        m_audioClient = nullptr;
    }
}

static void Convert(Sample4ch32& dst, Sample& src) {
    int ep = 1000;
    dst.Values[0] = src.Left / 32767.0f * (2147483647.0f - ep);
    dst.Values[1] = src.Left / 32767.0f * (2147483647.0f - ep);
    dst.Values[2] = src.Right / 32767.0f * (2147483647.0f - ep);
    dst.Values[3] = src.Right / 32767.0f * (2147483647.0f - ep);
}

static void Convert(Sample32& dst, Sample& src) {
    int ep = 1000;
    dst.Left = src.Left / 32767.0f * (2147483647.0f - ep);
    dst.Right = src.Right / 32767.0f * (2147483647.0f - ep);
}

void Manager_Impl_WasApi::ThreadFunc(void* p) {
    Manager_Impl_WasApi* this_ = (Manager_Impl_WasApi*)p;

    // Start device
    this_->m_audioClient->Start();

    const int32_t bufferDivision = 100;

    bool requiredResampling = (this_->m_resampler.GetResampleRatio() != 1.0f);

    while (this_->m_threading && WAIT_OBJECT_0 != WaitForSingleObject(this_->m_audioProcessingDoneEvent, 0)) {
        // Wait
        {
            DWORD retval = WaitForSingleObject(this_->m_event, INFINITE);
            if (retval != WAIT_OBJECT_0) {
                break;
            }
        }

        uint32_t bufferFrames = 0;
        this_->m_audioClient->GetBufferSize(&bufferFrames);

        // decrease padding
        UINT32 padding = 0;
        if (FAILED(this_->m_audioClient->GetCurrentPadding(&padding))) {
            continue;
        }

        bufferFrames -= padding;

        uint8_t* outputBuffer = nullptr;
        this_->m_audioRender->GetBuffer(bufferFrames, (BYTE**)&outputBuffer);
        if (outputBuffer == NULL) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const uint32_t unitSize = 44100 / bufferDivision;
        int32_t outputCount = 0;
        Sample readBuffer[unitSize];

        while (bufferFrames - outputCount > 0) {
            if (requiredResampling) {
                auto requiredSize = bufferFrames - outputCount;
                requiredSize = (requiredSize * (44100.0 / 48000.0));
                requiredSize = std::min(unitSize, requiredSize);

                int32_t sampleCount = this_->ReadSamples(readBuffer, requiredSize);

                int32_t processedCount = bufferFrames - outputCount;
                if (this_->samplerPerBit_ == 16) {
                    auto target = reinterpret_cast<Sample*>(outputBuffer);
                    auto result = this_->m_resampler.ProcessSamples(readBuffer, sampleCount, target + outputCount, processedCount);
                    outputCount += result.second;
                    if (result.second == 0) break;
                } else {
                    this_->processingBuffer_.resize(processedCount);

                    auto result = this_->m_resampler.ProcessSamples(readBuffer, sampleCount, this_->processingBuffer_.data(), processedCount);

                    if (this_->channels_ == 2 && this_->samplerPerBit_ == 32) {
                        auto target = reinterpret_cast<Sample32*>(outputBuffer);
                        for (int i = 0; i < result.second; i++) {
                            Convert(target[i + outputCount], this_->processingBuffer_[i]);
                        }
                    } else if (this_->channels_ == 4 && this_->samplerPerBit_ == 32) {
                        auto target = reinterpret_cast<Sample4ch32*>(outputBuffer);
                        for (int i = 0; i < result.second; i++) {
                            Convert(target[i + outputCount], this_->processingBuffer_[i]);
                        }
                    }

                    outputCount += result.second;
                    if (result.second == 0) break;
                }

            } else {
                if (this_->samplerPerBit_ == 16) {
                    auto target = reinterpret_cast<Sample*>(outputBuffer);
                    auto requiredSize = std::min(unitSize, bufferFrames - outputCount);
                    int32_t sampleCount = this_->ReadSamples(target + outputCount, requiredSize);
                    outputCount += sampleCount;
                } else {
                    auto requiredSize = std::min(unitSize, bufferFrames - outputCount);
                    int32_t sampleCount = this_->ReadSamples(readBuffer, requiredSize);

                    if (this_->channels_ == 2 && this_->samplerPerBit_ == 32) {
                        auto target = reinterpret_cast<Sample32*>(outputBuffer);
                        for (int i = 0; i < sampleCount; i++) {
                            Convert(target[i + outputCount], readBuffer[i]);
                        }
                    } else if (this_->channels_ == 4 && this_->samplerPerBit_ == 32) {
                        auto target = reinterpret_cast<Sample4ch32*>(outputBuffer);
                        for (int i = 0; i < sampleCount; i++) {
                            Convert(target[i + outputCount], readBuffer[i]);
                        }
                    }

                    outputCount += sampleCount;
                }
            }
        }

        this_->m_audioRender->ReleaseBuffer((uint32_t)outputCount, 0);
    }
}

Manager_Impl_WasApi::Manager_Impl_WasApi() : m_threading(false) {}

Manager_Impl_WasApi::~Manager_Impl_WasApi() { Reset(); }

bool Manager_Impl_WasApi::InitializeInternal() {
    HRESULT hr;

    hr = ::CoInitialize(NULL);

    initializingCo = SUCCEEDED(hr);

    // Generate handle
    m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_audioProcessingDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Get the audio endpoint device enumerator.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_context);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : CoCreateInstance");
        return false;
    }

    hr = m_context->GetDefaultAudioEndpoint(eRender, eMultimedia, &m_device);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : IMMDeviceEnumerator::GetDefaultAudioEndpoint");
        return false;
    }

    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : IMMDevice::Activate");
        return false;
    }

    // Get the device latency
    REFERENCE_TIME defaultDevicePeriod = 0;
    REFERENCE_TIME minimumDevicePeriod = 0;
    hr = m_audioClient->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : AudioClient::GetDevicePeriod");
        return false;
    }

    // Get the device format
    WAVEFORMATEX* deviceFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&deviceFormat);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : AudioClient::GetMixFormat");
        return false;
    }

    // Set the output format
    WAVEFORMATEX format = *deviceFormat;
    CoTaskMemFree(deviceFormat);
    m_format.Format = format;

    m_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    m_format.Format.nChannels = 2;
    m_format.Format.wBitsPerSample = 16;
    //// m_format.Format.nSamplesPerSec = 48000;	// Need to use device sample rate
    m_format.Format.nBlockAlign = m_format.Format.nChannels * m_format.Format.wBitsPerSample / 8;
    m_format.Format.nAvgBytesPerSec = m_format.Format.nBlockAlign * m_format.Format.nSamplesPerSec;
    m_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    m_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
    m_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 40 * 1000 * 10, 0, &m_format.Format, NULL);
    if (FAILED(hr)) {
        UINT32 bufferSize = 0;
        m_audioClient->GetBufferSize(&bufferSize);

        defaultDevicePeriod = (REFERENCE_TIME)(10000.0 * 1000 * bufferSize / m_format.Format.nSamplesPerSec + 0.5);
        SafeRelease(m_audioClient);

        m_format.Format = format;
        m_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
        m_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

        hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
        hr = m_audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, defaultDevicePeriod, 0, &m_format.Format, NULL);

        Log(LogType::Error, std::to_string(defaultDevicePeriod).c_str());

        if (FAILED(hr)) {
            Log(LogType::Error, "Failed : AudioClient::Initialize");
            return false;
        }
    }

    samplerPerBit_ = m_format.Format.wBitsPerSample;
    channels_ = m_format.Format.nChannels;

    if ((channels_ == 2 && samplerPerBit_ == 16) || (channels_ == 2 && samplerPerBit_ == 32) || (channels_ == 4 && samplerPerBit_ == 32)) {
    } else {
        Log(LogType::Error, "Failed : Not supported.");
        return false;
	}

    hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_audioRender);
    if (FAILED(hr)) {
        Log(LogType::Error, "Failed : AudioClient::GetService");
        return false;
    }

    m_audioClient->SetEventHandle(m_event);

    // Set sample rate converter
    m_resampler.SetResampleRatio(m_format.Format.nSamplesPerSec / 44100.0);

    m_threading = true;
    m_thread = std::thread(ThreadFunc, this);
    SetThreadPriority((HANDLE)m_thread.native_handle(), THREAD_PRIORITY_HIGHEST);

    return true;
}

void Manager_Impl_WasApi::FinalizeInternal() {
    Reset();

    SafeRelease(m_context);

    CloseHandle(m_event);

    if (initializingCo) {
        ::CoUninitialize();
    }
}
}  // namespace osm