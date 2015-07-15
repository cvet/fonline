#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Item.h"
#include "FileManager.h"
#include "AI.h"
#include "Defines.h"
#ifdef FONLINE_MAPPER
# include "CritterCl.h"
#endif

struct ProtoCritter: public Entity
{
    ProtoCritter();

    hash     ProtoId;
    UIntVec  TextsLang;
    FOMsgVec Texts;

    #ifdef FONLINE_MAPPER
    string   CollectionName;
    #endif

    const char* GetName() { return HASH_STR( ProtoId ); }

    #ifdef FONLINE_MAPPER
    uint GetCrType()
    {
        return ( uint ) Properties::GetValueAsInt( this, CritterCl::PropertyBaseCrType->GetEnumValue() );
    }
    #endif
};
typedef map< hash, ProtoCritter* > ProtoCritterMap;
typedef vector< ProtoCritter* >    ProtoCritterVec;

struct CritData
{
    hash   ProtoId;
    uint   CrType;
    uchar  Cond;
    bool   ClientToDelete;
    ushort HexX;
    ushort HexY;
    uchar  Dir;
    int    Multihex;
    ushort WorldX;
    ushort WorldY;
    uint   MapId;
    hash   MapPid;
    uint   GlobalGroupUid;
    ushort LastMapHexX;
    ushort LastMapHexY;
    uint   Anim1Life;
    uint   Anim1Knockout;
    uint   Anim1Dead;
    uint   Anim2Life;
    uint   Anim2Knockout;
    uint   Anim2Dead;
    uint   Anim2KnockoutEnd;
    uchar  GlobalMapFog[ GM_ZONES_FOG_SIZE ];
    uint   UserData[ 10 ];
    uint   Reserved[ 10 ];
};

#endif // __CRITTER_DATA__
