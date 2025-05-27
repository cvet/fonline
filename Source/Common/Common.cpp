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
#include "Version-Include.h"

#if FO_MAC
#include <sys/sysctl.h>
#endif

#if FO_WINDOWS || FO_LINUX || FO_MAC
#if !FO_WINDOWS
#if __has_include(<libunwind.h>) && !(FO_MAC && defined(__aarch64__))
#define BACKWARD_HAS_LIBUNWIND 1
#elif __has_include(<bfd.h>)
#define BACKWARD_HAS_BFD 1
#endif
#endif
#include "backward.hpp"
#endif

#include "WinApiUndef-Include.h"

FO_BEGIN_NAMESPACE();

static bool ExceptionMessageBox = false;

const timespan timespan::zero;
const nanotime nanotime::zero;
const synctime synctime::zero;

// ReSharper disable once CppInconsistentNaming
const ucolor ucolor::clear;

hstring::entry hstring::_zeroEntry;

map<uint16, std::function<InterthreadDataCallback(InterthreadDataCallback)>> InterthreadListeners;

GlobalDataCallback CreateGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
GlobalDataCallback DeleteGlobalDataCallbacks[MAX_GLOBAL_DATA_CALLBACKS];
int32 GlobalDataCallbacksCount;

static constexpr size_t STACK_TRACE_MAX_SIZE = 128;

struct StackTraceData
{
    size_t CallsCount = {};
    array<const SourceLocationData*, STACK_TRACE_MAX_SIZE> CallTree = {};
};

static thread_local StackTraceData StackTrace;

static StackTraceData* CrashStackTrace;

extern void InstallSystemExceptionHandler()
{
    FO_STACK_TRACE_ENTRY();

#if FO_WINDOWS || FO_LINUX || FO_MAC
    if (!IsRunInDebugger()) {
        [[maybe_unused]] static backward::SignalHandling sh;
        assert(sh.loaded());
    }
#endif
}

FO_END_NAMESPACE();
extern void SetCrashStackTrace() noexcept // Called in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_NAMESPACE CrashStackTrace = &FO_NAMESPACE StackTrace;
}
FO_BEGIN_NAMESPACE();

template<typename T>
static char* ItoA(T num, char buf[64], int32 base) noexcept
{
    static_assert(std::is_integral_v<T>);
    static_assert(sizeof(T) <= 16);

    int32 i = 0;
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
        const int32 rem = num % base;
        buf[i++] = static_cast<char>(rem > 9 ? rem - 10 + 'a' : rem + '0');
        num = num / base;
    }

    if (is_negative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    int32 start = 0;
    int32 end = i - 1;

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
    FO_NO_STACK_TRACE_ENTRY();

    WriteLogFatalMessage("Stack trace (most recent call first):\n");

    char itoa_buf[64] = {};

    for (int32 i = std::min(static_cast<int32>(st->CallsCount), static_cast<int32>(STACK_TRACE_MAX_SIZE)) - 1; i >= 0; i--) {
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

static BackwardOStreamBuffer CrashStreamBuf;
static auto CrashStream = std::ostream(&CrashStreamBuf); // Passed to Printer::print in backward.hpp

FO_END_NAMESPACE();
extern auto GetCrashStream() noexcept -> std::ostream& // Passed to Printer::print in backward.hpp
{
    FO_NO_STACK_TRACE_ENTRY();

    return FO_NAMESPACE CrashStream;
}
FO_BEGIN_NAMESPACE();

extern void CreateGlobalData()
{
    FO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        CreateGlobalDataCallbacks[i]();
    }

    InitBackupMemoryChunks();
}

extern void DeleteGlobalData()
{
    FO_STACK_TRACE_ENTRY();

    for (auto i = 0; i < GlobalDataCallbacksCount; i++) {
        DeleteGlobalDataCallbacks[i]();
    }
}

static auto InsertCatchedMark(const string& st) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

    ExceptionMessageBox = enabled;
}

extern void ReportStrongAssertAndExit(string_view message, const char* file, int32 line) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        throw StrongAssertationException(message, file, line);
    }
    catch (const StrongAssertationException& ex) {
        ReportExceptionAndExit(ex);
    }
}

extern void ReportVerifyFailed(string_view message, const char* file, int32 line) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    try {
        throw VerifyFailedException(message, file, line);
    }
    catch (const VerifyFailedException& ex) {
        ReportExceptionAndContinue(ex);
    }
}

extern void PushStackTrace(const SourceLocationData& loc) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    auto& st = StackTrace;

    if (st.CallsCount > 0) {
        st.CallsCount--;
    }
#endif
}

extern auto GetStackTrace() -> string
{
    FO_NO_STACK_TRACE_ENTRY();

#if !FO_NO_MANUAL_STACK_TRACE
    std::ostringstream ss;

    ss << "Stack trace (most recent call first):\n";

    const auto& st = StackTrace;

    for (int32 i = std::min(static_cast<int32>(st.CallsCount), static_cast<int32>(STACK_TRACE_MAX_SIZE)) - 1; i >= 0; i--) {
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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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
        int32 mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
        struct kinfo_proc info = {};
        size_t size = sizeof(info);
        if (::sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) == 0) {
            RunInDebugger = (info.kp_proc.p_flag & P_TRACED) != 0;
        }
    });

#else
    ignore_unused(RunInDebuggerOnce);
#endif

    return RunInDebugger;
}

extern auto BreakIntoDebugger([[maybe_unused]] string_view error_message) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    const auto traceback = GetStackTrace();
    const auto time = nanotime::now().desc(true);
    const string fname = strex("{}_{}_{:04}.{:02}.{:02}_{:02}-{:02}-{:02}.txt", FO_DEV_NAME, appendix, time.year, time.month, time.day, time.hour, time.minute, time.second);

    if (auto file = DiskFileSystem::OpenFile(fname, true)) {
        file.Write(strex("{}\n", appendix));
        file.Write(strex("{}\n", message));
        file.Write(strex("\n"));
        file.Write(strex("Application\n"));
        file.Write(strex("\tName        {}\n", FO_NICE_NAME));
        file.Write(strex("\tOS          Windows\n"));
        file.Write(strex("\tTimestamp   {:04}.{:02}.{:02} {:02}:{:02}:{:02}\n", time.year, time.month, time.day, time.hour, time.minute, time.second));
        file.Write(strex("\n"));
        file.Write(traceback);
        file.Write(strex("\n"));
    }
}

struct LocalTimeData
{
    LocalTimeData()
    {
        FO_STACK_TRACE_ENTRY();

        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm ltm = *std::localtime(&t);
        const std::time_t lt = std::mktime(&ltm);
        std::tm gtm = *std::gmtime(&lt);
        const std::time_t gt = std::mktime(&gtm);
        const int64 offset = lt - gt;
        Offset = std::chrono::seconds(offset);
    }

    std::chrono::system_clock::duration Offset {};
};
FO_GLOBAL_DATA(LocalTimeData, LocalTime);

auto make_time_desc(timespan time_offset, bool local) -> time_desc_t
{
    FO_STACK_TRACE_ENTRY();

    time_desc_t result;

    const auto now_sys = std::chrono::system_clock::now() + (local ? LocalTime->Offset : std::chrono::seconds(0));
    const auto time_sys = now_sys + std::chrono::duration_cast<std::chrono::system_clock::duration>(time_offset.value());

    const auto ymd_days = std::chrono::floor<std::chrono::days>(time_sys);
    const auto ymd = std::chrono::year_month_day(std::chrono::sys_days(ymd_days.time_since_epoch()));

    if (!ymd.ok()) {
        throw GenericException("Invalid year/month/day");
    }

    auto rest_day = time_sys - ymd_days;

    const auto hours = std::chrono::duration_cast<std::chrono::hours>(rest_day);
    rest_day -= hours;
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(rest_day);
    rest_day -= minutes;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(rest_day);
    rest_day -= seconds;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(rest_day);
    rest_day -= milliseconds;
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(rest_day);
    rest_day -= microseconds;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(rest_day);

    result.year = static_cast<int32>(ymd.year());
    result.month = static_cast<int32>(static_cast<uint32>(ymd.month()));
    result.day = static_cast<int32>(static_cast<uint32>(ymd.day()));
    result.hour = static_cast<int32>(hours.count());
    result.minute = static_cast<int32>(minutes.count());
    result.second = static_cast<int32>(seconds.count());
    result.millisecond = static_cast<int32>(milliseconds.count());
    result.microsecond = static_cast<int32>(microseconds.count());
    result.nanosecond = static_cast<int32>(nanoseconds.count());

    return result;
}

auto make_time_offset(int32 year, int32 month, int32 day, int32 hour, int32 minute, int32 second, int32 millisecond, int32 microsecond, int32 nanosecond, bool local) -> timespan
{
    FO_STACK_TRACE_ENTRY();

    const auto ymd = std::chrono::year_month_day {std::chrono::year {year}, std::chrono::month {numeric_cast<uint32>(month)}, std::chrono::day {numeric_cast<uint32>(day)}};

    if (!ymd.ok()) {
        throw GenericException("Invalid year/month/day");
    }

    const auto days_sys = std::chrono::sys_days {ymd};
    const auto time_of_day = std::chrono::hours {hour} + std::chrono::minutes {minute} + std::chrono::seconds {second} + std::chrono::milliseconds {millisecond} + std::chrono::microseconds {microsecond} + std::chrono::nanoseconds {nanosecond};
    const auto target_sys = std::chrono::sys_time<std::chrono::nanoseconds> {days_sys + time_of_day};
    const auto now_sys = std::chrono::system_clock::now() + (local ? LocalTime->Offset : std::chrono::seconds(0));
    const auto delta = target_sys - now_sys;

    return std::chrono::duration_cast<steady_time_point::duration>(delta);
}

FrameBalancer::FrameBalancer(bool enabled, int32 sleep, int32 fixed_fps) :
    _enabled {enabled && (sleep >= 0 || fixed_fps > 0)},
    _sleep {sleep},
    _fixedFps {fixed_fps}
{
    FO_STACK_TRACE_ENTRY();
}

void FrameBalancer::StartLoop()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopStart = nanotime::now();
}

void FrameBalancer::EndLoop()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }

    _loopDuration = nanotime::now() - _loopStart;

    if (_sleep >= 0) {
        if (_sleep == 0) {
            std::this_thread::yield();
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(_sleep));
        }
    }
    else if (_fixedFps > 0) {
        const timespan target_time = std::chrono::nanoseconds {static_cast<uint64>(1000.0 / static_cast<double>(_fixedFps) * 1000000.0)};
        const auto idle_time = target_time - _loopDuration + _idleTimeBalance;

        if (idle_time > timespan::zero) {
            const auto sleep_start = nanotime::now();

            std::this_thread::sleep_for(idle_time.value());

            const auto sleep_duration = nanotime::now() - sleep_start;

            _idleTimeBalance += (target_time - _loopDuration) - sleep_duration;
        }
        else {
            _idleTimeBalance += target_time - _loopDuration;

            if (_idleTimeBalance < timespan(-std::chrono::milliseconds {1000})) {
                _idleTimeBalance = timespan(-std::chrono::milliseconds {1000});
            }
        }
    }
}

WorkThread::WorkThread(string_view name)
{
    FO_STACK_TRACE_ENTRY();

    _name = name;
    _thread = std::thread(&WorkThread::ThreadEntry, this);
}

WorkThread::~WorkThread()
{
    FO_STACK_TRACE_ENTRY();

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
        FO_UNKNOWN_EXCEPTION();
    }
}

auto WorkThread::GetJobsCount() const -> size_t
{
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    return _jobs.size() + (_jobActive ? 1 : 0);
}

void WorkThread::SetExceptionHandler(ExceptionHandler handler)
{
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    _exceptionHandler = std::move(handler);
}

void WorkThread::AddJob(Job job)
{
    FO_STACK_TRACE_ENTRY();

    AddJobInternal(std::chrono::milliseconds {0}, std::move(job), false);
}

void WorkThread::AddJob(timespan delay, Job job)
{
    FO_STACK_TRACE_ENTRY();

    AddJobInternal(delay, std::move(job), false);
}

void WorkThread::AddJobInternal(timespan delay, Job job, bool no_notify)
{
    FO_STACK_TRACE_ENTRY();

    {
        std::unique_lock locker(_dataLocker);

        const auto fire_time = nanotime::now() + delay;

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
    FO_STACK_TRACE_ENTRY();

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
    FO_STACK_TRACE_ENTRY();

    std::unique_lock locker(_dataLocker);

    while (!_jobs.empty() || _jobActive) {
        _doneSignal.wait(locker);
    }
}

void WorkThread::ThreadEntry() noexcept
{
    FO_STACK_TRACE_ENTRY();

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
                    const auto cur_time = nanotime::now();

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
                    FO_UNKNOWN_EXCEPTION();
                }
            }
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
    catch (...) {
        FO_UNKNOWN_EXCEPTION();
    }
}

static thread_local string ThreadName;

extern void SetThisThreadName(const string& name)
{
    FO_STACK_TRACE_ENTRY();

    ThreadName = name;

    Platform::SetThreadName(ThreadName);

#if FO_TRACY
    tracy::SetThreadName(ThreadName.c_str());
#endif
}

extern auto GetThisThreadName() -> const string&
{
    FO_STACK_TRACE_ENTRY();

    if (ThreadName.empty()) {
        static size_t thread_counter = 0;
        thread_local size_t thread_num = ++thread_counter;

        ThreadName = strex("{}", thread_num);
    }

    return ThreadName;
}

// Dummy symbols for web build to avoid linker errors
#if FO_WEB

FO_END_NAMESPACE();

void emscripten_sleep(unsigned int32 ms)
{
    FO_STACK_TRACE_ENTRY();

    FO_UNREACHABLE_PLACE();
}

FO_BEGIN_NAMESPACE();

#endif

// Replace memory allocator
#if FO_HAVE_RPMALLOC

FO_END_NAMESPACE();

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
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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

extern void* CRTDECL operator new(std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
#else
    auto* p = rpmalloc(size);
#endif
    return p;
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete(void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void CRTDECL operator delete[](void* p, std::size_t /*size*/, std::align_val_t /*align*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    TracyFree(p);
    tracy::rpfree(p);
#else
    rpfree(p);
#endif
}

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align) noexcept(false)
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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

extern void* CRTDECL operator new(std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_TRACY
    tracy::InitRpmalloc();
    auto* p = tracy::rpaligned_alloc(static_cast<size_t>(align), size);
    TracyAlloc(p, size);
#else
    auto* p = rpaligned_alloc(static_cast<size_t>(align), size);
#endif
    return p;
}

extern void* CRTDECL operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t& /*tag*/) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

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
FO_BEGIN_NAMESPACE();

#endif

extern auto MemMalloc(size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    void* p = tracy::rpmalloc(size);
    TracyAlloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpmalloc(size);
#else
    return malloc(size);
#endif
}

extern auto MemCalloc(size_t num, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    void* p = tracy::rpcalloc(num, size);
    TracyAlloc(p, num * size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rpcalloc(num, size);
#else
    return calloc(num, size);
#endif
}

extern auto MemRealloc(void* ptr, size_t size) noexcept -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    tracy::InitRpmalloc();
    TracyFree(ptr);
    void* p = tracy::rprealloc(ptr, size);
    TracyAlloc(p, size);
    return p;
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    return rprealloc(ptr, size);
#else
    return realloc(ptr, size);
#endif
}

extern void MemFree(void* ptr) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

#if FO_HAVE_RPMALLOC && FO_TRACY
    TracyFree(ptr);
    tracy::rpfree(ptr);
#elif FO_HAVE_RPMALLOC && !FO_TRACY
    rpfree(ptr);
#else
    free(ptr);
#endif
}

extern void MemCopy(void* dest, const void* src, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    // Standard: If either dest or src is an invalid or null pointer, the behavior is undefined, even if count is zero
    // So check size first
    if (size != 0) {
        std::memcpy(dest, src, size);
    }
}

extern void MemFill(void* ptr, int32 value, size_t size) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0) {
        std::memset(ptr, value, size);
    }
}

extern auto MemCompare(const void* ptr1, const void* ptr2, size_t size) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    if (size != 0) {
        return std::memcmp(ptr1, ptr2, size) == 0;
    }

    return true;
}

static constexpr size_t BACKUP_MEMORY_CHUNKS = 100;
static constexpr size_t BACKUP_MEMORY_CHUNK_SIZE = 100000; // 100 chunks x 100kb = 10mb
static unique_ptr<unique_ptr<uint8[]>[]> BackupMemoryChunks;
static std::atomic_size_t BackupMemoryChunksCount;

extern void InitBackupMemoryChunks()
{
    FO_NO_STACK_TRACE_ENTRY();

    BackupMemoryChunks = std::make_unique<unique_ptr<uint8[]>[]>(BACKUP_MEMORY_CHUNKS);

    for (size_t i = 0; i < BACKUP_MEMORY_CHUNKS; i++) {
        BackupMemoryChunks[i] = std::make_unique<uint8[]>(BACKUP_MEMORY_CHUNK_SIZE);
    }

    BackupMemoryChunksCount.store(BACKUP_MEMORY_CHUNKS);
}

extern auto FreeBackupMemoryChunk() noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

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
    FO_NO_STACK_TRACE_ENTRY();

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

    if (App) {
        App->RequestQuit();
    }
}

extern void ReportAndExit(string_view message) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    WriteLogFatalMessage(message);
    ExitApp(false);
}

FO_END_NAMESPACE();
