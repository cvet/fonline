#include "catch_amalgamated.hpp"

#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ExceptionHandlingTestBaseException);
FO_DECLARE_EXCEPTION_EXT(ExceptionHandlingTestDerivedException, ExceptionHandlingTestBaseException);

TEST_CASE("ExceptionHandling")
{
    SECTION("BaseEngineExceptionCapturesMessageAndParams")
    {
        const GenericException ex {"Failure happened", 42, "tail"};

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
        // SecondFunc (the deeper call) appears before FirstFunc (its caller).
        ScriptStackTraceLayer layer;
        layer.ScriptFrames.push_back({StackTraceFrame::FrameType::Script, "SecondFunc", "/tmp/second.cpp", 22});
        layer.ScriptFrames.push_back({StackTraceFrame::FrameType::Script, "FirstFunc", "/tmp/first.cpp", 11});

        std::vector<ScriptStackTraceLayer> layers;
        layers.push_back(std::move(layer));

        StackTraceData st {};
        st.ScriptLayers = std::make_shared<const std::vector<ScriptStackTraceLayer>>(std::move(layers));

        const auto formatted = FormatStackTrace(st);

        CHECK(formatted.find("Stack trace (most recent call first):") == 0);
        CHECK(formatted.find("- [Script] SecondFunc (second.cpp line 22)") != string::npos);
        CHECK(formatted.find("- [Script] FirstFunc (first.cpp line 11)") != string::npos);
        CHECK(formatted.find("SecondFunc") < formatted.find("FirstFunc"));
    }

    SECTION("FormatStackTraceWithNoCallsReturnsHeaderOnly")
    {
        const StackTraceData st {};

        const auto formatted = FormatStackTrace(st);

        CHECK(formatted == "Stack trace (most recent call first):");
    }

    SECTION("SetExceptionCallbackReplacesAndClearsCallback")
    {
        const auto prev_callback = GetExceptionCallback();

        string message;
        bool has_origin = false;
        bool fatal = false;

        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData& st, bool is_fatal) {
            message = string(msg);
            has_origin = st.Origin.has_value();
            fatal = is_fatal;
        });

        const auto callback = GetExceptionCallback();
        REQUIRE(callback);
        const CatchedStackTraceData st {std::nullopt, {}};
        callback("Msg", st, true);

        CHECK(message == "Msg");
        CHECK_FALSE(has_origin);
        CHECK(fatal);

        SetExceptionCallback({});
        CHECK_FALSE(GetExceptionCallback());

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("DerivedExceptionPreservesOwnNameMessageAndParams")
    {
        // Regression: a macro exception derived from another macro exception (not BaseEngineException
        // directly) must still report its own name/message/params, with no stray null pushed into params.
        static_assert(std::is_base_of_v<ExceptionHandlingTestBaseException, ExceptionHandlingTestDerivedException>);
        static_assert(std::is_base_of_v<BaseEngineException, ExceptionHandlingTestDerivedException>);

        const ExceptionHandlingTestDerivedException ex {"Derived failure", 7, "extra"};

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
        const InvalidOperationException original {"Operation failed", 99};
        const InvalidOperationException copy {original};

        CHECK(string_view {copy.name()} == "InvalidOperationException");
        CHECK(copy.message() == "Operation failed");
        REQUIRE(copy.params().size() == 1);
        CHECK(copy.params()[0] == "99");
        CHECK(string_view {copy.what()}.find("InvalidOperationException: Operation failed") != string_view::npos);
    }

    SECTION("ReportExceptionAndContinueInvokesNonFatalCallback")
    {
        const auto prev_callback = GetExceptionCallback();

        string message;
        bool trace_received = false;
        bool trace_has_origin = false;
        bool fatal = true;

        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData& st, bool is_fatal) {
            message = string(msg);
            trace_received = true;
            trace_has_origin = st.Origin.has_value();
            fatal = is_fatal;
        });

        const GenericException ex {"Continue please"};
        ReportExceptionAndContinue(ex);

        CHECK(message == ex.what());
        CHECK(trace_received);
        CHECK(trace_has_origin);
        CHECK_FALSE(fatal);

        SetExceptionCallback(std::move(prev_callback));
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
        const auto prev_callback = GetExceptionCallback();

        vector<string> messages;

        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool is_fatal) {
            messages.emplace_back(msg);
            CHECK_FALSE(is_fatal);
        });

        FO_VERIFY_AND_CONTINUE(false, "Continue context", 42);

        REQUIRE(messages.size() == 1);
        CHECK(messages[0].find("VerificationException: Continue context") != string::npos);
        CHECK(messages[0].find("- 42") != string::npos);

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("VerifyAndReturnSupportsMessageForms")
    {
        const auto prev_callback = GetExceptionCallback();

        vector<string> messages;

        SetExceptionCallback([&](string_view msg, const CatchedStackTraceData&, bool is_fatal) {
            messages.emplace_back(msg);
            CHECK_FALSE(is_fatal);
        });

        const auto return_void_msg = [&] {
            FO_VERIFY_AND_RETURN(false, "Return context", 42);
            FAIL("Verify return message form did not return");
        };
        const auto return_value_msg = [&]() -> int32_t {
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

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("CatchedStackTraceDataIncludesOriginForEngineExceptions")
    {
        const auto prev_callback = GetExceptionCallback();

        bool trace_received = false;
        bool trace_has_origin = false;

        SetExceptionCallback([&](string_view, const CatchedStackTraceData& st, bool) { //
            trace_received = true;
            trace_has_origin = st.Origin.has_value();
        });

        try {
            throw GenericException("Boom");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }

        CHECK(trace_received);
        CHECK(trace_has_origin);

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("CatchedStackTraceDataForNonEngineExceptionPrefixesCatchedAt")
    {
        const auto prev_callback = GetExceptionCallback();

        bool trace_received = false;
        bool trace_has_origin = true;

        SetExceptionCallback([&](string_view, const CatchedStackTraceData& st, bool) { //
            trace_received = true;
            trace_has_origin = st.Origin.has_value();
        });

        try {
            throw std::runtime_error("plain");
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }

        CHECK(trace_received);
        CHECK_FALSE(trace_has_origin);

        SetExceptionCallback(std::move(prev_callback));
    }
}

FO_END_NAMESPACE
