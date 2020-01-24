#pragma once

#include "Common.h"

#include "Entity.h"
#include "FileSystem.h"

class ProtoManager : public NonCopyable
{
public:
    void ClearProtos(); // Todo: remove ProtoManager::ClearProtos
    bool LoadProtosFromFiles(FileManager& file_mngr);
    void GetBinaryData(UCharVec& data);
    void LoadProtosFromBinaryData(UCharVec& data);
    bool ValidateProtoResources(StrVec& resource_names);

    ProtoItem* GetProtoItem(hash pid);
    ProtoCritter* GetProtoCritter(hash pid);
    ProtoMap* GetProtoMap(hash pid);
    ProtoLocation* GetProtoLocation(hash pid);

    const ProtoItemMap& GetProtoItems();
    const ProtoCritterMap& GetProtoCritters();
    const ProtoMapMap& GetProtoMaps();
    const ProtoLocationMap& GetProtoLocations();

private:
    ProtoItemMap itemProtos {};
    ProtoCritterMap crProtos {};
    ProtoMapMap mapProtos {};
    ProtoLocationMap locProtos {};
};
