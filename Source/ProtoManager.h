#ifndef __PROTO_MANAGER__
#define __PROTO_MANAGER__

#include "Common.h"
#include "Item.h"
#ifdef FONLINE_SERVER
# include "Critter.h"
# include "Map.h"
#else
# include "CritterCl.h"
# include "MapCl.h"
#endif

class ProtoManager
{
private:
    ProtoItemMap    itemProtos;
    ProtoCritterMap crProtos;
    ProtoMapMap     mapProtos;
    ProtoLocMap     locProtos;

	struct CustomProtoData
	{
		string fileExt;
		string appName;
		ProtoCustomEntityMap map;
	};

	CustomProtoData nullCustomProtos;
	std::vector<CustomProtoData*> customProtos;


public:
    void ClearProtos();
    bool LoadProtosFromFiles();
    void GetBinaryData( UCharVec& data );
    void LoadProtosFromBinaryData( UCharVec& data );
    bool ValidateProtoResources( StrVec& resource_names );

	void CreateCustomProtoMap( uint subType, string fileExt, string appName );

    ProtoItem*     GetProtoItem( hash pid );
    ProtoCritter*  GetProtoCritter( hash pid );
    ProtoMap*      GetProtoMap( hash pid );
    ProtoLocation* GetProtoLocation( hash pid );
	ProtoCustomEntity* GetProtoCustom(uint subType, hash pid);

	const ProtoItemMap& GetProtoItems();
	const ProtoCritterMap& GetProtoCritters();
	const ProtoMapMap& GetProtoMaps();
	const ProtoLocMap& GetProtoLocations();
	const ProtoCustomEntityMap& GetProtoCustoms(uint subType);
};

extern ProtoManager ProtoMngr;

#endif // __PROTO_MANAGER__
