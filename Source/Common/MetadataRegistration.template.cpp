// FOnline Engine
// SPDX-License-Identifier: MIT

///@ CodeGen Template MetadataRegistration

#include "MetadataRegistration.h"

///@ CodeGen ValueIncludes

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

///@ CodeGen RegisterHelpers

#if !STUB_MODE
#if SERVER_REGISTRATION
void RegisterServerMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#elif CLIENT_REGISTRATION
void RegisterClientMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#elif MAPPER_REGISTRATION
void RegisterMapperMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#endif
#else
#if SERVER_REGISTRATION
void RegisterServerStubMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#elif CLIENT_REGISTRATION
void RegisterClientStubMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#elif MAPPER_REGISTRATION
void RegisterMapperStubMetadata(ptr<EngineMetadata> meta, nptr<const FileSystem> resources)
#endif
#endif
{
    FO_TRACE_ZONE(Engine);

#if SERVER_REGISTRATION
    meta->RegisterSide(EngineSideKind::ServerSide);
#elif CLIENT_REGISTRATION
    meta->RegisterSide(EngineSideKind::ClientSide);
#elif MAPPER_REGISTRATION
    meta->RegisterSide(EngineSideKind::MapperSide);
#endif

    ///@ CodeGen Register

    if (resources) {
#if SERVER_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Server"));
#elif CLIENT_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Client"));
#elif MAPPER_REGISTRATION
        RegisterDynamicMetadata(meta, ReadMetadataBin(resources, "Mapper"));
#endif
    }
}

#undef SERVER_REGISTRATION
#undef CLIENT_REGISTRATION
#undef MAPPER_REGISTRATION
#undef STUB_MODE

FO_END_NAMESPACE
