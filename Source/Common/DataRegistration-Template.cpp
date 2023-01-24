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

#include "Common.h"

#include "EngineBase.h"
#include "Properties.h"
#include "ScriptSystem.h"
#include "StringUtils.h"

///@ CodeGen Global

#if !COMPILER_MODE
#if SERVER_REGISTRATION

void Server_RegisterData(FOEngineBase* engine)
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen ServerRegister

    engine->FinalizeDataRegistration();
}

#elif CLIENT_REGISTRATION

static void RestoreProperty(PropertyRegistrator* registrator, string_view access, string_view type, const string& name, const vector<string_view>& flags)
{
    STACK_TRACE_ENTRY();

#define RESTORE_ARGS PropertyRegistrator *registrator, Property::AccessType access, string_view name, const vector<string_view>&flags
#define RESTORE_ARGS_PASS access, name, flags

    static unordered_map<string_view, std::function<void(PropertyRegistrator*, Property::AccessType, string_view, const vector<string_view>&)>> call_map = {
        ///@ CodeGen PropertyMap
    };

    static unordered_map<string_view, Property::AccessType> access_map = {
        {"PrivateCommon", Property::AccessType::PrivateCommon},
        {"PrivateClient", Property::AccessType::PrivateClient},
        {"PrivateServer", Property::AccessType::PrivateServer},
        {"Public", Property::AccessType::Public},
        {"PublicModifiable", Property::AccessType::PublicModifiable},
        {"PublicFullModifiable", Property::AccessType::PublicFullModifiable},
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

void Client_RegisterData(FOEngineBase* engine, const vector<uchar>& restore_info_bin)
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen ClientRegister

    map<string, vector<string>> restoreInfo;

    auto reader = DataReader(restore_info_bin);

    const auto info_size = reader.Read<uint>();
    for (const auto i : xrange(info_size)) {
        UNUSED_VARIABLE(i);

        const auto name_size = reader.Read<uint>();
        auto name = string();
        name.resize(name_size);
        reader.ReadPtr(name.data(), name_size);

        const auto data_size = reader.Read<uint>();
        auto data = vector<string>();
        data.reserve(data_size);

        for (const auto e : xrange(data_size)) {
            UNUSED_VARIABLE(e);

            const auto entry_size = reader.Read<uint>();
            auto entry = string();
            entry.resize(entry_size);
            reader.ReadPtr(entry.data(), entry_size);

            data.emplace_back(std::move(entry));
        }

        restoreInfo.emplace(std::move(name), std::move(data));
    }

    reader.VerifyEnd();

    // Restore enums
    for (const auto& info : restoreInfo["Enums"]) {
        static unordered_map<string, const type_info*> enum_type_map = {
            {"int8", &typeid(char)},
            {"int16", &typeid(short)},
            {"int", &typeid(int)},
            {"uint8", &typeid(uchar)},
            {"uint16", &typeid(ushort)},
            {"uint", &typeid(uint)},
        };

        const auto tokens = _str(info).split(' ');
        const auto& enum_name = tokens[0];
        const auto* enum_type = enum_type_map[tokens[1]];

        unordered_map<string, int> key_values;
        for (size_t i = 2; i < tokens.size(); i++) {
            const auto kv = _str(tokens[i]).split('=');
            RUNTIME_ASSERT(kv.size() == 2);
            const auto key = kv[0];
            const auto value = _str(kv[1]).toInt();
            key_values.emplace(key, value);
        }

        engine->AddEnumGroup(enum_name, *enum_type, std::move(key_values));
    }

    // Restore property components
    for (const auto& info : restoreInfo["PropertyComponents"]) {
        const auto tokens = _str(info).split(' ');
        auto* prop_registrator = engine->GetOrCreatePropertyRegistrator(tokens[0]);
        prop_registrator->RegisterComponent(tokens[1]);
    }

    // Restore properties
    for (const auto& info : restoreInfo["Properties"]) {
        const auto tokens = _str(info).split(' ');
        auto* prop_registrator = engine->GetOrCreatePropertyRegistrator(tokens[0]);

        vector<string_view> flags;
        flags.reserve(tokens.size() - 4);
        for (size_t i = 4; i < tokens.size(); i++) {
            flags.emplace_back(tokens[i]);
        }

        RestoreProperty(prop_registrator, tokens[1], tokens[2], tokens[3], flags);
    }

    engine->FinalizeDataRegistration();
}

#elif SINGLE_REGISTRATION

void Single_RegisterData(FOEngineBase* engine)
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen SingleRegister

    engine->FinalizeDataRegistration();
}

#elif MAPPER_REGISTRATION

void Mapper_RegisterData(FOEngineBase* engine)
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen MapperRegister

    engine->FinalizeDataRegistration();
}

#elif BAKER_REGISTRATION

void Baker_RegisterData(FOEngineBase* engine)
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen BakerRegister

    engine->FinalizeDataRegistration();
}

auto Baker_GetRestoreInfo() -> vector<uchar>
{
    STACK_TRACE_ENTRY();

    map<string, vector<string>> restore_info;

    ///@ CodeGen WriteRestoreInfo

    size_t estimated_size = sizeof(uint);
    for (const auto& [name, data] : restore_info) {
        estimated_size += sizeof(uint) + name.size();
        estimated_size += sizeof(uint);
        for (const auto& entry : data) {
            estimated_size += sizeof(uint) + entry.size();
        }
    }

    vector<uchar> restore_info_bin;
    restore_info_bin.reserve(estimated_size + 1024u);

    auto writer = DataWriter(restore_info_bin);
    writer.Write<uint>(static_cast<uint>(restore_info.size()));
    for (const auto& [name, data] : restore_info) {
        writer.Write<uint>(static_cast<uint>(name.size()));
        writer.WritePtr(name.data(), name.size());
        writer.Write<uint>(static_cast<uint>(data.size()));
        for (const auto& entry : data) {
            writer.Write<uint>(static_cast<uint>(entry.size()));
            writer.WritePtr(entry.data(), entry.size());
        }
    }

    restore_info_bin.shrink_to_fit();

    return restore_info_bin;
}

#endif
#endif

#if COMPILER_MODE

#if SERVER_REGISTRATION
void AngelScript_ServerCompiler_RegisterData(FOEngineBase* engine)
#elif CLIENT_REGISTRATION
void AngelScript_ClientCompiler_RegisterData(FOEngineBase* engine)
#elif SINGLE_REGISTRATION
void AngelScript_SingleCompiler_RegisterData(FOEngineBase* engine)
#elif MAPPER_REGISTRATION
void AngelScript_MapperCompiler_RegisterData(FOEngineBase* engine)
#endif
{
    STACK_TRACE_ENTRY();

    ///@ CodeGen CompilerRegister

    engine->FinalizeDataRegistration();
}

#endif
