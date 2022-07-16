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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: improve deferred calls

#pragma once

#include "Common.h"

#include "DeferredCalls.h"

class FOServer;

class ServerDeferredCallManager final : public DeferredCallManager
{
public:
    ServerDeferredCallManager() = delete;
    explicit ServerDeferredCallManager(FOServer* engine);
    ServerDeferredCallManager(const ServerDeferredCallManager&) = delete;
    ServerDeferredCallManager(ServerDeferredCallManager&&) noexcept = delete;
    auto operator=(const ServerDeferredCallManager&) = delete;
    auto operator=(ServerDeferredCallManager&&) noexcept = delete;
    ~ServerDeferredCallManager() override = default;

    void LoadDeferredCalls();
    auto AddSavedDeferredCall(uint delay, string_view func_name, const int* value, const vector<int>* values, const uint* value2, const vector<uint>* values2) -> uint;

private:
    auto GetNextId() -> uint override;
    void OnDeferredCallRemoved(const DeferredCall& call) override;

    FOServer* _serverEngine;
    unordered_set<uint> _savedCalls {};
};
