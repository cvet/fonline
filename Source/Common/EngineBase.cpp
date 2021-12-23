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

auto FOEngineBase::HashToString(hash h) const -> string_view
{
    if (const auto it = _hashesLookup.find(h.Value); it != _hashesLookup.end()) {
        return it->second;
    }

    WriteLog("Can't resolve hash {}.\n", h.Value);
    return "";
}

auto FOEngineBase::StringToHash(string_view s) const -> hash
{
    if (const auto it = _hashesLookupRev.find(s); it != _hashesLookupRev.end()) {
        return hash {it->second};
    }

    const auto value = Hashing::MurmurHash2(reinterpret_cast<const uchar*>(s.data()), static_cast<uint>(s.length()));
    RUNTIME_ASSERT(value);

    if (const auto it = _hashesLookup.find(value); it != _hashesLookup.end()) {
        throw HashCollisionException("Hash collision", s, it->second, value);
    }

    _hashesLookup.emplace(value, s);
    _hashesLookupRev.emplace(s, value);

    return hash {value};
}

auto FOEngineBase::ResolveGenericValue(string_view str, bool& failed) -> int
{
    if (str.empty()) {
        failed = true;
        return 0;
    }

    if (str[0] == '@' && str[1] != 0) {
        static_assert(sizeof(hash::Value) == sizeof(int));
        return static_cast<int>(StringToHash(str.substr(1)).Value);
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
