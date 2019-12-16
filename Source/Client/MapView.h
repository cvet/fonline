#pragma once

#include "Common.h"
#include "Entity.h"

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
