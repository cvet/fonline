#include "MapCl.h"

PROPERTIES_IMPL( MapCl );
CLASS_PROPERTY_IMPL( MapCl, FileDir );
CLASS_PROPERTY_IMPL( MapCl, Width );
CLASS_PROPERTY_IMPL( MapCl, Height );
CLASS_PROPERTY_IMPL( MapCl, WorkHexX );
CLASS_PROPERTY_IMPL( MapCl, WorkHexY );
CLASS_PROPERTY_IMPL( MapCl, LocId );
CLASS_PROPERTY_IMPL( MapCl, LocMapIndex );
CLASS_PROPERTY_IMPL( MapCl, RainCapacity );
CLASS_PROPERTY_IMPL( MapCl, CurDayTime );
CLASS_PROPERTY_IMPL( MapCl, ScriptId );
CLASS_PROPERTY_IMPL( MapCl, DayTime );
CLASS_PROPERTY_IMPL( MapCl, DayColor );
CLASS_PROPERTY_IMPL( MapCl, IsNoLogOut );

MapCl::MapCl( uint id, ProtoMap* proto ): Entity( id, EntityType::MapCl, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );
}

MapCl::~MapCl()
{
    //
}

PROPERTIES_IMPL( LocationCl );
CLASS_PROPERTY_IMPL( LocationCl, MapProtos );
CLASS_PROPERTY_IMPL( LocationCl, MapEntrances );
CLASS_PROPERTY_IMPL( LocationCl, MaxPlayers );
CLASS_PROPERTY_IMPL( LocationCl, AutoGarbage );
CLASS_PROPERTY_IMPL( LocationCl, GeckVisible );
CLASS_PROPERTY_IMPL( LocationCl, EntranceScript );
CLASS_PROPERTY_IMPL( LocationCl, WorldX );
CLASS_PROPERTY_IMPL( LocationCl, WorldY );
CLASS_PROPERTY_IMPL( LocationCl, Radius );
CLASS_PROPERTY_IMPL( LocationCl, Hidden );
CLASS_PROPERTY_IMPL( LocationCl, ToGarbage );
CLASS_PROPERTY_IMPL( LocationCl, Color );

LocationCl::LocationCl( uint id, ProtoLocation* proto ): Entity( id, EntityType::LocationCl, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );
}

LocationCl::~LocationCl()
{
    //
}
