#include "Threading.h"
#include "StringUtils.h"
#include "Testing.h"

#if defined(FO_WINDOWS) || defined(FO_LINUX) || defined(FO_MAC)

THREAD char Thread::threadName[64] = {0};
SizeTStrMap Thread::threadNames;
Mutex Thread::threadNamesLocker;

static void* ThreadBeginExecution(Thread::ThreadFunc func, char* name, void* arg)
{
    Thread::SetName(name);
    delete[] name;
    func(arg);
    return nullptr;
}

Thread::~Thread()
{
    if (thread.joinable())
        thread.detach();
}

void Thread::Start(ThreadFunc func, const string& name, void* arg /* = nullptr */)
{
    std::thread t(ThreadBeginExecution, func, Str::Duplicate(name), arg);
    thread.swap(t);
}

void Thread::Wait()
{
    if (thread.joinable())
        thread.join();
}

size_t Thread::GetId()
{
#ifdef FO_WINDOWS
    return (size_t)GetCurrentThreadId();
#else
    return (size_t)pthread_self();
#endif
}

void Thread::SetName(const char* name)
{
    if (threadName[0])
        return;

    Str::Copy(threadName, name);
    SCOPE_LOCK(threadNamesLocker);
    threadNames.insert(std::make_pair(GetId(), threadName));
}

const char* Thread::GetName()
{
    return threadName;
}

const char* Thread::FindName(size_t thread_id)
{
    SCOPE_LOCK(threadNamesLocker);
    auto it = threadNames.find(thread_id);
    return it != threadNames.end() ? it->second.c_str() : nullptr;
}

void Thread::Sleep(uint ms)
{
#if defined(FO_WINDOWS)
    SleepEx(ms, FALSE);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&req, &req) == -1 && errno == EINTR)
        continue;
#endif
}

#else

void Thread::Start(ThreadFunc func, const string& name, void* arg /* = nullptr */)
{
    UNREACHABLE_PLACE;
}

void Thread::Wait()
{
    UNREACHABLE_PLACE;
}

size_t Thread::GetId()
{
    return 1;
}

void Thread::SetName(const char* name)
{
}

const char* Thread::GetName()
{
    return "Main";
}

const char* Thread::FindName(size_t thread_id)
{
    return "Main";
}

void Thread::Sleep(uint ms)
{
}

#endif
