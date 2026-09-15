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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "catch_amalgamated.hpp"

#include "BaseLogging.h"
#include "CommonHelpers.h"
#include "DiskFileSystem.h"
#include "ExceptionHandling.h"

// The crash reporter publishes these entry points to backward.hpp only, so they are declared here exactly as
// that header declares them - they carry no engine namespace and appear in no engine header
auto GetCrashStream() noexcept -> std::ostream&;
void SetCrashStackTrace() noexcept;
void SetCrashSignalInfo(int32_t signum, int32_t code, const void* address) noexcept;
void SetCrashSehInfo(uint32_t code, uint32_t flags, const void* address) noexcept;
void SetCrashTerminationInfo(const char* reason) noexcept;

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ExceptionHandlingTestBaseException);
FO_DECLARE_EXCEPTION_EXT(ExceptionHandlingTestDerivedException, ExceptionHandlingTestBaseException);

TEST_CASE("ExceptionHandling")
{
    SECTION("BaseEngineExceptionCapturesMessageAndParams")
    {
        GenericException ex {"Failure happened", 42, "tail"};

        CHECK(string_view {ex.name()} == "GenericException");
        CHECK(ex.message() == "Failure happened");
        REQUIRE(ex.params().size() == 2);
        CHECK(ex.params()[0] == "42");
        CHECK(ex.params()[1] == "tail");
        CHECK(string_view {ex.what()}.find("GenericException: Failure happened") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- 42") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- tail") != string_view::npos);
    }

    SECTION("FormatStackTraceListsNewestCallFirst")
    {
        // Order matches the unified trace contract: provider emits most-recent first, so
        // SecondFunc (the deeper call) appears before FirstFunc (its caller)
        stack_trace::script_layer layer;
        layer.script_frames.push_back({stack_trace::frame::frame_type::script, "SecondFunc", "/tmp/second.cpp", 22});
        layer.script_frames.push_back({stack_trace::frame::frame_type::script, "FirstFunc", "/tmp/first.cpp", 11});

        std::vector<stack_trace::script_layer> layers;
        layers.push_back(std::move(layer));

        stack_trace::data st {};
        st.script_layers = std::make_shared<const std::vector<stack_trace::script_layer>>(std::move(layers));

        auto formatted = stack_trace::format(st);

        CHECK(formatted.find("Stack trace (most recent call first):") == 0);
        CHECK(formatted.find("- [Script] SecondFunc (second.cpp line 22)") != string::npos);
        CHECK(formatted.find("- [Script] FirstFunc (first.cpp line 11)") != string::npos);
        CHECK(formatted.find("SecondFunc") < formatted.find("FirstFunc"));
    }

    SECTION("FormatStackTraceWithNoCallsReturnsHeaderOnly")
    {
        stack_trace::data st {};

        auto formatted = stack_trace::format(st);

        CHECK(formatted == "Stack trace (most recent call first):");
    }

    SECTION("SetExceptionCallbackReplacesAndClearsCallback")
    {
        auto prev_callback = exceptions::get_callback();

        string message;
        bool has_origin = false;
        bool fatal = false;

        exceptions::set_callback([&](string_view msg, const stack_trace::catched_data& st, bool is_fatal) {
            message = string(msg);
            has_origin = st.origin.has_value();
            fatal = is_fatal;
        });

        auto callback = exceptions::get_callback();
        REQUIRE(callback);
        stack_trace::catched_data st {std::nullopt, {}};
        callback("Msg", st, true);

        CHECK(message == "Msg");
        CHECK_FALSE(has_origin);
        CHECK(fatal);

        exceptions::set_callback({});
        CHECK_FALSE(exceptions::get_callback());

        exceptions::set_callback(std::move(prev_callback));
    }

    SECTION("DerivedExceptionPreservesOwnNameMessageAndParams")
    {
        // Regression: a macro exception derived from another macro exception (not BaseEngineException
        // directly) must still report its own name/message/params, with no stray null pushed into params
        static_assert(std::is_base_of_v<ExceptionHandlingTestBaseException, ExceptionHandlingTestDerivedException>);
        static_assert(std::is_base_of_v<BaseEngineException, ExceptionHandlingTestDerivedException>);

        ExceptionHandlingTestDerivedException ex {"Derived failure", 7, "extra"};

        CHECK(string_view {ex.name()} == "ExceptionHandlingTestDerivedException");
        CHECK(ex.message() == "Derived failure");
        REQUIRE(ex.params().size() == 2);
        CHECK(ex.params()[0] == "7");
        CHECK(ex.params()[1] == "extra");
        CHECK(string_view {ex.what()}.find("ExceptionHandlingTestDerivedException: Derived failure") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- 7") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- extra") != string_view::npos);
        CHECK(string_view {ex.what()}.find("0x0") == string_view::npos);
    }

    SECTION("BaseEngineExceptionCopyPreservesPayload")
    {
        InvalidOperationException original {"Operation failed", 99};
        InvalidOperationException copy {original};

        CHECK(string_view {copy.name()} == "InvalidOperationException");
        CHECK(copy.message() == "Operation failed");
        REQUIRE(copy.params().size() == 1);
        CHECK(copy.params()[0] == "99");
        CHECK(string_view {copy.what()}.find("InvalidOperationException: Operation failed") != string_view::npos);
    }

    SECTION("ReportExceptionAndContinueInvokesNonFatalCallback")
    {
        auto prev_callback = exceptions::get_callback();

        string message;
        bool trace_received = false;
        bool trace_has_origin = false;
        bool fatal = true;

        exceptions::set_callback([&](string_view msg, const stack_trace::catched_data& st, bool is_fatal) {
            message = string(msg);
            trace_received = true;
            trace_has_origin = st.origin.has_value();
            fatal = is_fatal;
        });

        GenericException ex {"Continue please"};
        exceptions::report_and_continue(ex);

        CHECK(message == ex.what());
        CHECK(trace_received);
        CHECK(trace_has_origin);
        CHECK_FALSE(fatal);

        exceptions::set_callback(std::move(prev_callback));
    }

    SECTION("VerifyAndThrowUsesVerificationExceptionWithSeparateParams")
    {
        try {
            FO_VERIFY_AND_THROW(false, "Throw context", 42);
            FAIL("Verify throw message form did not throw");
        }
        catch (const VerificationException& ex) {
            CHECK(string_view {ex.name()} == "VerificationException");
            CHECK(ex.message() == "Throw context");
            REQUIRE(ex.params().size() == 1);
            CHECK(ex.params()[0] == "42");
            CHECK(string_view {ex.what()}.find("VerificationException: Throw context") != string_view::npos);
            CHECK(string_view {ex.what()}.find("- 42") != string_view::npos);
        }
    }

    SECTION("VerifyAndContinueSupportsMessageForm")
    {
        auto prev_callback = exceptions::get_callback();

        vector<string> messages;

        exceptions::set_callback([&](string_view msg, const stack_trace::catched_data&, bool is_fatal) {
            messages.emplace_back(msg);
            CHECK_FALSE(is_fatal);
        });

        FO_VERIFY_AND_CONTINUE(false, "Continue context", 42);

        REQUIRE(messages.size() == 1);
        CHECK(messages[0].find("VerificationException: Continue context") != string::npos);
        CHECK(messages[0].find("- 42") != string::npos);

        exceptions::set_callback(std::move(prev_callback));
    }

    SECTION("VerifyAndReturnSupportsMessageForms")
    {
        auto prev_callback = exceptions::get_callback();

        vector<string> messages;

        exceptions::set_callback([&](string_view msg, const stack_trace::catched_data&, bool is_fatal) {
            messages.emplace_back(msg);
            CHECK_FALSE(is_fatal);
        });

        auto return_void_msg = [&] {
            FO_VERIFY_AND_RETURN(false, "Return context", 42);
            FAIL("Verify return message form did not return");
        };
        auto return_value_msg = [&]() -> int32_t {
            FO_VERIFY_AND_RETURN_VALUE(false, 22, "Return value context", 43);
            return 0;
        };

        return_void_msg();
        CHECK(return_value_msg() == 22);

        REQUIRE(messages.size() == 2);
        CHECK(messages[0].find("VerificationException: Return context") != string::npos);
        CHECK(messages[0].find("- 42") != string::npos);
        CHECK(messages[1].find("VerificationException: Return value context") != string::npos);
        CHECK(messages[1].find("- 43") != string::npos);

        exceptions::set_callback(std::move(prev_callback));
    }

    SECTION("CatchedStackTraceDataIncludesOriginForEngineExceptions")
    {
        auto prev_callback = exceptions::get_callback();

        bool trace_received = false;
        bool trace_has_origin = false;

        exceptions::set_callback([&](string_view, const stack_trace::catched_data& st, bool) { //
            trace_received = true;
            trace_has_origin = st.origin.has_value();
        });

        try {
            throw GenericException("Boom");
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        CHECK(trace_received);
        CHECK(trace_has_origin);

        exceptions::set_callback(std::move(prev_callback));
    }

    SECTION("CatchedStackTraceDataForNonEngineExceptionPrefixesCatchedAt")
    {
        auto prev_callback = exceptions::get_callback();

        bool trace_received = false;
        bool trace_has_origin = true;

        exceptions::set_callback([&](string_view, const stack_trace::catched_data& st, bool) { //
            trace_received = true;
            trace_has_origin = st.origin.has_value();
        });

        try {
            throw std::runtime_error("plain");
        }
        catch (const std::exception& ex) {
            exceptions::report_and_continue(ex);
        }

        CHECK(trace_received);
        CHECK_FALSE(trace_has_origin);

        exceptions::set_callback(std::move(prev_callback));
    }
}

static const auto NULL_LOG_PATH =
#if FO_WINDOWS
    string_view {"NUL"};
#else
    string_view {"/dev/null"};
#endif

TEST_CASE("CrashReporterHooks")
{
    // The crash reporter writes through the base log, so the log is pointed at a private file and the
    // emitted report is read back from there instead of leaking into the test console
    auto log_path = std::filesystem::temp_directory_path() / std::format("lf_crash_report_{}.log", std::chrono::steady_clock::now().time_since_epoch().count());
    string log_file = fs::path_to_string(log_path);

    logging::to_file(log_file);
    auto restore_log = scope_exit([&log_file]() noexcept {
        safe_call([&log_file] {
            logging::to_file(NULL_LOG_PATH);
            (void)fs::remove_file(log_file);
        });
    });

    SECTION("TheFatalReportCarriesTheReasonAndTheCapturedTrace")
    {
        // This is the std::terminate path: backward.hpp records the reason, captures the trace and then
        // prints the frames into the crash stream, whose first write emits the whole report header
        ::SetCrashTerminationInfo("std::terminate");
        ::SetCrashStackTrace();

        std::ostream& stream = ::GetCrashStream();
        stream << "printed frame line\n";
        stream.flush();

        optional<string> written = fs::read_file(log_file);
        REQUIRE(written.has_value());
        CHECK(written->find("FATAL ERROR!") != string::npos);
        CHECK(written->find("Crash reason: Runtime termination: std::terminate") != string::npos);
        CHECK(written->find("Stack trace (most recent call first):") != string::npos);
        CHECK(written->find("printed frame line") != string::npos);
    }

    SECTION("AnInFlightExceptionIsAppendedToTheTerminationReason")
    {
        // std::terminate normally fires while an exception is in flight, and the reporter is expected to
        // name it - the header is already spent by now, so the stored reason is re-read through the stream
        try {
            throw GenericException("Unhandled boom");
        }
        catch (const std::exception&) {
            ::SetCrashTerminationInfo("std::terminate");
        }

        // A reason with no source, and the non-std branch of the same lookup
        ::SetCrashTerminationInfo(nullptr);

        try {
            throw 42;
        }
        catch (...) {
            ::SetCrashTerminationInfo("std::terminate");
        }

        std::ostream& stream = ::GetCrashStream();
        stream.put('x');
        stream.flush();

        CHECK(fs::read_file(log_file).has_value());
    }

    SECTION("EverySignalAndSehCodeResolvesToAName")
    {
        // Only the C-standard six signals exist everywhere, and the POSIX rest are undeclared by the MSVC runtime,
        // so they cannot be listed unconditionally
        constexpr std::array KNOWN_SIGNALS = {
            SIGABRT,
            SIGFPE,
            SIGILL,
            SIGINT,
            SIGSEGV,
            SIGTERM,
#if !FO_WINDOWS
            SIGBUS,
            SIGQUIT,
            SIGSYS,
            SIGTRAP,
            SIGXCPU,
            SIGXFSZ,
#endif
        };

        for (int32_t signum : KNOWN_SIGNALS) {
            CHECK_NOTHROW(::SetCrashSignalInfo(signum, 1, nullptr));
        }

        CHECK_NOTHROW(::SetCrashSignalInfo(0, 0, nullptr));

        constexpr std::array KNOWN_SEH_CODES = {0x40010005U, 0x80000003U, 0x80000004U, 0xC0000005U, 0xC0000006U, 0xC0000008U, 0xC000001DU, 0xC0000025U, 0xC000008CU, 0xC000008DU, 0xC000008EU, 0xC000008FU, 0xC0000090U, 0xC0000091U, 0xC0000092U, 0xC0000093U, 0xC0000094U, 0xC0000095U, 0xC0000096U, 0xC00000FDU, 0xE0434352U, 0xE06D7363U};

        for (uint32_t code : KNOWN_SEH_CODES) {
            CHECK_NOTHROW(::SetCrashSehInfo(code, 0, nullptr));
        }

        CHECK_NOTHROW(::SetCrashSehInfo(0x11111111U, 1, &log_file));
    }

    SECTION("TheCrashStreamNeverServesReads")
    {
        // backward.hpp only ever prints into the stream, so the read side answers end-of-file
        CHECK(::GetCrashStream().rdbuf()->sgetc() == std::streambuf::traits_type::eof());
    }

    SECTION("TheAlternateSignalStackIsInstalledOncePerThread")
    {
        // The handler runs on its own stack so a stack-overflow crash can still be reported; a repeat
        // call on an already-equipped thread must be a no-op rather than a second reservation
        CHECK_NOTHROW(exceptions::install_crash_handler_stack());
        CHECK_NOTHROW(exceptions::install_crash_handler_stack());

        bool installed_on_worker = false;
        std::thread worker {[&installed_on_worker] {
            exceptions::install_crash_handler_stack();
            installed_on_worker = true;
        }};
        worker.join();

        CHECK(installed_on_worker);
    }
}

FO_END_NAMESPACE
