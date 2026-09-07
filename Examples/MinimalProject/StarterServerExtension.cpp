#include "Common.h"

#include "Server.h"

#if !defined(FO_STARTER_PROJECT_DEPENDENCY) || FO_STARTER_PROJECT_DEPENDENCY != 1
#error Starter project dependency was not linked to the SERVER role
#endif

FO_USING_NAMESPACE();

FO_BEGIN_NAMESPACE

///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_NativeStarterValue(ptr<ServerEngine> server);

///@ EngineHook
FO_SCRIPT_API CritterVisibilityMode CheckCritterVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> cr, ptr<const Critter> target);

FO_END_NAMESPACE

int32_t FO_NAMESPACE Server_Game_NativeStarterValue(ptr<ServerEngine> server)
{
    ignore_unused(server);

    return 42;
}

CritterVisibilityMode FO_NAMESPACE CheckCritterVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> cr, ptr<const Critter> target)
{
    ignore_unused(server);
    ignore_unused(map);

    return GeometryHelper::GetDistance(cr->GetHex(), target->GetHex()) <= cr->GetLookDistance() ? CritterVisibilityMode::Full : CritterVisibilityMode::None;
}
