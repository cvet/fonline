//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "catch_amalgamated.hpp"

#include "ContentUpdateTransport.h"

FO_BEGIN_NAMESPACE

struct TestContentUpdateDownloadState
{
    uint32_t ProcessCount {};
    uint32_t CancelCount {};
    uint32_t DestroyCount {};
    ContentUpdateTransportStatus Status {ContentUpdateTransportStatus::InProgress};
    uint64_t DownloadedBytes {17};
    string Error {};
};

class TestContentUpdateDownload final : public ContentUpdateTransportDownload
{
public:
    explicit TestContentUpdateDownload(shared_ptr<TestContentUpdateDownloadState> state) :
        _state {std::move(state)}
    {
        FO_STACK_TRACE_ENTRY();
    }

    ~TestContentUpdateDownload() override
    {
        FO_NO_STACK_TRACE_ENTRY();

        ++_state->DestroyCount;
    }

    void Process() override
    {
        FO_STACK_TRACE_ENTRY();

        ++_state->ProcessCount;
    }

    void Cancel() noexcept override
    {
        FO_NO_STACK_TRACE_ENTRY();

        ++_state->CancelCount;
        _state->Status = ContentUpdateTransportStatus::Failed;
        _state->Error = "cancelled";
    }

    [[nodiscard]] auto GetStatus() const noexcept -> ContentUpdateTransportStatus override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _state->Status;
    }

    [[nodiscard]] auto GetDownloadedBytes() const noexcept -> uint64_t override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _state->DownloadedBytes;
    }

    [[nodiscard]] auto GetError() const noexcept -> string_view override
    {
        FO_NO_STACK_TRACE_ENTRY();

        return _state->Error;
    }

private:
    shared_ptr<TestContentUpdateDownloadState> _state;
};

TEST_CASE("ContentUpdateTransportRegistry")
{
    ContentUpdateSource source {};
    source.Provider = "test";
    source.SourceKey = "mirror";
    source.Transport = "https";
    source.Locator = "https://example.invalid/file";

    ContentUpdateFileInfo file {};
    file.Size = 42;

    const ContentUpdateTransportRequest request {source, file, "candidate.part"};

    SECTION("owns source and file metadata")
    {
        source.Provider = "changed";
        source.Locator = "https://changed.invalid/file";
        file.Size = 84;

        CHECK(request.Source.Provider == "test");
        CHECK(request.Source.Locator == "https://example.invalid/file");
        CHECK(request.File.Size == 42);
        CHECK(request.CandidatePath == "candidate.part");
    }

    SECTION("creates downloads and exposes their lifecycle")
    {
        ContentUpdateTransportRegistry registry;
        auto state = SafeAlloc::MakeShared<TestContentUpdateDownloadState>();
        registry.Register("https", [state](const ContentUpdateTransportRequest& actual_request) -> unique_nptr<ContentUpdateTransportDownload> {
            REQUIRE(actual_request.Source.Provider == "test");
            REQUIRE(actual_request.Source.SourceKey == "mirror");
            REQUIRE(actual_request.Source.Transport == "https");
            REQUIRE(actual_request.Source.Locator == "https://example.invalid/file");
            REQUIRE(actual_request.File.Size == 42);
            REQUIRE(actual_request.CandidatePath == "candidate.part");

            unique_ptr<ContentUpdateTransportDownload> download = SafeAlloc::MakeUnique<TestContentUpdateDownload>(state);
            return unique_nptr<ContentUpdateTransportDownload> {std::move(download)};
        });

        CHECK(registry.IsRegistered("https"));
        CHECK_FALSE(registry.IsRegistered("HTTPS"));

        auto download = registry.Create("https", request);
        REQUIRE(download);
        CHECK(download->GetStatus() == ContentUpdateTransportStatus::InProgress);
        CHECK(download->GetDownloadedBytes() == 17);
        CHECK(download->GetError().empty());

        download->Process();
        CHECK(state->ProcessCount == 1);

        state->DownloadedBytes = 42;
        state->Status = ContentUpdateTransportStatus::Succeeded;
        CHECK(download->GetDownloadedBytes() == 42);
        CHECK(download->GetStatus() == ContentUpdateTransportStatus::Succeeded);

        download->Cancel();
        CHECK(state->CancelCount == 1);
        CHECK(download->GetStatus() == ContentUpdateTransportStatus::Failed);
        CHECK(download->GetError() == "cancelled");
        CHECK(state->DestroyCount == 0);

        download.reset();
        CHECK(state->DestroyCount == 1);
    }

    SECTION("declining a request is distinct from an unknown transport")
    {
        ContentUpdateTransportRegistry registry;
        uint32_t decline_count = 0;
        registry.Register("decline", [&decline_count](const ContentUpdateTransportRequest&) -> unique_nptr<ContentUpdateTransportDownload> {
            ++decline_count;
            return nullptr;
        });

        CHECK(registry.IsRegistered("decline"));
        CHECK_FALSE(registry.Create("decline", request));
        CHECK(decline_count == 1);
        CHECK_FALSE(registry.IsRegistered("missing"));
        CHECK_FALSE(registry.Create("missing", request));
        CHECK(decline_count == 1);
    }

    SECTION("factory exceptions propagate without mutating the registry")
    {
        ContentUpdateTransportRegistry registry;
        uint32_t attempt_count = 0;
        registry.Register("throwing", [&attempt_count](const ContentUpdateTransportRequest&) -> unique_nptr<ContentUpdateTransportDownload> {
            ++attempt_count;
            throw ContentUpdaterException("Test transport factory failure");
        });

        CHECK_THROWS_AS(registry.Create("throwing", request), ContentUpdaterException);
        CHECK_THROWS_AS(registry.Create("throwing", request), ContentUpdaterException);
        CHECK(attempt_count == 2);
        CHECK(registry.IsRegistered("throwing"));
    }

    SECTION("registration validates factories and is isolated per registry")
    {
        ContentUpdateTransportRegistry first_registry;
        ContentUpdateTransportRegistry second_registry;
        const auto decline = [](const ContentUpdateTransportRequest&) -> unique_nptr<ContentUpdateTransportDownload> { return nullptr; };

        first_registry.Register("https", decline);
        second_registry.Register("https", decline);
        CHECK(first_registry.IsRegistered("https"));
        CHECK(second_registry.IsRegistered("https"));

        CHECK_THROWS(first_registry.Register("https", decline));
        CHECK(second_registry.IsRegistered("https"));
        CHECK_THROWS(first_registry.Register("", decline));

        ContentUpdateTransportRegistry::Factory empty_factory;
        CHECK_THROWS(first_registry.Register("empty", std::move(empty_factory)));
        CHECK_FALSE(first_registry.IsRegistered("empty"));
    }
}

FO_END_NAMESPACE
