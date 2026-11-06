// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnyData.h"
#include "Properties.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(PropertySerializationException);

namespace PropertiesSerializer
{
    auto MigrateDocument(ptr<const PropertyRegistrar> registrar, AnyData::Document& doc, nptr<AnyData::Document> updates = nullptr, optional<AnyData::Value> prototype = std::nullopt) -> bool;
    auto SaveToDocument(ptr<const Properties> props, nptr<const Properties> base, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Document;
    auto LoadFromDocument(ptr<Properties> props, const AnyData::Document& doc, hash_resolver& hashes, NameResolver& name_resolver) noexcept -> bool;
    auto SavePropertyToValue(ptr<const Properties> props, ptr<const Property> prop, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Value;
    auto SavePropertyToValue(ptr<const Property> prop, const_span<uint8_t> raw_data, hash_resolver& hashes, NameResolver& name_resolver) -> AnyData::Value;
    auto SavePropertyToText(ptr<const Properties> props, ptr<const Property> prop, hash_resolver& hashes, NameResolver& name_resolver) -> string;
    auto SavePropertyToText(ptr<const Property> prop, const_span<uint8_t> raw_data, hash_resolver& hashes, NameResolver& name_resolver) -> string;
    void LoadPropertyFromValue(ptr<Properties> props, ptr<const Property> prop, const AnyData::Value& value, hash_resolver& hashes, NameResolver& name_resolver);
    void LoadPropertyFromValue(ptr<const Property> prop, const AnyData::Value& value, const function<void(const_span<uint8_t>)>& set_data, hash_resolver& hashes, NameResolver& name_resolver);
    void LoadPropertyFromText(ptr<Properties> props, ptr<const Property> prop, string_view text, hash_resolver& hashes, NameResolver& name_resolver);
}

FO_END_NAMESPACE
