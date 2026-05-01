#include "catch_amalgamated.hpp"

#include "ExceptionHandling.h"

FO_BEGIN_NAMESPACE

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
        string traceback;
        bool fatal = false;

        SetExceptionCallback([&](string_view msg, string_view trace, bool is_fatal) {
            message = string(msg);
            traceback = string(trace);
            fatal = is_fatal;
        });

        const auto callback = GetExceptionCallback();
        REQUIRE(callback);
        callback("Msg", "Trace", true);

        CHECK(message == "Msg");
        CHECK(traceback == "Trace");
        CHECK(fatal);

        SetExceptionCallback({});
        CHECK_FALSE(GetExceptionCallback());

        SetExceptionCallback(std::move(prev_callback));
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
        string traceback;
        bool fatal = true;

        SetExceptionCallback([&](string_view msg, string_view trace, bool is_fatal) {
            message = string(msg);
            traceback = string(trace);
            fatal = is_fatal;
        });

        const GenericException ex {"Continue please"};
        ReportExceptionAndContinue(ex);

        CHECK(message == ex.what());
        CHECK_FALSE(traceback.empty());
        CHECK_FALSE(fatal);

        SetExceptionCallback(std::move(prev_callback));
    }

    SECTION("ReportVerifyFailedUsesVerifyFailedException")
    {
        const auto prev_callback = GetExceptionCallback();

        string message;
        string traceback;
        bool fatal = true;

        SetExceptionCallback([&](string_view msg, string_view trace, bool is_fatal) {
            message = string(msg);
            traceback = string(trace);
            fatal = is_fatal;
        });

        ReportVerifyFailed("CheckInput", "unit_test.cpp", 77);

        CHECK(message.find("VerifyFailedException: CheckInput") != string::npos);
        CHECK(message.find("- unit_test.cpp") != string::npos);
        CHECK(message.find("- 77") != string::npos);
        CHECK_FALSE(traceback.empty());
        CHECK_FALSE(fatal);

        SetExceptionCallback(std::move(prev_callback));
    }
}

FO_END_NAMESPACE
