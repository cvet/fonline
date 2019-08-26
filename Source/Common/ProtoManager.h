#ifndef __PROTO_MANAGER__
#define __PROTO_MANAGER__

#include "Common.h"
#include "Entity.h"

class ProtoManager
{
private:
    ProtoItemMap    itemProtos;
    ProtoCritterMap crProtos;
    ProtoMapMap     mapProtos;
    ProtoLocMap     locProtos;

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

    const ProtoItemMap&    GetProtoItems()     { return itemProtos; }
    const ProtoCritterMap& GetProtoCritters()  { return crProtos; }
    const ProtoMapMap&     GetProtoMaps()      { return mapProtos; }
    const ProtoLocMap&     GetProtoLocations() { return locProtos; }
};

extern ProtoManager ProtoMngr;

#endif // __PROTO_MANAGER__
