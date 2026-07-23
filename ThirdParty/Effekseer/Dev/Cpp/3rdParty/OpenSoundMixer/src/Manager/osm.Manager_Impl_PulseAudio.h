
#pragma once

#include "../osm.Manager_Impl.h"

#include <pulse/pulseaudio.h>
#include <thread>

#define FUNCPTR_TYPE(n) decltype(n)*
#define FUNCPTR_MV(n) FUNCPTR_TYPE(n) m_##n = nullptr;
#define LOAD_FUNCPTR_MV(n) { m_##n=(FUNCPTR_TYPE(n))dlsym(m_dll,#n); if(m_##n==nullptr){printf(#n" is not found.\n"); return false; }}
#define CALL_FUNCPTR_MV(n) m_##n
namespace osm
{
	class Manager_Impl_PulseAudio
		: public Manager_Impl
	{
	private:
		void*				m_dll;
		volatile bool			m_threading;
		std::thread			m_thread;

		pa_mainloop*			m_mainLoop;
		pa_context*			m_context;
		pa_stream*			m_stream;

		FUNCPTR_MV(pa_mainloop_new)
		FUNCPTR_MV(pa_context_new)
		FUNCPTR_MV(pa_context_connect)
		FUNCPTR_MV(pa_mainloop_get_api)
		FUNCPTR_MV(pa_mainloop_iterate)
		FUNCPTR_MV(pa_context_get_state)
		FUNCPTR_MV(pa_stream_new)
		FUNCPTR_MV(pa_stream_connect_playback)
		FUNCPTR_MV(pa_stream_get_state)
		FUNCPTR_MV(pa_stream_writable_size)
		FUNCPTR_MV(pa_stream_write)
		FUNCPTR_MV(pa_stream_disconnect)
		FUNCPTR_MV(pa_stream_unref)
		FUNCPTR_MV(pa_context_disconnect)
		FUNCPTR_MV(pa_context_unref)
		FUNCPTR_MV(pa_mainloop_free)
		FUNCPTR_MV(pa_stream_get_underflow_index)

		void Reset();

		static void ThreadFunc(void* p);

	public:
		Manager_Impl_PulseAudio();
		virtual ~Manager_Impl_PulseAudio();

		bool InitializeInternal() override;
		void FinalizeInternal() override;
	};
}
