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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// ReSharper disable CppClangTidyReadabilityRedundantDeclaration
// ReSharper disable CppClangTidyClangDiagnosticCastFunctionTypeStrict
// ReSharper disable CppClangTidyModernizeUseDesignatedInitializers

///@ CodeGen Template MetadataRegistration

#include "MetadataRegistration.h"

///@ CodeGen Defines

#if !STUB_MODE
#if SERVER_REGISTRATION
#include "Server.h"
#elif CLIENT_REGISTRATION
#include "Client.h"
#elif MAPPER_REGISTRATION
#include "Mapper.h"
#endif
#endif

#if !STUB_MODE
///@ CodeGen Includes
#endif

FO_BEGIN_NAMESPACE

///@ CodeGen Global

#if !STUB_MODE
#if SERVER_REGISTRATION
void RegisterServerMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#elif CLIENT_REGISTRATION
void RegisterClientMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#elif MAPPER_REGISTRATION
void RegisterMapperMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#endif
#else
#if SERVER_REGISTRATION
void RegisterServerStubMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#elif CLIENT_REGISTRATION
void RegisterClientStubMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#elif MAPPER_REGISTRATION
void RegisterMapperStubMetadata(EngineMetadata* meta, const FileSystem* resources, bool dont_finalize)
#endif
#endif
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(meta);

    const auto resolve_type = [&](string_view type_str) { return meta->ResolveComplexType(type_str); };

#if SERVER_REGISTRATION
    meta->RegisterSide(EngineSideKind::ServerSide);
#elif CLIENT_REGISTRATION
    meta->RegisterSide(EngineSideKind::ClientSide);
#elif MAPPER_REGISTRATION
    meta->RegisterSide(EngineSideKind::MapperSide);
#endif

    ///@ CodeGen Register

    if (resources != nullptr) {
#if SERVER_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Server"));
#elif CLIENT_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Client"));
#elif MAPPER_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Mapper"));
#endif
    }

    if (!dont_finalize) {
        meta->FinalizeRegistration();
    }
}

#undef SERVER_REGISTRATION
#undef CLIENT_REGISTRATION
#undef MAPPER_REGISTRATION
#undef STUB_MODE

FO_END_NAMESPACE
