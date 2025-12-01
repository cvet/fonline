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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#include "AnyData.h"
#include "Properties.h"

FO_BEGIN_NAMESPACE();

FO_DECLARE_EXCEPTION(PropertySerializationException);

class PropertiesSerializator final
{
public:
    PropertiesSerializator() = delete;

    [[nodiscard]] static auto SaveToDocument(const Properties* props, const Properties* base, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Document;
    [[nodiscard]] static auto LoadFromDocument(Properties* props, const AnyData::Document& doc, HashResolver& hash_resolver, NameResolver& name_resolver) noexcept -> bool;
    [[nodiscard]] static auto SavePropertyToValue(const Properties* props, const Property* prop, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value;
    [[nodiscard]] static auto SavePropertyToValue(const Property* prop, span<const uint8> raw_data, HashResolver& hash_resolver, NameResolver& name_resolver) -> AnyData::Value;
    static void LoadPropertyFromValue(Properties* props, const Property* prop, const AnyData::Value& value, HashResolver& hash_resolver, NameResolver& name_resolver);
    static void LoadPropertyFromValue(const Property* prop, const AnyData::Value& value, const function<void(span<const uint8>)>& set_data, HashResolver& hash_resolver, NameResolver& name_resolver);
};

FO_END_NAMESPACE();
