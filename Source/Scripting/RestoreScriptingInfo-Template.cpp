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

// ReSharper disable CppInconsistentNaming
// ReSharper disable CppCStyleCast
// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppClangTidyReadabilityQualifiedAuto
// ReSharper disable CppClangTidyModernizeUseNodiscard
// ReSharper disable CppClangTidyClangDiagnosticExtraSemiStmt
// ReSharper disable CppUseAuto

///@ CodeGen Template RestoreInfo

///@ CodeGen Defines

#if SERVER_SCRIPTING
#include "ServerScripting.h"
#elif CLIENT_SCRIPTING
#include "ClientScripting.h"
#endif

#if SERVER_SCRIPTING

auto ServerScriptSystem::GetInfoForRestore() const -> const vector<uchar>&
{
    RUNTIME_ASSERT(!_restoreInfoBin.empty());

    return _restoreInfoBin;
}

void ServerScriptSystem::AddRestoreInfo(string_view key, vector<string>&& info)
{
    _restoreInfo.emplace(string(key), std::move(info));
}

void ServerScriptSystem::WriteRestoreInfo()
{
    ///@ CodeGen Body

    size_t estimated_size = sizeof(uint);
    for (const auto& [name, data] : _restoreInfo) {
        estimated_size += sizeof(uint) + name.size();
        estimated_size += sizeof(uint);
        for (const auto& entry : data) {
            estimated_size += sizeof(uint) + entry.size();
        }
    }

    _restoreInfoBin.reserve(estimated_size + 1024u);

    auto writer = DataWriter(_restoreInfoBin);
    writer.Write<uint>(static_cast<uint>(_restoreInfo.size()));
    for (const auto& [name, data] : _restoreInfo) {
        writer.Write<uint>(static_cast<uint>(name.size()));
        writer.WritePtr(name.data(), name.size());
        writer.Write<uint>(static_cast<uint>(data.size()));
        for (const auto& entry : data) {
            writer.Write<uint>(static_cast<uint>(entry.size()));
            writer.WritePtr(entry.data(), entry.size());
        }
    }

    _restoreInfoBin.shrink_to_fit();
    _restoreInfo = {};
}

#endif

#if CLIENT_SCRIPTING

void ClientScriptSystem::PrepareRestoreInfo(const vector<uchar>& restore_info_bin)
{
    auto reader = DataReader(restore_info_bin);

    const auto info_size = reader.Read<uint>();
    for (const auto i : xrange(info_size)) {
        const auto name_size = reader.Read<uint>();
        auto name = string();
        name.resize(name_size);
        reader.ReadPtr(name.data(), name_size);

        const auto data_size = reader.Read<uint>();
        auto data = vector<string>();
        data.reserve(data_size);

        for (const auto e : xrange(data_size)) {
            const auto entry_size = reader.Read<uint>();
            auto entry = string();
            entry.resize(entry_size);
            reader.ReadPtr(entry.data(), entry_size);

            data.emplace_back(std::move(entry));
        }

        _restoreInfo.emplace(std::move(name), std::move(data));
    }
}

#endif
