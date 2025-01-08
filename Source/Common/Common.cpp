//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Common.h"
#include "Application.h"
#include "DiskFileSystem.h"
#include "Log.h"
#include "Platform.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Version-Include.h"

#if FO_MAC
#include <sys/sysctl.h>
#endif

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>)
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#endif

#include "WinApiUndef-Include.h"

static bool ExceptionMessageBox = false;

// ReSharper disable once CppInconsistentNaming
const ucolor ucolor::clear;

hstring::entry hstring::_zeroEntry;

map<uint16, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int GlobalDataCallbacksCount;

static constexpr size_t STACK_TRACE_MAX_SIZE = 128;

struct StackTraceData
{
    size_t CallsCount = {};
    array<const SourceLocationData*, STACK_TRACE_MAX_SIZE> CallTree = {};
};

static thread_local StackTraceData StackTrace;

static StackTraceData* CrashStackTrace;

extern void SetCrashStackTrace() noexcept // Called in backward.hpp
{
    NO_STACK_TRACE_ENTRY();

    CrashStackTrace = &StackTrace;
}

template<typename T>
static char* ItoA(T num, char buf[64], int base) noexcept
{
    static_assert(std::is_integral_v<T>);
    static_assert(sizeof(T) <= 16);

    int i = 0;
    [[maybe_unused]] bool is_negative = false;

    if (num == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return buf;
    }

    if constexpr (std::is_signed_v<T>) {
        if (num < 0 && base == 10) {
            is_negative = true;
            num = -num;
        }
    }

    while (num != 0) {
        const int rem = num % base;
        buf[i++] = static_cast<char>(rem > 9 ? rem - 10 + 'a' : rem + '0');
        num = num / base;
    }

    if (is_negative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    int start = 0;
    int end = i - 1;

    while (start < end) {
        const char ch = buf[start];
        buf[start] = buf[end];
        buf[end] = ch;
        end--;
        start++;
    }

    return buf;
}

static void SafeWriteStackTrace(const StackTraceData* st) noexcept
{
    NO_STACK_TRACE_ENTRY();

    WriteLogFatalMessage("Stack trace (most recent call first):\n");

    char itoa_buf[64] = {};

    for (int i = std::min(static_cast<int>(st->CallsCount), static_cast<int>(STACK_TRACE_MAX_SIZE)) - 1; i >= 0; i--) {
        const auto& entry = st->CallTree[i];
        WriteLogFatalMessage("- ");
        WriteLogFatalMessage(entry->function);
        WriteLogFatalMessage(" (");
        WriteLogFatalMessage(strex(entry->file).extractFileName());
        WriteLogFatalMessage(" line ");
        WriteLogFatalMessage(ItoA(entry->line, itoa_buf, 10));
        WriteLogFatalMessage(")\n");
    }

    if (st->CallsCount > STACK_TRACE_MAX_SIZE) {
        WriteLogFatalMessage("- ...and ");
        WriteLogFatalMessage(ItoA(st->CallsCount - STACK_TRACE_MAX_SIZE, itoa_buf, 10));
        WriteLogFatalMessage(" more entries\n");
    }

    WriteLogFatalMessage("\n");
}

class BackwardOStreamBuffer : public std::streambuf
{
public:
    BackwardOStreamBuffer() = default;
    BackwardOStreamBuffer(const BackwardOStreamBuffer&) = delete;
    BackwardOStreamBuffer(BackwardOStreamBuffer&&) noexcept = delete;
    auto operator=(const BackwardOStreamBuffer&) = delete;
    auto operator=(BackwardOStreamBuffer&&) noexcept -> BackwardOStreamBuffer& = delete;
    ~BackwardOStreamBuffer() override = default;

    auto underflow() -> int_type override
    {
        //
        return traits_type::eof();
    }

    auto overflow(int_type ch) -> int_type override
    {
        const char s[] = {static_cast<char>(ch)};
        WriteLogFatalMessage(string_view {s, 1});
        return ch;
    }

    auto xsputn(const char_type* s, std::streamsize count) -> std::streamsize override /*noexcept*/
    {
        if (_firstCall) {
            WriteHeader();
            _firstCall = false;
        }

        WriteLogFatalMessage(string_view {s, static_cast<string_view::size_type>(count)});
        return count;
    }

private:
    void WriteHeader() const noexcept
    {
        WriteLogFatalMessage("\nFATAL ERROR!\n\n");

        if (CrashStackTrace != nullptr) {
            SafeWriteStackTrace(CrashStackTrace);
        }
    }

    bool _firstCall = true;
};

static BackwardOStreamBuffer BackwardOStreamBuf;
extern std::ostream BackwardOStream = std::ostream(&BackwardOStreamBuf); // Passed to Printer::print in backward.hpp

extern void CreateGlobalData()
{
    STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        CreateGlobalDataCallbacks[i]();
    }

    InitBackupMemoryChunks();
}

extern void DeleteGlobalData()
{
    STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        DeleteGlobalDataCallbacks[i]();
    }
}

static auto InsertCatchedMark(const string& st) -> string
{
    NO_STACK_TRACE_ENTRY();

    const auto catched_st = GetStackTrace();

    // Skip 'Stack trace (most recent ...'
    auto pos = catched_st.find('\n');

    if (pos == string::npos) {
        return st;
    }

    // Find stack traces intercection
    pos = st.find(catched_st.substr(pos + 1));

    if (pos == string::npos) {
        return st;
    }

    // Insert in end of line
    pos = st.find('\n', pos);
    return st.substr(0, pos).append(" <- Catched here").append(pos != string::npos ? st.substr(pos) : "");
}

extern void ReportExceptionAndExit(const std::exception& ex) noexcept
{
    NO_STACK_TRACE_ENTRY();

    try {
        const auto* base_engine_ex = dynamic_cast<const BaseEngineException*>(&ex);

        if (base_engine_ex != nullptr) {
            WriteLog(LogType::Error, "{}\n{}\nShutdown!", ex.what(), InsertCatchedMark(base_engine_ex->StackTrace()));
        }
        else {
            WriteLog(LogType::Error, "{}\nCatched at: {}\nShutdown!", ex.what(), GetStackTrace());
        }

        if (BreakIntoDebugger(ex.what())) {
            ExitApp(false);
        }

        CreateDumpMessage("FatalException", ex.what());

        if (base_engine_ex != nullptr) {
            MessageBox::ShowErrorMessage(ex.what(), InsertCatchedMark(base_engine_ex->StackTrace()), true);
        }
        else {
            MessageBox::ShowErrorMessage(ex.what(), strex("Catched at: {}", GetStackTrace()), true);
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }

    ExitApp(false);
}

extern void ReportExceptionAndContinue(const std::exception& ex) noexcept
{
    NO_STACK_TRACE_ENTRY();

    try {
        const auto* base_engine_ex = dynamic_cast<const BaseEngineException*>(&ex);

        if (base_engine_ex != nullptr) {
            WriteLog(LogType::Error, "{}\n{}", ex.what(), InsertCatchedMark(base_engine_ex->StackTrace()));
        }
        else {
            WriteLog(LogType::Error, "{}\nCatched at: {}", ex.what(), GetStackTrace());
        }

        if (BreakIntoDebugger(ex.what())) {
            return;
        }

        if (ExceptionMessageBox) {
            if (base_engine_ex != nullptr) {
                MessageBox::ShowErrorMessage(ex.what(), InsertCatchedMark(base_engine_ex->StackTrace()), false);
            }
            else {
                MessageBox::ShowErrorMessage(ex.what(), strex("Catched at: {}", GetStackTrace()), false);
            }
        }
    }
    catch (...) {
        BreakIntoDebugger();
    }
}

extern void ShowExceptionMessageBox(bool enabled) noexcept
{
    NO_STACK_TRACE_ENTRY();

    ExceptionMessageBox = enabled;
}

extern void ReportStrongAssertAndExit(string_view message, const char* file, int line) noexcept
{
    NO_STACK_TRACE_ENTRY();

    try {
        throw StrongAssertationException(message, file, line);
    }
    catch (StrongAssertationException& ex) {
        ReportExceptionAndExit(ex);
    }
}

extern void ReportVerifyFailed(string_view message, const char* file, int line) noexcept
{
    NO_STACK_TRACE_ENTRY();

    try {
        throw VerifyFailedException(message, file, line);
    }
    catch (VerifyFailedException& ex) {
        ReportExceptionAndContinue(ex);
    }
}

extern void PushStackTrace(const SourceLocationData& loc) noexcept
{
    NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    auto& st = StackTrace;

    if (st.CallsCount < STACK_TRACE_MAX_SIZE) {
        st.CallTree[st.CallsCount] = &loc;
    }

    st.CallsCount++;
#endif
}

extern void PopStackTrace() noexcept
{
    NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    auto& st = StackTrace;

    if (st.CallsCount > 0) {
        st.CallsCount--;
    }
#endif
}

extern auto GetStackTrace() -> string
{
    NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    std::ostringstream ss;

    ss << "Stack trace (most recent call first):\n";

    const auto& st = StackTrace;

    for (int i = std::min(static_cast<int>(st.CallsCount), static_cast<int>(STACK_TRACE_MAX_SIZE)) - 1; i >= 0; i--) {
        const auto& entry = st.CallTree[i];

        ss << "- " << entry->function << " (" << strex(entry->file).extractFileName().strv() << " line " << entry->line << ")\n";
    }

    if (st.CallsCount > STACK_TRACE_MAX_SIZE) {
        ss << "- ...and " << (st.CallsCount - STACK_TRACE_MAX_SIZE) << " more entries\n";
    }

    std::string st_str = ss.str();

    if (!st_str.empty() && st_str.back() == '\n') {
        st_str.pop_back();
    }

    return string(st_str);

#else
    return GetRealStackTrace();
#endif
}

extern auto GetStackTraceEntry(size_t deep) noexcept -> const SourceLocationData*
{
    NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    const auto& st = StackTrace;

    if (deep < st.CallsCount) {
        return st.CallTree[st.CallsCount - 1 - deep];
    }
    else {
        return nullptr;
    }

#else

    return nullptr;
#endif
}

extern auto GetRealStackTrace() -> string
{
    NO_STACK_TRACE_ENTRY();

    if (IsRunInDebugger()) {
        return "Stack trace disabled (debugger detected)";
    }

#if FO_WINDOWS || FO_LINUX || FO_MAC
    backward::TraceResolver resolver;
    backward::StackTrace st;
    st.load_here(42);
    st.skip_n_firsts(2);

    std::ostringstream ss;

    ss << "Stack trace (most recent call first):\n";

    for (size_t i = 0; i < st.size(); ++i) {
        backward::ResolvedTrace trace = resolver.resolve(st[i]);

        auto obj_func = string(trace.object_function);

        if (obj_func.length() > 100) {
            obj_func.resize(97);
            obj_func.append("...");
        }

        string file_name = strex(trace.source.filename).extractFileName();

        if (!file_name.empty()) {
            file_name.append(" ");
        }

        file_name += strex("{}", trace.source.line);

        ss << "- " << obj_func << " (" << file_name << ")\n";
    }

    std::string st_str = ss.str();

    if (!st_str.empty() && st_str.back() == '\n') {
        st_str.pop_back();
    }

    return string(st_str);
#else

    return "Stack trace not supported";
#endif
}

static bool RunInDebugger = false;
static std::once_flag RunInDebuggerOnce;

extern auto IsRunInDebugger() noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

#if FO_WINDOWS
    std::call_once(RunInDebuggerOnce, [] { RunInDebugger = ::IsDebuggerPresent() != FALSE; });

#elif FO_LINUX
    std::call_once(RunInDebuggerOnce, [] {
        const auto status_fd = ::open("/proc/self/status", O_RDONLY);
        if (status_fd != -1) {
            char buf[4096] = {0};
            const auto num_read = ::read(status_fd, buf, sizeof(buf) - 1);
            ::close(status_fd);
            if (num_read > 0) {
                const auto* tracer_pid_str = ::strstr(buf, "TracerPid:");
                if (tracer_pid_str != nullptr) {
                    for (const char* s = tracer_pid_str + "TracerPid:"_len; s <= buf + num_read; ++s) {
                        if (::isspace(*s) == 0) {
                            RunInDebugger = ::isdigit(*s) != 0 && *s != '0';
                            break;
                        }
                    }
                }
            }
        }
    });

#elif FO_MAC
    std::call_once(RunInDebuggerOnce, [] {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info = {};
        size_t size = sizeof(info);
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            RunInDebugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });

#else
    UNUSED_VARIABLE(RunInDebuggerOnce);
#endif

    return RunInDebugger;
}

extern auto BreakIntoDebugger([[maybe_unused]] string_view error_message) noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    if (IsRunInDebugger()) {
#if FO_WINDOWS
        ::DebugBreak();
        return true;
#elif FO_LINUX
#ifdef __has_builtin
#if __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
#else
        ::raise(SIGTRAP);
#endif
#else
        ::raise(SIGTRAP);
#endif
        return true;
#elif FO_MAC
        __builtin_debugtrap();
        return true;
#endif
    }

    return false;
}

extern void CreateDumpMessage(string_view appendix, string_view message)
{
    STACK_TRACE_ENTRY();

    const auto traceback = GetStackTrace();
    const auto dt = Timer::GetCurrentDateTime();
    const string fname = strex("{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", FO_DEV_NAME, appendix, dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);

    if (auto file = DiskFileSystem::OpenFile(fname, true)) {
        file.Write(strex("{}\n", appendix));
        file.Write(strex("{}\n", message));
        file.Write(strex("\n"));
        file.Write(strex("Application\n"));
        file.Write(strex("\tName        {}\n", FO_DEV_NAME));
        file.Write(strex("\tVersion     {}\n", FO_GAME_VERSION));
        file.Write(strex("\tOS          Windows\n"));
        file.Write(strex("\tTimestamp   {:04}.{:02}.{:02} {:02}:{:02}:{:02}\n", dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second));
        file.Write(strex("\n"));
        file.Write(traceback);
        file.Write(strex("\n"));
    }
}

FrameBalancer::FrameBalancer(bool enabled, int sleep, int fixed_fps) :
    _enabled {enabled && (sleep >= 0 || fixed_fps > 0)},
    _sleep {sleep},
    _fixedFps {fixed_fps}
{
    STACK_TRACE_ENTRY();
}

auto FrameBalancer::GetLoopDuration() const -> time_duration
{
    STACK_TRACE_ENTRY();

    return _loopDuration;
}

void FrameBalancer::StartLoop()
{
    STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopStart = Timer::CurTime();
}

void FrameBalancer::EndLoop()
{
    STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopDuration = Timer::CurTime() - _loopStart;

    if (_sleep >= 0) {
        if (_sleep == 0) {
            std::this_thread::yield();
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
        }
    }
    else if (_fixedFps > 0) {
        const auto target_time = time_duration {std::chrono::nanoseconds {static_cast<uint64>(1000.0 / static_cast<double>(_fixedFps) * 1000000.0)}};
        const auto idle_time = target_time - _loopDuration + _idleTimeBalance;

        if (idle_time > std::chrono::milliseconds {0}) {
            const auto sleep_start = Timer::CurTime();

            std::this_thread::sleep_for(idle_time);

            const auto sleep_duration = Timer::CurTime() - sleep_start;

            _idleTimeBalance += (target_time - _loopDuration) - sleep_duration;
        }
        else {
            _idleTimeBalance += target_time - _loopDuration;

            if (_idleTimeBalance < -std::chrono::milliseconds {1000}) {
                _idleTimeBalance = -std::chrono::milliseconds {1000};
            }
        }
    }
}

WorkThread::WorkThread(string_view name)
{
    STACK_TRACE_ENTRY();

    _name = name;
    _thread = std::thread(&WorkThread::ThreadEntry, this);
}

WorkThread::~WorkThread()
{
    STACK_TRACE_ENTRY();

    {
        std::unique_lock locker(_dataLocker);

        _finish = true;
    }

    _workSignal.notify_one();

    try {
        _thread.join();
    }
    catch (const std::exception& ex) {
        ReportExceptionAndContinue(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

auto WorkThread::GetJobsCount() const -> size_t
{
    STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    return _jobs.size() + (_jobActive ? 1 : 0);
}

void WorkThread::SetExceptionHandler(ExceptionHandler handler)
{
    STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    _exceptionHandler = std::move(handler);
}

void WorkThread::AddJob(Job job)
{
    STACK_TRACE_ENTRY();

    AddJobInternal(std::chrono::milliseconds {0}, std::move(job), false);
}

void WorkThread::AddJob(time_duration delay, Job job)
{
    STACK_TRACE_ENTRY();

    AddJobInternal(delay, std::move(job), false);
}

void WorkThread::AddJobInternal(time_duration delay, Job job, bool no_notify)
{
    STACK_TRACE_ENTRY();

    {
        std::unique_lock locker(_dataLocker);

        const auto fire_time = Timer::CurTime() + delay;

        if (_jobs.empty() || fire_time >= _jobs.back().first) {
            _jobs.emplace_back(fire_time, std::move(job));
        }
        else {
            for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                if (fire_time < it->first) {
                    _jobs.emplace(it, fire_time, std::move(job));
                    break;
                }
            }
        }
    }

    if (!no_notify) {
        _workSignal.notify_one();
    }
}

void WorkThread::Clear()
{
    STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    _clearJobs = true;

    locker.unlock();
    _workSignal.notify_one();
    locker.lock();

    while (_clearJobs) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::Wait() const
{
    STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    while (!_jobs.empty() || _jobActive) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::ThreadEntry() noexcept
{
    STACK_TRACE_ENTRY();

    try {
        SetThisThreadName(_name);

        while (true) {
            Job job;

            {
                std::unique_lock locker(_dataLocker);

                _jobActive = false;

                if (_clearJobs) {
                    _jobs.clear();
                    _clearJobs = false;
                }

                if (_jobs.empty()) {
                    locker.unlock();
                    _doneSignal.notify_all();
                    locker.lock();
                }

                if (_jobs.empty()) {
                    if (_clearJobs) {
                        _clearJobs = false;
                    }

                    if (_finish) {
                        break;
                    }

                    _workSignal.wait(locker);
                }

                if (!_jobs.empty()) {
                    const auto cur_time = Timer::CurTime();

                    for (auto it = _jobs.begin(); it != _jobs.end(); ++it) {
                        if (cur_time >= it->first) {
                            job = std::move(it->second);
                            _jobs.erase(it);
                            _jobActive = true;
                            break;
                        }
                    }
                }
            }

            if (job) {
                try {
                    const auto next_call_delay = job();

                    // Schedule repeat
                    if (next_call_delay.has_value()) {
                        AddJobInternal(next_call_delay.value(), std::move(job), true);
                    }
                }
                catch (const std::exception& ex) {
                    ReportExceptionAndContinue(ex);

                    // Exception handling
                    {
                        std::unique_lock locker(_dataLocker);

                        if (_exceptionHandler) {
                            try {
                                if (_exceptionHandler(ex)) {
                                    _jobs.clear();
                                }
                            }
                            catch (const std::exception& ex2) {
                                ReportExceptionAndContinue(ex2);
                            }
                        }
                    }

                    // Todo: schedule job repeat with last duration?
                    try {
                        // AddJobInternal(next_call_duration.value(), std::move(job), true);
                    }
                    catch (const std::exception& ex2) {
                        ReportExceptionAndContinue(ex2);
                    }
                }
                catch (...) {
                    UNKNOWN_EXCEPTION();
                }
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        UNKNOWN_EXCEPTION();
    }
}

static thread_local string ThreadName;

extern void SetThisThreadName(const string& name)
{
    STACK_TRACE_ENTRY();

    ThreadName = name;

    Platform::SetThreadName(ThreadName);

#if FO_TRACY
    tracy::SetThreadName(ThreadName.c_str());
#endif
}

extern auto GetThisThreadName() -> const string&
{
    STACK_TRACE_ENTRY();

    if (ThreadName.empty()) {
        static size_t thread_counter = 0;
        thread_local size_t thread_num = ++thread_counter;

        ThreadName = strex("{}", thread_num);
    }

    return ThreadName;
}

// Dummy symbols for web build to avoid linker errors
#if FO_WEB
void* SDL_LoadObject(const char* sofile)
{
    STACK_TRACE_ENTRY();

    UNREACHABLE_PLACE();
}

void* SDL_LoadFunction(void* handle, const char* name)
{
    STACK_TRACE_ENTRY();

    UNREACHABLE_PLACE();
}

void SDL_UnloadObject(void* handle)
{
    STACK_TRACE_ENTRY();

    UNREACHABLE_PLACE();
}

void emscripten_sleep(unsigned int ms)
{
    STACK_TRACE_ENTRY();

    UNREACHABLE_PLACE();
}
#endif

// Replace memory allocator
#if FO_HAVE_RPMALLOC

#if FO_TRACY
#include "client/tracy_rpmalloc.hpp"
#else
#include "rpmalloc.h"
#endif

#include <new>

#if FO_WINDOWS
#define CRTDECL __CRTDECL
#else
#define CRTDECL
#endif

extern void CRTDECL operator delete(void* p) noexcept
{
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p) noexcept
{
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size) noexcept(false)
{
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size) noexcept(false)
{
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, const std::nothrow_t& tag) noexcept
{
    UNUSED_VARIABLE(tag);
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, const std::nothrow_t& tag) noexcept
{
    UNUSED_VARIABLE(tag);
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void CRTDECL operator delete(void* p, std::size_t size) noexcept
{
    UNUSED_VARIABLE(size);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t size) noexcept
{
    UNUSED_VARIABLE(size);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::align_val_t align) noexcept
{
    UNUSED_VARIABLE(align);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::align_val_t align) noexcept
{
    UNUSED_VARIABLE(align);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::size_t size, std::align_val_t align) noexcept
{
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(align);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t size, std::align_val_t align) noexcept
{
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(align);
#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align) noexcept(false)
{
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align) noexcept(false)
{
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& tag) noexcept
{
    UNUSED_VARIABLE(tag);
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& tag) noexcept
{
    UNUSED_VARIABLE(tag);
#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

#undef CRTDECL

#endif

extern auto MemMalloc(size_t size) -> void*
{
    NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    return tracy::rpmalloc(size);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpmalloc(size);
#else
    return malloc(size);
#endif
}

extern auto MemCalloc(size_t num, size_t size) -> void*
{
    NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    return tracy::rpcalloc(num, size);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpcalloc(num, size);
#else
    return calloc(num, size);
#endif
}

extern auto MemRealloc(void* ptr, size_t size) -> void*
{
    NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    return tracy::rprealloc(ptr, size);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rprealloc(ptr, size);
#else
    return realloc(ptr, size);
#endif
}

extern void MemFree(void* ptr)
{
    NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::rpfree(ptr);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    rpfree(ptr);
#else
    free(ptr);
#endif
}

static constexpr size_t BACKUP_MEMORY_CHUNKS = 100;
static constexpr size_t BACKUP_MEMORY_CHUNK_SIZE = 100000; // 100 chunks x 100kb = 10mb
static unique_ptr<unique_ptr<uint8[]>[]> BackupMemoryChunks;
static std::atomic_size_t BackupMemoryChunksCount;

extern void InitBackupMemoryChunks()
{
    NO_STACK_TRACE_ENTRY();

    BackupMemoryChunks = std::make_unique<unique_ptr<uint8[]>[]>(BACKUP_MEMORY_CHUNKS);

    for (size_t i = 0; i < BACKUP_MEMORY_CHUNKS; i++) {
        BackupMemoryChunks[i] = std::make_unique<uint8[]>(BACKUP_MEMORY_CHUNK_SIZE);
    }

    BackupMemoryChunksCount.store(BACKUP_MEMORY_CHUNKS);
}

extern auto FreeBackupMemoryChunk() noexcept -> bool
{
    NO_STACK_TRACE_ENTRY();

    while (true) {
        size_t cur_size = BackupMemoryChunksCount.load();

        if (cur_size == 0) {
            return false;
        }

        if (BackupMemoryChunksCount.compare_exchange_strong(cur_size, cur_size - 1)) {
            BackupMemoryChunks[cur_size].reset();
            return true;
        }
    }
}

extern void ReportBadAlloc(string_view message, string_view type_str, size_t count, size_t size) noexcept
{
    NO_STACK_TRACE_ENTRY();

    char itoa_buf[64] = {};

    WriteLogFatalMessage("\nBAD ALLOC!\n\n");
    WriteLogFatalMessage(message);
    WriteLogFatalMessage("\n");
    WriteLogFatalMessage("Type: ");
    WriteLogFatalMessage(type_str);
    WriteLogFatalMessage("\n");
    WriteLogFatalMessage("Count: ");
    WriteLogFatalMessage(ItoA(count, itoa_buf, 10));
    WriteLogFatalMessage("\n");
    WriteLogFatalMessage("Size: ");
    WriteLogFatalMessage(ItoA(size, itoa_buf, 10));
    WriteLogFatalMessage("\n\n");
    SafeWriteStackTrace(&StackTrace);

    if (App != nullptr) {
        App->RequestQuit();
    }
}

extern void ReportAndExit(string_view message) noexcept
{
    NO_STACK_TRACE_ENTRY();

    WriteLogFatalMessage(message);
    ExitApp(false);
}
