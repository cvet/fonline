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

#pragma once

#include "Common.h"

#include "EngineBase.h"
#include "EntityProtos.h"
#include "FileSystem.h"

DECLARE_EXCEPTION(ProtoManagerException);

class ProtoManager final
{
public:
    explicit ProtoManager(FOEngineBase* engine);
    ProtoManager(const ProtoManager&) = delete;
    ProtoManager(ProtoManager&&) noexcept = delete;
    auto operator=(const ProtoManager&) = delete;
    auto operator=(ProtoManager&&) noexcept = delete;
    ~ProtoManager() = default;

    [[nodiscard]] auto GetProtosBinaryData() const -> vector<uchar>;
    [[nodiscard]] auto GetProtoItem(hstring proto_id) -> const ProtoItem*;
    [[nodiscard]] auto GetProtoCritter(hstring proto_id) -> const ProtoCritter*;
    [[nodiscard]] auto GetProtoMap(hstring proto_id) -> const ProtoMap*;
    [[nodiscard]] auto GetProtoLocation(hstring proto_id) -> const ProtoLocation*;
    [[nodiscard]] auto GetProtoItems() const -> const unordered_map<hstring, const ProtoItem*>&;
    [[nodiscard]] auto GetProtoCritters() const -> const unordered_map<hstring, const ProtoCritter*>&;
    [[nodiscard]] auto GetProtoMaps() const -> const unordered_map<hstring, const ProtoMap*>&;
    [[nodiscard]] auto GetProtoLocations() const -> const unordered_map<hstring, const ProtoLocation*>&;
    [[nodiscard]] auto GetAllProtos() const -> vector<const ProtoEntity*>;

    void ParseProtos(FileSystem& resources);
    void LoadFromResources();

private:
    FOEngineBase* _engine;
    unordered_map<hstring, const ProtoItem*> _itemProtos {};
    unordered_map<hstring, const ProtoCritter*> _crProtos {};
    unordered_map<hstring, const ProtoMap*> _mapProtos {};
    unordered_map<hstring, const ProtoLocation*> _locProtos {};
};
