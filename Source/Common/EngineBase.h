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
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#pragma once

#include "Common.h"

#include "EntityProperties.h"

DECLARE_EXCEPTION(HashResolveException);
DECLARE_EXCEPTION(HashCollisionException);

class FOEngineBase : public NameResolver, public PropertyRegistratorsHolder, public Entity, public GameProperties
{
public:
    FOEngineBase(const FOEngineBase&) = delete;
    FOEngineBase(FOEngineBase&&) noexcept = delete;
    auto operator=(const FOEngineBase&) = delete;
    auto operator=(FOEngineBase&&) noexcept = delete;

    [[nodiscard]] auto GetName() const -> string_view override { return "Engine"; }
    [[nodiscard]] auto ToHashedString(string_view s) const -> hstring override;
    [[nodiscard]] auto ResolveHash(hstring::hash_t h) const -> hstring override;
    [[nodiscard]] auto ResolveGenericValue(string_view str, bool& failed) -> int override;

protected:
    explicit FOEngineBase(bool is_server);
    ~FOEngineBase() override = default;

private:
    mutable unordered_map<hstring::hash_t, hstring::entry> _hashStorage {};
};
