#include "MapCl.h"

PROPERTIES_IMPL( Map );
CLASS_PROPERTY_IMPL( Map, FileDir );
CLASS_PROPERTY_IMPL( Map, Width );
CLASS_PROPERTY_IMPL( Map, Height );
CLASS_PROPERTY_IMPL( Map, WorkHexX );
CLASS_PROPERTY_IMPL( Map, WorkHexY );
CLASS_PROPERTY_IMPL( Map, LocId );
CLASS_PROPERTY_IMPL( Map, LocMapIndex );
CLASS_PROPERTY_IMPL( Map, RainCapacity );
CLASS_PROPERTY_IMPL( Map, CurDayTime );
CLASS_PROPERTY_IMPL( Map, ScriptId );
CLASS_PROPERTY_IMPL( Map, DayTime );
CLASS_PROPERTY_IMPL( Map, DayColor );
CLASS_PROPERTY_IMPL( Map, IsNoLogOut );

Map::Map( uint id, ProtoMap* proto ): Entity( id, EntityType::Map, PropertiesRegistrators[0], proto )
{
    RUNTIME_ASSERT( proto );
}

Map::~Map()
{
    //
}

PROPERTIES_IMPL( Location );
CLASS_PROPERTY_IMPL( Location, MapProtos );
CLASS_PROPERTY_IMPL( Location, MapEntrances );
CLASS_PROPERTY_IMPL( Location, MaxPlayers );
CLASS_PROPERTY_IMPL( Location, AutoGarbage );
CLASS_PROPERTY_IMPL( Location, GeckVisible );
CLASS_PROPERTY_IMPL( Location, EntranceScript );
CLASS_PROPERTY_IMPL( Location, WorldX );
CLASS_PROPERTY_IMPL( Location, WorldY );
CLASS_PROPERTY_IMPL( Location, Radius );
CLASS_PROPERTY_IMPL( Location, Hidden );
CLASS_PROPERTY_IMPL( Location, ToGarbage );
CLASS_PROPERTY_IMPL( Location, Color );

Location::Location( uint id, ProtoLocation* proto ): Entity( id, EntityType::Location, PropertiesRegistrators[0], proto )
{
    RUNTIME_ASSERT( proto );
}

Location::~Location()
{
    //
}
