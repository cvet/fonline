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

#pragma once

#include "Common.h"

#include "ContentUpdater.h"

FO_BEGIN_NAMESPACE

enum class ContentUpdateTransportStatus : uint8_t
{
    InProgress = 0,
    Succeeded = 1,
    Failed = 2,
};

struct ContentUpdateTransportRequest
{
    ContentUpdateSource Source {};
    ContentUpdateFileInfo File {};
    string CandidatePath {};
};

class ContentUpdateTransportDownload
{
public:
    ContentUpdateTransportDownload() = default;
    ContentUpdateTransportDownload(const ContentUpdateTransportDownload&) = delete;
    ContentUpdateTransportDownload(ContentUpdateTransportDownload&&) noexcept = delete;
    auto operator=(const ContentUpdateTransportDownload&) -> ContentUpdateTransportDownload& = delete;
    auto operator=(ContentUpdateTransportDownload&&) noexcept -> ContentUpdateTransportDownload& = delete;
    virtual ~ContentUpdateTransportDownload() = default;

    virtual void Process() = 0;
    virtual void Cancel() noexcept = 0;
    [[nodiscard]] virtual auto GetStatus() const noexcept -> ContentUpdateTransportStatus = 0;
    [[nodiscard]] virtual auto GetDownloadedBytes() const noexcept -> uint64_t = 0;
    [[nodiscard]] virtual auto GetError() const noexcept -> string_view = 0;
};

class ContentUpdateTransportRegistry final
{
public:
    using Factory = function<unique_nptr<ContentUpdateTransportDownload>(const ContentUpdateTransportRequest&)>;

    ContentUpdateTransportRegistry() = default;
    ContentUpdateTransportRegistry(const ContentUpdateTransportRegistry&) = delete;
    ContentUpdateTransportRegistry(ContentUpdateTransportRegistry&&) noexcept = delete;
    auto operator=(const ContentUpdateTransportRegistry&) -> ContentUpdateTransportRegistry& = delete;
    auto operator=(ContentUpdateTransportRegistry&&) noexcept -> ContentUpdateTransportRegistry& = delete;
    ~ContentUpdateTransportRegistry() = default;

    void Register(string_view transport, Factory factory);
    [[nodiscard]] auto IsRegistered(string_view transport) const -> bool;
    [[nodiscard]] auto Create(string_view transport, const ContentUpdateTransportRequest& request) const -> unique_nptr<ContentUpdateTransportDownload>;

private:
    map<string, Factory> _factories {};
};

FO_END_NAMESPACE
