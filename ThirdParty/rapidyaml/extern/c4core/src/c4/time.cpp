// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "c4/time.hpp"

#if defined(C4_WIN)
#   include "c4/windows.hpp"
#elif defined(C4_POSIX)
#   include <time.h>
#else
#   include <chrono>
#endif

C4_BEGIN_NAMESPACE(c4)

//-----------------------------------------------------------------------------
/** a general-use time stamp in microseconds (usecs).
 * Although this is timed precisely, there may be some issues.
 * Eg, concurrent or heavy use may cause penalties.
 * @see http://stackoverflow.com/questions/2414359/microsecond-resolution-timestamps-on-windows
 * @see https://www.strchr.com/performance_measurements_with_rdtsc
 * @see https://msdn.microsoft.com/en-us/library/windows/desktop/ee417693(v=vs.85).aspx */
time_type currtime()
{
#ifdef C4_WIN
    time_type ifreq;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    ifreq = time_type(1.e6) / time_type(freq.QuadPart);
    LARGE_INTEGER ts;
    QueryPerformanceCounter(&ts);
    time_type usecs = time_type(ts.QuadPart) * ifreq;
    return usecs;
#elif defined(C4_POSIX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    time_type usecs = time_type(1.e6) * time_type(ts.tv_sec)
                    + time_type(1.e-3) * time_type(ts.tv_nsec);
    return usecs;
#else
    auto time_point = std::chrono::high_resolution_clock::now();
    auto duration = time_point.time_since_epoch();
    auto usecs_d = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    time_type usecs = (time_type)usecs_d.count();
    return usecs;
#endif
}

C4_END_NAMESPACE(c4)
