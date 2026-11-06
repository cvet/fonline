// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "AnyData.h"

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

namespace DataBaseSerialization
{
    inline constexpr size_t MAX_DOCUMENT_BYTES {64 * 1024 * 1024};
    inline constexpr size_t MAX_DOCUMENT_DEPTH {64};

    auto ToJson(const AnyData::Document& doc) -> nlohmann::json;
    auto FromJson(const nlohmann::json& json) -> AnyData::Document;
    auto FromJsonText(string_view text) -> AnyData::Document;
#if FO_DATABASE_SQLITE
    auto ToCbor(const AnyData::Document& doc) -> vector<uint8_t>;
    auto FromCbor(const_span<uint8_t> data) -> AnyData::Document;
#endif
}

FO_END_NAMESPACE
