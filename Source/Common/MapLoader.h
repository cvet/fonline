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

#include "ConfigFile.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

// Baked map binaries carry no self-description, so a stale or foreign file has to be rejected by this marker
// instead of being read as element counts. Bump the version whenever the layout written by MapBaker changes
constexpr uint32_t BAKED_MAP_FILE_MAGIC = 0x424D4F46; // "FOMB"
constexpr uint32_t BAKED_MAP_FILE_VERSION = 1;

FO_DECLARE_EXCEPTION(MapLoaderException);

class EngineMetadata;

namespace MapLoader
{
    using CrLoadFunc = function<void(ident_t id, ptr<const ProtoCritter> proto, ptr<const map<string_view, string_view>> kv)>;
    using ItemLoadFunc = function<void(ident_t id, ptr<const ProtoItem> proto, ptr<const map<string_view, string_view>> kv)>;

    void Load(string_view name, string_view file_name, const string& buf, const EngineMetadata& meta, hash_resolver& hashes, const CrLoadFunc& cr_load, const ItemLoadFunc& item_load);
    auto EnumerateMaps(string_view file_name, const string& buf) -> vector<string>;
    void ReadBakedFileHeader(data_reader& reader, string_view map_name);
}

FO_END_NAMESPACE
