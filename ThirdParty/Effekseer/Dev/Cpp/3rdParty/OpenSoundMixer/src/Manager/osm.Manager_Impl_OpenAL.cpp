
#include "osm.Manager_Impl_OpenAL.h"
#include <iostream>

namespace osm
{
	static const ALenum format = AL_FORMAT_STEREO16;
	static const ALsizei freq  = 44100;

	static const size_t queueSize      = 8;
	static const size_t bufferDivision = 100;
	static const size_t bufferSize     = freq / bufferDivision;
	static const size_t bufferBytes    = sizeof(Sample) * bufferSize;

	void Manager_Impl_OpenAL::Reset()
	{
		if (m_threading)
		{
			m_threading = false;
			m_thread.join();
		}

		if (!m_buffers.empty())
		{
			alDeleteBuffers(m_buffers.size(), &m_buffers[0]);
			m_buffers.clear();
		}

		if (m_source)
		{
			alDeleteSources(1, &m_source);
			m_source = 0;
		}

		alcMakeContextCurrent(NULL);

		if (m_context != nullptr)
		{
			alcDestroyContext(m_context);
			m_context = nullptr;
		}

		if (m_device != nullptr)
		{
			alcCloseDevice(m_device);
			m_device = nullptr;
		}
	}

	void Manager_Impl_OpenAL::ThreadFuncInternal()
	{
		Sample bufs[queueSize][bufferSize];
		int32_t targetBuf = 0;
        
		while (m_threading)
		{
            // バッファにデータを読み込みソースに追加
            ALint queue;
            alGetSourceiv(m_source, AL_BUFFERS_QUEUED, &queue);
            
            if (!queue) {
                for (auto b : m_buffers) {
                    auto sampleCount = ReadSamples(bufs[targetBuf], bufferSize);
                    auto sampleBytes = sampleCount * sizeof(Sample);
                    
                    alBufferData(b, format, bufs[targetBuf], sampleBytes, freq);
                    alSourceQueueBuffers(m_source, 1, &b);
                    targetBuf = (targetBuf + 1) % queueSize;
                }
            }

			// 再生状態にする
			ALint state;
			alGetSourcei(m_source, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING)
			{
				alSourcePlay(m_source);
			}

			// 再生終了したバッファにデータを読み込み
			ALint processed;
			alGetSourceiv(m_source, AL_BUFFERS_PROCESSED, &processed);
            
            //std::cout << "f " << state << " " << queue << " " << processed << std::endl;
            
			while (processed > 0)
			{
				ALuint unqueued;
				alSourceUnqueueBuffers(m_source, 1, &unqueued);

				auto sampleCount = ReadSamples(bufs[targetBuf], bufferSize);
				auto sampleBytes = sampleCount * sizeof(Sample);

				alBufferData(unqueued, format, bufs[targetBuf], sampleBytes, freq);
				alSourceQueueBuffers(m_source, 1, &unqueued);
				targetBuf = (targetBuf + 1) % queueSize;
				--processed;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// 終わりまで待つ
		while (true)
		{
			ALint queued;
			alGetSourceiv(m_source, AL_BUFFERS_QUEUED, &queued);
			if (!queued)
			{
				break;
			}

			ALint processed;
			alGetSourceiv(m_source, AL_BUFFERS_PROCESSED, &processed);
			while (processed > 0)
			{
				ALuint unqueued;
				alSourceUnqueueBuffers(m_source, 1, &unqueued);
				--processed;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void Manager_Impl_OpenAL::ThreadFunc(void* p)
	{
		Manager_Impl_OpenAL* this_ = (Manager_Impl_OpenAL*) p;
		this_->ThreadFuncInternal();
	}

	Manager_Impl_OpenAL::Manager_Impl_OpenAL()
		: m_device(nullptr)
		, m_context(nullptr)
		, m_source(0)
		, m_threading(false)
	{

	}

	Manager_Impl_OpenAL::~Manager_Impl_OpenAL()
	{
		Reset();
	}

	bool Manager_Impl_OpenAL::InitializeInternal()
	{
		m_device = alcOpenDevice(NULL);
		if (!m_device) return false;

		m_context = alcCreateContext(m_device, NULL);
		if (!m_context) return false;

		alcMakeContextCurrent(m_context);

		alGenSources(1, &m_source);
		if (!m_source) return false;

		m_buffers.resize(queueSize);
		alGenBuffers(queueSize, &m_buffers[0]);

		alSourcei(m_source, AL_LOOPING, AL_FALSE);

		m_threading = true;
		m_thread = std::thread(ThreadFunc, this);

		return true;
	}

	void Manager_Impl_OpenAL::FinalizeInternal()
	{
		Reset();
	}
}
