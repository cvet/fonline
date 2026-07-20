
#include "osm.Manager_Impl_PulseAudio.h"

#include <dlfcn.h>

namespace osm
{
	void Manager_Impl_PulseAudio::Reset()
	{
		if (m_threading)
		{
			m_threading = false;
			m_thread.join();
		}
		
		if(m_stream != nullptr)
		{
			CALL_FUNCPTR_MV(pa_stream_disconnect)(m_stream);
			CALL_FUNCPTR_MV(pa_stream_unref)(m_stream);
			m_stream = nullptr;
		}

		if(m_context != nullptr)
		{
			CALL_FUNCPTR_MV(pa_context_disconnect)(m_context);
			CALL_FUNCPTR_MV(pa_context_unref)(m_context);
			m_context = nullptr;
		}
		
		if(m_mainLoop != nullptr)
		{
			CALL_FUNCPTR_MV(pa_mainloop_free)(m_mainLoop);
			m_mainLoop = nullptr;
		}
	}

	void Manager_Impl_PulseAudio::ThreadFunc(void* p)
	{
		Manager_Impl_PulseAudio* this_ = (Manager_Impl_PulseAudio*) p;
		bool hasError = false;

		auto iterate = [this_]() -> void 
		{
			this_->CALL_FUNCPTR_MV(pa_mainloop_iterate)(this_->m_mainLoop,0,NULL);
		};
	
		const int32_t bufferDivision = 100;

		Sample bufs[44100 / bufferDivision];

		while (this_->m_threading && !hasError)
		{
			if(PA_STREAM_READY==this_->CALL_FUNCPTR_MV(pa_stream_get_state)(this_->m_stream))
			{
				size_t writableSize = this_->CALL_FUNCPTR_MV(pa_stream_writable_size)(this_->m_stream);
			
				if( writableSize == 0)
				{
					iterate();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
	
				if (writableSize > 44100 / bufferDivision * sizeof(Sample)) writableSize = 44100 / bufferDivision * sizeof(Sample);

				auto sampleCount = this_->ReadSamples(bufs, writableSize / sizeof(Sample) );
				auto sampleBytes = sampleCount * sizeof(Sample);
				this_->CALL_FUNCPTR_MV(pa_stream_write)(
					this_->m_stream,
					bufs,
					sampleBytes,
					NULL,
					0,
					PA_SEEK_RELATIVE);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			if(0<=this_->CALL_FUNCPTR_MV(pa_stream_get_underflow_index)(this_->m_stream))
			{
				hasError = true;
				printf("Underflow is detected.");
			}
		
			iterate();
		}

		// 終わりまで待つ
		while (!hasError)
		{
			size_t writableSize = this_->CALL_FUNCPTR_MV(pa_stream_writable_size)(this_->m_stream);
			if(writableSize == 0)
			{
				iterate();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			else
			{
				break;
			}
		}
	}

	Manager_Impl_PulseAudio::Manager_Impl_PulseAudio()
		: m_dll(nullptr)
		, m_threading(false)
		, m_mainLoop(nullptr)
		, m_context(nullptr)
		, m_stream(nullptr)
	{

	}

	Manager_Impl_PulseAudio::~Manager_Impl_PulseAudio()
	{
		Reset();

		if(m_dll != nullptr)
		{
			dlclose(m_dll);
			m_dll = nullptr;
		}
	}

	bool Manager_Impl_PulseAudio::InitializeInternal()
	{
		// load functions
		m_dll = dlopen("libpulse.so.0", RTLD_LAZY);
		if(m_dll==nullptr) return false;
		
		LOAD_FUNCPTR_MV(pa_mainloop_new)
		LOAD_FUNCPTR_MV(pa_context_new)
		LOAD_FUNCPTR_MV(pa_context_connect)
		LOAD_FUNCPTR_MV(pa_mainloop_get_api)
		LOAD_FUNCPTR_MV(pa_mainloop_iterate)
		LOAD_FUNCPTR_MV(pa_context_get_state)
		LOAD_FUNCPTR_MV(pa_stream_new)
		LOAD_FUNCPTR_MV(pa_stream_connect_playback)
		LOAD_FUNCPTR_MV(pa_stream_get_state)
		LOAD_FUNCPTR_MV(pa_stream_writable_size)
		LOAD_FUNCPTR_MV(pa_stream_write)
		LOAD_FUNCPTR_MV(pa_stream_disconnect)
		LOAD_FUNCPTR_MV(pa_stream_unref)
		LOAD_FUNCPTR_MV(pa_context_disconnect)
		LOAD_FUNCPTR_MV(pa_context_unref)
		LOAD_FUNCPTR_MV(pa_mainloop_free)		
		LOAD_FUNCPTR_MV(pa_stream_get_underflow_index)

		m_mainLoop = CALL_FUNCPTR_MV(pa_mainloop_new)();
		if( m_mainLoop == nullptr ) return false;

		m_context = CALL_FUNCPTR_MV(pa_context_new)(CALL_FUNCPTR_MV(pa_mainloop_get_api)(m_mainLoop), "OSM" );
		if( m_context == nullptr ) return false;

		CALL_FUNCPTR_MV(pa_context_connect)(m_context, NULL,(pa_context_flags_t)0, NULL);

		int32_t readyCount = 0;
		while(true)
		{
			CALL_FUNCPTR_MV(pa_mainloop_iterate)(m_mainLoop,0,NULL);

			if(PA_CONTEXT_READY==CALL_FUNCPTR_MV(pa_context_get_state)(m_context))
			{
				break;
			}
			else
			{
				readyCount++;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				if(readyCount>1000) break;
			}
		}

		if(readyCount>1000) return false;

		const pa_sample_spec spec =
		{
			PA_SAMPLE_S16LE,
			44100,
			2
		};

		m_stream = CALL_FUNCPTR_MV(pa_stream_new)(m_context,"OSMStream",&spec,NULL);
		if( m_stream == nullptr ) return false;

		CALL_FUNCPTR_MV(pa_stream_connect_playback)(m_stream,NULL,NULL,(pa_stream_flags_t)0,NULL,NULL);

		m_threading = true;
		m_thread = std::thread(ThreadFunc, this);
		return true;
	}

	void Manager_Impl_PulseAudio::FinalizeInternal()
	{
		Reset();
	}
}
