#include "Common.h"

#include "Server.h"

FO_USING_NAMESPACE();

FO_BEGIN_NAMESPACE

///@ EngineHook
FO_SCRIPT_API CritterVisibilityMode CheckCritterVisibilityHook(const ServerEngine* server, const Map* map, const Critter* cr, const Critter* target);

FO_END_NAMESPACE

CritterVisibilityMode FO_NAMESPACE CheckCritterVisibilityHook(const ServerEngine* server, const Map* map, const Critter* cr, const Critter* target)
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(server);
    ignore_unused(map);

    return GeometryHelper::GetDistance(cr->GetHex(), target->GetHex()) <= cr->GetLookDistance() ? CritterVisibilityMode::Full : CritterVisibilityMode::None;
}
