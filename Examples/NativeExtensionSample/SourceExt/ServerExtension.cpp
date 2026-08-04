#include "Common.h"

#include "NativeExtensionCore.h"
#include "Server.h"

#if !defined(FO_NATIVE_EXTENSION_CORE_LINKED) || FO_NATIVE_EXTENSION_CORE_LINKED != 1
#error NativeExtensionCore was not linked to the SERVER role
#endif

FO_USING_NAMESPACE();

namespace
{

auto GetNativeExtensionState(ptr<ServerEngine> server) -> ptr<NativeExtensionSample::ServerState>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(server->UserData, "Native extension sample state is not initialized");
    return server->UserData.reinterpret_as<NativeExtensionSample::ServerState>();
}

}

FO_BEGIN_NAMESPACE

///@ EngineHook
FO_SCRIPT_API void ServerInitHook(ptr<ServerEngine> server);

// SyncScope: reads immutable state owned by this ServerEngine instance; no additional cover is required.
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_NativeExtensionValue(ptr<ServerEngine> server, int32_t delta);

FO_END_NAMESPACE

void FO_NAMESPACE ServerInitHook(ptr<ServerEngine> server)
{
    FO_VERIFY_AND_THROW(!server->UserData, "Native extension sample requires ownership of ServerEngine.UserData");

    auto state_storage = SafeAlloc::MakeUnique<NativeExtensionSample::ServerState>();
    state_storage->BaseValue = NativeExtensionSample::InitialBaseValue;
    auto state_ptr = state_storage.release();
    server->UserData = make_unique_del_ptr(state_ptr.reinterpret_as<uint8_t>(), [](ptr<uint8_t> storage) FO_DEFERRED {
        auto owned_state = adopt_unique_ptr(storage.reinterpret_as<NativeExtensionSample::ServerState>());
        ignore_unused(owned_state);
    });

    WriteLog("native_extension_hook_initialized");
}

int32_t FO_NAMESPACE Server_Game_NativeExtensionValue(ptr<ServerEngine> server, int32_t delta)
{
    return NativeExtensionSample::EvaluateValue(*GetNativeExtensionState(server), delta);
}
