#include "catch_amalgamated.hpp"

#include "ExceptionHandling.h"

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
        CHECK(ex.params()[0] == u8"42");
        CHECK(ex.params()[1] == u8"tail");
        CHECK(string_view {ex.what()}.find("GenericException: Failure happened") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- 42") != string_view::npos);
        CHECK(string_view {ex.what()}.find("- tail") != string_view::npos);
    }

    SECTION("BaseEngineExceptionAcceptsStrictTextParamsDirectly")
    {
        u8string utf8_value {u8"путь/🌍"};
        string ascii_value {"Server"};
        GenericException ex {"Strict text context", utf8_value, utf8_value.view(), ascii_value, string_view {ascii_value}};

        REQUIRE(ex.params().size() == 4);
        CHECK(ex.params()[0] == utf8_value);
        CHECK(ex.params()[1] == utf8_value);
        CHECK(ex.params()[2] == u8"Server");
        CHECK(ex.params()[3] == u8"Server");
        CHECK(string_view {ex.what()}.find(utf8_as_char_view(utf8_value.view())) != string_view::npos);
        u8string strict_message = exception_message_utf8(ex);
        CHECK(strict_message.view().native_view().find(utf8_value.view().native_view()) != std::u8string_view::npos);
    }

    SECTION("ExceptionMessageUtf8NormalizesInvalidNativeText")
    {
        std::string invalid_message {"bad native text "};
        invalid_message.push_back(static_cast<char>(0xFF));
        std::runtime_error ex {invalid_message};

        CHECK(exception_message_utf8(ex) == u8string {u8"Native exception message is not valid UTF-8"});
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

        auto formatted = FormatStackTrace(st);

        CHECK(formatted.find("Stack trace (most recent call first):") == 0);
        CHECK(formatted.find("- [Script] SecondFunc (second.cpp line 22)") != string::npos);
        CHECK(formatted.find("- [Script] FirstFunc (first.cpp line 11)") != string::npos);
        CHECK(formatted.find("SecondFunc") < formatted.find("FirstFunc"));
    }

    SECTION("FormatStackTraceWithNoCallsReturnsHeaderOnly")
    {
        StackTraceData st {};

        auto formatted = FormatStackTrace(st);

        CHECK(formatted == "Stack trace (most recent call first):");
    }

    SECTION("SetExceptionCallbackReplacesAndClearsCallback")
    {
        auto prev_callback = GetExceptionCallback();

        u8string message;
        bool has_origin = false;
        bool fatal = false;

        SetExceptionCallback([&](u8string_view msg, const CatchedStackTraceData& st, bool is_fatal) {
            message.assign(msg);
            has_origin = st.Origin.has_value();
            fatal = is_fatal;
        });

        auto callback = GetExceptionCallback();
        REQUIRE(callback);
        CatchedStackTraceData st {std::nullopt, {}};
        callback(u8"Msg", st, true);

        CHECK(message == u8"Msg");
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

        ExceptionHandlingTestDerivedException ex {"Derived failure", 7, "extra"};

        CHECK(string_view {ex.name()} == "ExceptionHandlingTestDerivedException");
        CHECK(ex.message() == "Derived failure");
        REQUIRE(ex.params().size() == 2);
        CHECK(ex.params()[0] == u8"7");
        CHECK(ex.params()[1] == u8"extra");
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
        CHECK(copy.params()[0] == u8"99");
        CHECK(string_view {copy.what()}.find("InvalidOperationException: Operation failed") != string_view::npos);
    }

    SECTION("ReportExceptionAndContinueInvokesNonFatalCallback")
    {
        auto prev_callback = GetExceptionCallback();

        u8string message;
        bool trace_received = false;
        bool trace_has_origin = false;
        bool fatal = true;

        SetExceptionCallback([&](u8string_view msg, const CatchedStackTraceData& st, bool is_fatal) {
            message.assign(msg);
            trace_received = true;
            trace_has_origin = st.Origin.has_value();
            fatal = is_fatal;
        });

        GenericException ex {"Continue please"};
        ReportExceptionAndContinue(ex);

        CHECK(message == exception_message_utf8(ex));
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
            CHECK(ex.params()[0] == u8"42");
            CHECK(string_view {ex.what()}.find("VerificationException: Throw context") != string_view::npos);
            CHECK(string_view {ex.what()}.find("- 42") != string_view::npos);
        }
    }

    SECTION("VerifyAndContinueSupportsMessageForm")
    {
        auto prev_callback = GetExceptionCallback();

        vector<u8string> messages;

        SetExceptionCallback([&](u8string_view msg, const CatchedStackTraceData&, bool is_fatal) {
            messages.emplace_back(msg);
            CHECK_FALSE(is_fatal);
        });

        FO_VERIFY_AND_CONTINUE(false, "Continue context", 42);

        REQUIRE(messages.size() == 1);
        CHECK(messages[0].view().native_view().find(u8"VerificationException: Continue context") != std::u8string_view::npos);
        CHECK(messages[0].view().native_view().find(u8"- 42") != std::u8string_view::npos);

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("VerifyAndReturnSupportsMessageForms")
    {
        auto prev_callback = GetExceptionCallback();

        vector<u8string> messages;

        SetExceptionCallback([&](u8string_view msg, const CatchedStackTraceData&, bool is_fatal) {
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
        CHECK(messages[0].view().native_view().find(u8"VerificationException: Return context") != std::u8string_view::npos);
        CHECK(messages[0].view().native_view().find(u8"- 42") != std::u8string_view::npos);
        CHECK(messages[1].view().native_view().find(u8"VerificationException: Return value context") != std::u8string_view::npos);
        CHECK(messages[1].view().native_view().find(u8"- 43") != std::u8string_view::npos);

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("CatchedStackTraceDataIncludesOriginForEngineExceptions")
    {
        auto prev_callback = GetExceptionCallback();

        bool trace_received = false;
        bool trace_has_origin = false;

        SetExceptionCallback([&](u8string_view, const CatchedStackTraceData& st, bool) { //
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
        auto prev_callback = GetExceptionCallback();

        bool trace_received = false;
        bool trace_has_origin = true;

        SetExceptionCallback([&](u8string_view, const CatchedStackTraceData& st, bool) { //
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
