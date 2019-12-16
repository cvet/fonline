#include "LocationView.h"
#include "Testing.h"

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
