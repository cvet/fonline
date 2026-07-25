
#pragma once

#include "../osm.Manager_Impl.h"

#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <thread>
#include <vector>

namespace osm
{
	class Manager_Impl_OpenAL
		: public Manager_Impl
	{
	private:
		volatile bool		m_threading;
		std::thread			m_thread;
		ALCdevice*			m_device;
		ALCcontext*			m_context;
		ALuint				m_source;
		std::vector<ALuint>	m_buffers;

		void Reset();

		static void ThreadFunc(void* p);

		void ThreadFuncInternal();

	public:
		Manager_Impl_OpenAL();
		virtual ~Manager_Impl_OpenAL();

		bool InitializeInternal() override;
		void FinalizeInternal() override;
	};
}
