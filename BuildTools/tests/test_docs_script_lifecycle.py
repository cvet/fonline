from __future__ import annotations

import unittest
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]


class ScriptLifecycleDocumentationTests(unittest.TestCase):
    def _read(self, relative_path: str) -> str:
        return (ENGINE_ROOT / relative_path).read_text(encoding="utf-8")

    def test_guide_covers_the_source_owned_lifecycle_boundaries(self) -> None:
        guide = self._read("Docs/en/how-to/scripting/lifecycle-and-concurrency.md")

        for heading in (
            "## Module initialization",
            "## Events and callback ownership",
            "### Persistence preload boundary",
            "## Entity `InitScript` callbacks",
            "## Async propagation and `Yield`",
            "## Server entity synchronization",
            "### Native entry covers",
            "## Mutable state ownership",
            "## Destruction and shutdown",
            "## Validation routes",
        ):
            self.assertIn(heading, guide)

        for contract in (
            "ascending priority",
            "transitive marker",
            "Covers do not survive suspension",
            "replaces the current entity cover",
            "Do not call `Game.Sync(...)` while `Game.Lock()` is held",
            "Entity::MarkAsDestroyed()",
            "Game.OnCritterPreLoad",
            "An unresolved name or mismatched signature throws `ScriptException`",
            "SetupScript(typedFunction)",
            "Process_RemoteCall()",
            "ShutDownAndRelease()",
        ):
            self.assertIn(contract, guide)

    def test_module_init_and_mutable_global_claims_match_runtime_markers(self) -> None:
        script_system = self._read("Source/Common/ScriptSystem.cpp")
        backend = self._read("Source/Scripting/AngelScript/AngelScriptBackend.cpp")

        self.assertIn("std::ranges::stable_sort", script_system)
        self.assertLess(script_system.index("UnfreezeGlobalVars();"), script_system.index("FreezeGlobalVars();"))
        self.assertIn('_scriptSys->AddInitFunc(std::move(func_wrapper), priority);', backend)
        self.assertIn("MutableGlobalsAllowedNamespaces", backend)
        self.assertIn("mutable global variable(s)", backend)

    def test_callback_async_and_scheduling_claims_match_runtime_markers(self) -> None:
        attributes = self._read("Source/Scripting/AngelScript/AngelScriptAttributes.cpp")
        globals_source = self._read("Source/Scripting/AngelScript/AngelScriptGlobals.cpp")
        client = self._read("Source/Client/Client.cpp")

        self.assertIn('CallbackAttributeRule {.AttributeName = "Event"', attributes)
        self.assertIn('CallbackAttributeRule {.AttributeName = "TimeEvent"', attributes)
        self.assertIn("caller must carry the same marker", attributes)
        self.assertIn('SetFunctionAttributes(as_engine->GetFunctionById(yield_id), {"Async"});', globals_source)
        self.assertIn("Execute only callbacks that were due when this pass began", client)

    def test_server_cover_and_entity_teardown_claims_match_runtime_markers(self) -> None:
        server = self._read("Source/Server/Server.cpp")
        entity_manager = self._read("Source/Server/EntityManager.cpp")
        sync_header = self._read("Source/Server/EntitySync.h")
        sync_source = self._read("Source/Server/EntitySync.cpp")
        methods = self._read("Source/Scripting/ServerGlobalScriptMethods.cpp")
        entity = self._read("Source/Common/Entity.cpp")

        self.assertIn("ScopedSyncContext nested;", server)
        self.assertIn("_ctx.Release();", sync_header)
        self.assertIn("_ctx.Deactivate();", sync_header)
        self.assertIn("Cannot call Sync() while holding a singleton lock", sync_source)
        self.assertIn("replaces current cover", methods)
        self.assertIn("ReleaseLocks();", sync_source)
        self.assertIn("ReleaseSingletonLocks();", sync_source)
        remote_call = server[server.index("void ServerEngine::Process_RemoteCall") :]
        self.assertLess(remote_call.index("ValidateInboundRemoteCallData"), remote_call.index("ctx->SyncEntity(player);"))
        self.assertLess(remote_call.index("ctx->SyncEntity(player);"), remote_call.index("HandleInboundRemoteCall"))
        preload = entity_manager[
            entity_manager.index("auto EntityManager::LoadCritter(") :
            entity_manager.index("auto EntityManager::LoadItem(")
        ]
        self.assertLess(preload.index("LoadInnerEntities(cr"), preload.index("OnCritterPreLoad.Fire(cr)"))
        self.assertLess(preload.index("LockMapTransfers();"), preload.index("OnCritterPreLoad.Fire(cr)"))
        self.assertLess(preload.index("OnCritterPreLoad.Fire(cr)"), preload.index("if (cr->IsDestroyed())"))
        mark_destroyed = entity[entity.index("void Entity::MarkAsDestroyed()") :]
        self.assertLess(mark_destroyed.index("UnsubscribeAllEvents();"), mark_destroyed.index("_isDestroyed.store"))
        self.assertLess(mark_destroyed.index("ClearAllTimeEvents();"), mark_destroyed.index("_isDestroyed.store"))

    def test_entity_init_script_claims_match_runtime_markers(self) -> None:
        script_system = self._read("Source/Common/ScriptSystem.h")
        entity_manager = self._read("Source/Server/EntityManager.cpp")
        baker = self._read("Source/Tools/Baker.cpp")

        self.assertIn("Init function not found or has a mismatched signature", script_system)
        self.assertIn("Script function signature does not match property binding", baker)
        location_init = entity_manager[entity_manager.index("void EntityManager::CallInit(ptr<Location>") :]
        self.assertLess(location_init.index("loc->SetInitCalled();"), location_init.index("_engine->OnLocationInit.Fire"))
        self.assertLess(location_init.index("_engine->OnLocationInit.Fire"), location_init.index("CallInitScript"))

    def test_script_object_shutdown_claims_match_runtime_markers(self) -> None:
        backend = self._read("Source/Scripting/AngelScript/AngelScriptBackend.cpp")
        call_bridge = self._read("Source/Scripting/AngelScript/AngelScriptCall.cpp")
        script_engine = self._read("ThirdParty/AngelScript/sdk/angelscript/source/as_scriptengine.cpp")

        backend_shutdown = backend[backend.index("AngelScriptBackend::~AngelScriptBackend()") :]
        self.assertLess(backend_shutdown.index("_contextMngr.reset();"), backend_shutdown.index("ShutDownAndRelease();"))
        self.assertLess(backend_shutdown.index("ShutDownAndRelease();"), backend_shutdown.index("_meta.reset();"))
        shutdown = script_engine[script_engine.index("int asCScriptEngine::ShutDownAndRelease()") :]
        self.assertLess(shutdown.index("scriptModules[n]->CallExit();"), shutdown.index("collectGarbageUntilStable();"))
        self.assertLess(shutdown.index("scriptModules[n]->Discard();"), shutdown.rindex("collectGarbageUntilStable();"))
        self.assertIn("gc.ReportAndReleaseUndestroyedObjects();", shutdown)
        self.assertIn("as_engine->ReleaseScriptObject(cur_obj.get(), as_ret_type.get());", call_bridge)


if __name__ == "__main__":
    unittest.main()
