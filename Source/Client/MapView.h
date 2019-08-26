#pragma once

#include "Common.h"
#include "Entity.h"

class LocationView: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( CScriptArray *, MapProtos );    // hash[]
    CLASS_PROPERTY( CScriptArray *, MapEntrances ); // hash[]
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

    LocationView( uint id, ProtoLocation* proto );
    ~LocationView() = default;
};

class MapView: public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY( string, FileDir );
    CLASS_PROPERTY( ushort, Width );
    CLASS_PROPERTY( ushort, Height );
    CLASS_PROPERTY( ushort, WorkHexX );
    CLASS_PROPERTY( ushort, WorkHexY );
    CLASS_PROPERTY( uint, LocId );
    CLASS_PROPERTY( uint, LocMapIndex );
    CLASS_PROPERTY( uchar, RainCapacity );
    CLASS_PROPERTY( int, CurDayTime );
    CLASS_PROPERTY( hash, ScriptId );
    CLASS_PROPERTY( CScriptArray *, DayTime );      // 4 int
    CLASS_PROPERTY( CScriptArray *, DayColor );     // 12 uchar
    CLASS_PROPERTY( bool, IsNoLogOut );

    MapView( uint id, ProtoMap* proto );
    ~MapView() = default;
};
