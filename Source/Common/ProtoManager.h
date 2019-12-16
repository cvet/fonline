#pragma once

#include "Common.h"
#include "Entity.h"

class ProtoManager
{
public:
    void ClearProtos();
    bool LoadProtosFromFiles();
    void GetBinaryData( UCharVec& data );
    void LoadProtosFromBinaryData( UCharVec& data );
    bool ValidateProtoResources( StrVec& resource_names );

    ProtoItem*     GetProtoItem( hash pid );
    ProtoCritter*  GetProtoCritter( hash pid );
    ProtoMap*      GetProtoMap( hash pid );
    ProtoLocation* GetProtoLocation( hash pid );

    const ProtoItemMap&     GetProtoItems()     { return itemProtos; }
    const ProtoCritterMap&  GetProtoCritters()  { return crProtos; }
    const ProtoMapMap&      GetProtoMaps()      { return mapProtos; }
    const ProtoLocationMap& GetProtoLocations() { return locProtos; }

private:
    ProtoItemMap     itemProtos;
    ProtoCritterMap  crProtos;
    ProtoMapMap      mapProtos;
    ProtoLocationMap locProtos;
};
