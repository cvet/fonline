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
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppClangTidyClangDiagnosticOldStyleCast
// ReSharper disable CppClangTidyReadabilityMagicNumbers

///@ CodeGen Template DataRegistration

///@ CodeGen Defines

#ifndef SERVER_REGISTRATION
#cmakedefine01 SERVER_REGISTRATION
#cmakedefine01 CLIENT_REGISTRATION
#endif

#include "Common.h"

#include "EngineBase.h"
#include "Properties.h"

#if SERVER_REGISTRATION
#include "Server.h"
#elif CLIENT_REGISTRATION
#include "Client.h"
#endif

enum class ScriptEnum_int8 : char
{
};
enum class ScriptEnum_uint8 : uchar
{
};
enum class ScriptEnum_int16 : short
{
};
enum class ScriptEnum_uint16 : ushort
{
};
enum class ScriptEnum_int : int
{
};
enum class ScriptEnum_uint : uint
{
};

///@ CodeGen Global

template<typename T>
static void RegisterProperty(PropertyRegistrator* registrator, Property::AccessType access, string_view name, const vector<string>& flags)
{
    registrator->Register<T>(access, name, flags);
}

#if SERVER_REGISTRATION

void FOServer::RegisterData()
{
    ResetRegisteredData();

    unordered_map<string, PropertyRegistrator*> registrators;
    PropertyRegistrator* registrator;

    ///@ CodeGen ServerRegister

    map<string, vector<string>> restoreInfo;

    ///@ CodeGen WriteRestoreInfo

    size_t estimated_size = sizeof(uint);
    for (const auto& [name, data] : restoreInfo) {
        estimated_size += sizeof(uint) + name.size();
        estimated_size += sizeof(uint);
        for (const auto& entry : data) {
            estimated_size += sizeof(uint) + entry.size();
        }
    }

    _restoreInfoBin.reserve(estimated_size + 1024u);

    auto writer = DataWriter(_restoreInfoBin);
    writer.Write<uint>(static_cast<uint>(restoreInfo.size()));
    for (const auto& [name, data] : restoreInfo) {
        writer.Write<uint>(static_cast<uint>(name.size()));
        writer.WritePtr(name.data(), name.size());
        writer.Write<uint>(static_cast<uint>(data.size()));
        for (const auto& entry : data) {
            writer.Write<uint>(static_cast<uint>(entry.size()));
            writer.WritePtr(entry.data(), entry.size());
        }
    }

    _restoreInfoBin.shrink_to_fit();

    FinalizeDataRegistration();
}

#endif

#if CLIENT_REGISTRATION

static void RestoreProperty(PropertyRegistrator* registrator, string_view access, string_view type, const string& name, const vector<string>& flags)
{
#define RESTORE_ARGS PropertyRegistrator *registrator, Property::AccessType access, string_view name, const vector<string>&flags
#define RESTORE_ARGS_PASS registrator, access, name, flags

    static unordered_map<string_view, std::function<void(PropertyRegistrator*, Property::AccessType, string_view, const vector<string>&)>> call_map = {
        ///@ CodeGen PropertyMap
    };

    static unordered_map<string_view, Property::AccessType> access_map = {
        {"PrivateCommon", Property::AccessType::PrivateCommon},
        {"PrivateClient", Property::AccessType::PrivateClient},
        {"PrivateServer", Property::AccessType::PrivateServer},
        {"Public", Property::AccessType::Public},
        {"PublicModifiable", Property::AccessType::PublicModifiable},
        {"PublicFullModifiable", Property::AccessType::PublicFullModifiable},
        {"PublicStatic", Property::AccessType::PublicStatic},
        {"Protected", Property::AccessType::Protected},
        {"ProtectedModifiable", Property::AccessType::ProtectedModifiable},
        {"VirtualPrivateCommon", Property::AccessType::VirtualPrivateCommon},
        {"VirtualPrivateClient", Property::AccessType::VirtualPrivateClient},
        {"VirtualPrivateServer", Property::AccessType::VirtualPrivateServer},
        {"VirtualPublic", Property::AccessType::VirtualPublic},
        {"VirtualProtected", Property::AccessType::VirtualProtected},
    };

    const auto it = call_map.find(type);
    if (it == call_map.end()) {
        throw DataRegistrationException("Invalid property for restoring", type);
    }

    it->second(registrator, access_map[access], name, flags);

#undef RESTORE_ARGS
#undef RESTORE_ARGS_PASS
}

void FOClient::RegisterData(const vector<uchar>& restore_info_bin)
{
    ResetRegisteredData();

    unordered_map<string, PropertyRegistrator*> registrators;
    PropertyRegistrator* registrator;

    ///@ CodeGen ClientRegister

    map<string, vector<string>> restoreInfo;

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

        restoreInfo.emplace(std::move(name), std::move(data));
    }

    // Restore enums
    for (const auto& info : restoreInfo["Enums"]) {
        auto tokens = _str(info).split(' ');
        // AS_VERIFY(engine->RegisterEnum(tokens[0].c_str()));
        // for (size_t i = 2; i < tokens.size() - 2; i += 2) {
        //    AS_VERIFY(engine->RegisterEnumValue(tokens[0].c_str(), tokens[i].c_str(), _str(tokens[i + 1]).toInt()));
        //}
    }

    // Restore properties
    for (const auto& info : restoreInfo["Properties"]) {
        const auto tokens = _str(info).split(' ');
        auto* prop_registrator = registrators[tokens[0]];
        auto flags = tokens;
        flags.erase(flags.begin(), flags.begin() + 4);
        RestoreProperty(prop_registrator, tokens[1], tokens[2], tokens[3], flags);
    }

    FinalizeDataRegistration();
}

#endif