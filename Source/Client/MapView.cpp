#include "MapView.h"
#include "Testing.h"

MapView::MapView( uint id, ProtoMap* proto ): Entity( id, EntityType::MapView, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );
}

PROPERTIES_IMPL( MapView );
CLASS_PROPERTY_IMPL( MapView, FileDir );
CLASS_PROPERTY_IMPL( MapView, Width );
CLASS_PROPERTY_IMPL( MapView, Height );
CLASS_PROPERTY_IMPL( MapView, WorkHexX );
CLASS_PROPERTY_IMPL( MapView, WorkHexY );
CLASS_PROPERTY_IMPL( MapView, LocId );
CLASS_PROPERTY_IMPL( MapView, LocMapIndex );
CLASS_PROPERTY_IMPL( MapView, RainCapacity );
CLASS_PROPERTY_IMPL( MapView, CurDayTime );
CLASS_PROPERTY_IMPL( MapView, ScriptId );
CLASS_PROPERTY_IMPL( MapView, DayTime );
CLASS_PROPERTY_IMPL( MapView, DayColor );
CLASS_PROPERTY_IMPL( MapView, IsNoLogOut );

LocationView::LocationView( uint id, ProtoLocation* proto ): Entity( id, EntityType::LocationView, PropertiesRegistrator, proto )
{
    RUNTIME_ASSERT( proto );
}

PROPERTIES_IMPL( LocationView );
CLASS_PROPERTY_IMPL( LocationView, MapProtos );
CLASS_PROPERTY_IMPL( LocationView, MapEntrances );
CLASS_PROPERTY_IMPL( LocationView, MaxPlayers );
CLASS_PROPERTY_IMPL( LocationView, AutoGarbage );
CLASS_PROPERTY_IMPL( LocationView, GeckVisible );
CLASS_PROPERTY_IMPL( LocationView, EntranceScript );
CLASS_PROPERTY_IMPL( LocationView, WorldX );
CLASS_PROPERTY_IMPL( LocationView, WorldY );
CLASS_PROPERTY_IMPL( LocationView, Radius );
CLASS_PROPERTY_IMPL( LocationView, Hidden );
CLASS_PROPERTY_IMPL( LocationView, ToGarbage );
CLASS_PROPERTY_IMPL( LocationView, Color );
