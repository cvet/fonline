#pragma once

#include "Common.h"

#include "Entity.h"

class LocationView : public Entity
{
public:
    PROPERTIES_HEADER();
    CLASS_PROPERTY(CScriptArray*, MapProtos); // hash[]
    CLASS_PROPERTY(CScriptArray*, MapEntrances); // hash[]
    CLASS_PROPERTY(uint, MaxPlayers);
    CLASS_PROPERTY(bool, AutoGarbage);
    CLASS_PROPERTY(bool, GeckVisible);
    CLASS_PROPERTY(hash, EntranceScript);
    CLASS_PROPERTY(ushort, WorldX);
    CLASS_PROPERTY(ushort, WorldY);
    CLASS_PROPERTY(ushort, Radius);
    CLASS_PROPERTY(bool, Hidden);
    CLASS_PROPERTY(bool, ToGarbage);
    CLASS_PROPERTY(uint, Color);

    LocationView(uint id, ProtoLocation* proto);
    ~LocationView() = default;
};
