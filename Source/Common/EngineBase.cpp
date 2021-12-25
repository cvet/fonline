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

#include "EngineBase.h"
#include "GenericUtils.h"
#include "Log.h"
#include "StringUtils.h"

FOEngineBase::FOEngineBase(bool is_server) : PropertyRegistratorsHolder(is_server), Entity(GetPropertyRegistrator(ENTITY_CLASS_NAME)), GameProperties(GetInitRef())
{
}

auto FOEngineBase::ToHashedString(string_view s) const -> hstring
{
    static_assert(std::is_same_v<hstring::hash_t, decltype(Hashing::MurmurHash2(nullptr, 0))>);

    if (s.empty()) {
        return hstring();
    }

    const auto hash_value = Hashing::MurmurHash2(reinterpret_cast<const uchar*>(s.data()), static_cast<uint>(s.length()));
    RUNTIME_ASSERT(hash_value != 0u);

    if (const auto it = _hashStorage.find(hash_value); it != _hashStorage.end()) {
#if FO_DEBUG
        const auto collision_detected = (s != it->second.Str);
#else
        const auto collision_detected = (s.length() != it->second.Str.length() && s != it->second.Str);
#endif
        if (collision_detected) {
            throw HashCollisionException("Hash collision", s, it->second.Str, hash_value);
        }

        return hstring(&it->second);
    }

    const auto [it, inserted] = _hashStorage.emplace(hash_value, hstring::entry {hash_value, string(s)});
    RUNTIME_ASSERT(inserted);

    return hstring(&it->second);
}

auto FOEngineBase::ResolveHash(hstring::hash_t h) const -> hstring
{
    if (const auto it = _hashStorage.find(h); it != _hashStorage.end()) {
        return hstring(&it->second);
    }

    throw HashResolveException("Can't resolve hash", h);
}

auto FOEngineBase::ResolveGenericValue(string_view str, bool& failed) -> int
{
    if (str.empty()) {
        failed = true;
        return 0;
    }

    if (str[0] == '@' && str[1] != 0) {
        return ToHashedString(str.substr(1)).as_int();
    }
    else if (_str(str).isNumber()) {
        return _str(str).toInt();
    }
    else if (_str(str).compareIgnoreCase("true")) {
        return 1;
    }
    else if (_str(str).compareIgnoreCase("false")) {
        return 0;
    }

    return ResolveEnumValue(str, failed);
}
