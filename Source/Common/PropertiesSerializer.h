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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(PropertySerializationException);

namespace PropertiesSerializer
{
    [[nodiscard]] auto SaveToDocument(ptr<const Properties> props, nptr<const Properties> base, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Document;
    [[nodiscard]] auto LoadFromDocument(ptr<Properties> props, const AnyData::Document& doc, hash_resolver& hashes, NameResolver& name_resolver) noexcept -> bool;
    [[nodiscard]] auto SavePropertyToValue(ptr<const Properties> props, ptr<const Property> prop, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Value;
    [[nodiscard]] auto SavePropertyToValue(ptr<const Property> prop, const_span<uint8_t> raw_data, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Value;
    [[nodiscard]] auto SavePropertyToText(ptr<const Properties> props, ptr<const Property> prop, hash_resolver& hashes, NameResolver& name_resolver) -> string;
    [[nodiscard]] auto SavePropertyToText(ptr<const Property> prop, const_span<uint8_t> raw_data, hash_resolver& hashes, NameResolver& name_resolver) -> string;
    void LoadPropertyFromValue(ptr<Properties> props, ptr<const Property> prop, const AnyData::Value& value, hash_resolver& hashes, NameResolver& name_resolver);
    void LoadPropertyFromValue(ptr<const Property> prop, const AnyData::Value& value, const function<void(const_span<uint8_t>)>& set_data, hash_resolver& hashes, NameResolver& name_resolver);
    void LoadPropertyFromText(ptr<Properties> props, ptr<const Property> prop, string_view text, hash_resolver& hashes, NameResolver& name_resolver);
}

FO_END_NAMESPACE
