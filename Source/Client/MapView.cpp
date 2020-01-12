#include "MapView.h"
#include "Testing.h"

MapView::MapView(uint id, ProtoMap* proto) : Entity(id, EntityType::MapView, PropertiesRegistrator, proto)
{
    RUNTIME_ASSERT(proto);
}

PROPERTIES_IMPL(MapView);
CLASS_PROPERTY_IMPL(MapView, FileDir);
CLASS_PROPERTY_IMPL(MapView, Width);
CLASS_PROPERTY_IMPL(MapView, Height);
CLASS_PROPERTY_IMPL(MapView, WorkHexX);
CLASS_PROPERTY_IMPL(MapView, WorkHexY);
CLASS_PROPERTY_IMPL(MapView, LocId);
CLASS_PROPERTY_IMPL(MapView, LocMapIndex);
CLASS_PROPERTY_IMPL(MapView, RainCapacity);
CLASS_PROPERTY_IMPL(MapView, CurDayTime);
CLASS_PROPERTY_IMPL(MapView, ScriptId);
CLASS_PROPERTY_IMPL(MapView, DayTime);
CLASS_PROPERTY_IMPL(MapView, DayColor);
CLASS_PROPERTY_IMPL(MapView, IsNoLogOut);
