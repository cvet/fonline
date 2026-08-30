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

#include "ContentUpdateTransport.h"

FO_BEGIN_NAMESPACE

void ContentUpdateTransportRegistry::Register(string_view transport, Factory factory)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!transport.empty(), "Content update transport name is empty");
    FO_VERIFY_AND_THROW(factory, "Content update transport factory is empty", transport);

    auto insert_result = _factories.emplace(string(transport), std::move(factory));
    const bool inserted = insert_result.second;
    FO_VERIFY_AND_THROW(inserted, "Content update transport is already registered", transport);
}

auto ContentUpdateTransportRegistry::IsRegistered(string_view transport) const -> bool
{
    FO_STACK_TRACE_ENTRY();

    return _factories.contains(string(transport));
}

auto ContentUpdateTransportRegistry::Create(string_view transport, const ContentUpdateTransportRequest& request) const -> unique_nptr<ContentUpdateTransportDownload>
{
    FO_STACK_TRACE_ENTRY();

    const auto it = _factories.find(string(transport));

    if (it == _factories.end()) {
        return nullptr;
    }

    return it->second(request);
}

FO_END_NAMESPACE
