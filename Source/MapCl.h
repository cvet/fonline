#ifndef __MAP_CL__
#define __MAP_CL__

#include "Common.h"
#include "Entity.h"
#include "ProtoMap.h"

class Location;

class Map: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( ushort, Width );
    CLASS_PROPERTY( ushort, Height );
    CLASS_PROPERTY( ushort, WorkHexX );
    CLASS_PROPERTY( ushort, WorkHexY );
    CLASS_PROPERTY( uint, LocId );
    CLASS_PROPERTY( uint, LocMapIndex );
    CLASS_PROPERTY( uchar, RainCapacity );
    CLASS_PROPERTY( bool, IsTurnBasedAviable );
    CLASS_PROPERTY( int, CurDayTime );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( ScriptArray *, DayTime );    // 4 int
    CLASS_PROPERTY( ScriptArray *, DayColor );   // 12 uchar
    CLASS_PROPERTY( bool, IsNoLogOut );

    Map( uint id, ProtoMap* proto );
    ~Map();
};

class Location: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( ScriptArray *, MapProtos );    // hash[]
    CLASS_PROPERTY( ScriptArray *, MapEntrances ); // hash[]
    CLASS_PROPERTY( uint, MaxPlayers );
    CLASS_PROPERTY( bool, AutoGarbage );
    CLASS_PROPERTY( bool, GeckVisible );
    CLASS_PROPERTY( hash, EntranceScript );
    CLASS_PROPERTY( ushort, WorldX );
    CLASS_PROPERTY( ushort, WorldY );
    CLASS_PROPERTY( ushort, Radius );
    CLASS_PROPERTY( bool, Hidden );
    CLASS_PROPERTY( bool, ToGarbage );
    CLASS_PROPERTY( uint, Color );

    Location( uint id, ProtoLocation* proto );
    ~Location();
};
typedef map< uint, Location* > LocMap;
typedef vector< Location* >    LocVec;

#endif // __MAP_CL__
