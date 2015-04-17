#ifndef __CRITTER_DATA__
#define __CRITTER_DATA__

#include "Item.h"
#include "FileManager.h"
#include "AI.h"
#include "Defines.h"
#include "Properties.h"
#ifdef FONLINE_MAPPER
# include "CritterCl.h"
#endif

#define MAX_STORED_LOCATIONS    ( 1000 )

struct ProtoCritter
{
    hash        ProtoId;
    Properties* Props;
    #ifdef FONLINE_MAPPER
    string      CollectionName;
    #endif

    #ifdef FONLINE_MAPPER
    uint GetBaseType()
    {
        return (uint) Props->GetValueAsInt( CritterCl::PropertyBaseCrType->GetEnumValue() );
    }
    #endif
};
typedef map< hash, ProtoCritter* > ProtoCritterMap;
typedef vector< ProtoCritter* >    ProtoCritterVec;

struct CritData
{
    uint       Id;
    ushort     HexX;
    ushort     HexY;
    ushort     WorldX;
    ushort     WorldY;
    uint       BaseType;
    uchar      Dir;
    uchar      Cond;
    uchar      Reserved000;
    char       Reserved0;
    hash       ScriptId;
    uint       ShowCritterDist1;
    uint       ShowCritterDist2;
    uint       ShowCritterDist3;
    ushort     Reserved00;
    short      Multihex;
    uint       GlobalGroupUid;
    ushort     LastHexX;
    ushort     LastHexY;
    uint       Reserved1[ 4 ];
    uint       MapId;
    hash       MapPid;
    ushort     Reserved2;
    uint       Anim1Life;
    uint       Anim1Knockout;
    uint       Anim1Dead;
    uint       Anim2Life;
    uint       Anim2Knockout;
    uint       Anim2Dead;
    uint       Anim2KnockoutEnd;
    uint       Reserved3[ 3 ];
    uint       Reserved4[ 8 ];
    bool       ClientToDelete;
    uchar      Reserved5;
    ushort     Reserved6;
    uint       Temp;
    ushort     Reserved8;
    ushort     HoloInfoCount;
    uint       HoloInfo[ MAX_HOLO_INFO ];
    uint       Reserved9[ 10 ];
    int        Scores[ SCORES_MAX ];
    // Npc data
    uint       HomeMap;
    ushort     HomeX;
    ushort     HomeY;
    uchar      HomeOri;
    uchar      Reserved11;
    hash       ProtoId;
    uint       Reserved12;
    uint       Reserved13;
    uint       Reserved14;
    uint       Reserved15;
    bool       IsDataExt;
    uchar      Reserved16;
    ushort     Reserved17;
    uint       Reserved18[ 8 ];
    hash       FavoriteItemPid[ 4 ];
    uint       Reserved19[ 10 ];
    uint       EnemyStackCount;
    uint       EnemyStack[ MAX_ENEMY_STACK ];
    uint       Reserved20[ 5 ];
    uchar      BagCurrentSet[ MAX_NPC_BAGS_PACKS ];
    short      BagRefreshTime;
    uchar      Reserved21;
    uchar      BagSize;
    NpcBagItem Bag[ MAX_NPC_BAGS ];
    uint       Reserved22[ 100 ];
};

struct CritDataExt
{
    uint   Reserved23[ 10 ];
    uchar  GlobalMapFog[ GM_ZONES_FOG_SIZE ];
    ushort Reserved24;
    ushort LocationsCount;
    uint   LocationsId[ MAX_STORED_LOCATIONS ];
    uint   Reserved25[ 40 ];
    uint   PlayIp[ MAX_STORED_IP ];
    ushort PlayPort[ MAX_STORED_IP ];
    uint   CurrentIp;
    uint   Reserved26[ 29 ];
};

#endif // __CRITTER_DATA__
