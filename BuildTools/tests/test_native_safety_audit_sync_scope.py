"""Regression tests for the native sync-scope (entity-cover boundary) audit.

Fixtures are written to temporary project trees with an ``Engine/`` checkout
and an optional ``SourceExt/`` project scope; only the final tests read the
real Engine sources of this checkout.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from dataclasses import replace
from pathlib import Path

import pytest


BUILDTOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS))

from native_safety_audit import scope  # noqa: E402
from native_safety_audit import sync_scope as audit  # noqa: E402


CLI = BUILDTOOLS / "native_safety_audit.py"
ENGINE_ROOT = BUILDTOOLS.parent


def write_tree(root: Path, files: dict[str, str]) -> None:
    for relative, text in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def fixture_context(root: Path, config: dict[str, object] | None = None) -> scope.AuditContext:
    config_path = None

    if config is not None:
        config_path = root / "audit-config.json"
        config_path.write_text(json.dumps(config), encoding="utf-8")

    scopes = ["SourceExt"] if (root / "SourceExt").is_dir() else []

    return scope.create_context(root, root / "Engine", scopes, config_path)


def collect_context(context: scope.AuditContext) -> list[audit.Function]:
    sources, stale = audit.source_files(context)
    assert stale == []

    return audit.collect_functions(sources)


def analyze(source: str, relative: str = "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp") -> list[audit.Diagnostic]:
    return analyze_files({relative: source})


def analyze_files(sources: dict[str, str], config: dict[str, object] | None = None) -> list[audit.Diagnostic]:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir).resolve()
        write_tree(root, fixture_sources(sources))
        context = fixture_context(root, config)

        return audit.analyze(collect_context(context), audit.rules_for(context))


def collect_files(sources: dict[str, str]) -> list[audit.Function]:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir).resolve()
        write_tree(root, fixture_sources(sources))

        return collect_context(fixture_context(root))


def fixture_sources(sources: dict[str, str]) -> dict[str, str]:
    canonical_sources = {
        "Engine/Source/Common/Entity.h": """
inline void ValidateEntityAccess(nptr<const Entity> entity)
{
    if (entity) {
        entity->ValidateAccess();
    }
}

inline void Entity::LockForPropertyAccess() noexcept
{
}

inline void Entity::LockForPropertyAccessShared() noexcept
{
}

inline void Entity::UnlockForPropertyAccess() noexcept
{
}

inline void Entity::UnlockForPropertyAccessShared() noexcept
{
}
""",
        "Engine/Source/Server/EntitySync.cpp": """
auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose) noexcept -> bool
{
    return true;
}

auto EntityLock::HasForeignDescendantHolder(std::thread::id self) const noexcept -> bool
{
    for (const auto& [tid, count] : _descendantHolders) {
        if (tid != self && count > 0) {
            return true;
        }
    }
    return false;
}

auto EntityLock::HasWaitingExclusive() const noexcept -> bool
{
    return std::ranges::any_of(_waitQueue, [](const WaitEntry& e) { return e.Kind == WaitKind::Exclusive && e.State.load(std::memory_order_acquire) == WaitEntry::STATE_WAITING; });
}

bool EntityLock::TryLockStateMutex()
{
    return _mutex.try_lock();
}

void EntityLock::UnlockStateMutex() noexcept
{
    _mutex.unlock();
}

bool EntityLock::IsEnsureOpCompatible(bool is_exclusive) const noexcept
{
    const auto this_thread = std::this_thread::get_id();
    const auto owner_thread = _ownerThread.load(std::memory_order_acquire);

    if (is_exclusive) {
        return owner_thread == this_thread || (owner_thread == std::thread::id {} && _sharedHolders.empty() && !HasForeignDescendantHolder(this_thread));
    }

    return owner_thread == this_thread || _descendantHolders.contains(this_thread) || (owner_thread == std::thread::id {} && !HasWaitingExclusive());
}

void EntityLock::CommitEnsureOp(bool is_exclusive) noexcept
{
    const auto this_thread = std::this_thread::get_id();
    const auto owner_thread = _ownerThread.load(std::memory_order_relaxed);

    if (is_exclusive) {
        if (owner_thread == this_thread) {
            _recursionCount++;
        }
        else {
            _ownerThread.store(this_thread, std::memory_order_release);
            _recursionCount = 1;
            tsan_acquire(this);
        }
    }
    else if (owner_thread == this_thread || _descendantHolders.contains(this_thread)) {
        _descendantHolders[this_thread]++;
    }
    else {
        _descendantHolders.emplace(this_thread, 1);
    }
}

void SyncContext::EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    if (!IsEntityAccessValid(entity, true)) {
        throw EntitySyncException();
    }
    EnsureEntitySyncedImpl(entity);
}

void SyncContext::EnsureEntitySyncedImpl(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    for (size_t i = 1; i < ops.size(); i++) {
        FO_VERIFY_AND_THROW(ops[i - 1].first < ops[i].first, "Ensure acquisition operations must be sorted and unique", i);
    }

    vector<ptr<EntityLock>> state_locked;
    state_locked.reserve(ops.size());

    auto unlock_state = scope_exit([&state_locked]() noexcept {
        for (auto it = state_locked.rbegin(); it != state_locked.rend(); ++it) {
            (*it)->UnlockStateMutex();
        }
    });

    for (const auto& [const_lock, is_exclusive] : ops) {
        ignore_unused(is_exclusive);
        auto lock = const_lock;

        if (!lock->TryLockStateMutex()) {
            BackoffBeforeSyncRetry(0);
            return;
        }

        state_locked.emplace_back(lock);
    }

    for (const auto& [lock, is_exclusive] : ops) {
        if (!lock->IsEnsureOpCompatible(is_exclusive)) {
            BackoffBeforeSyncRetry(0);
            return;
        }
    }

    for (const auto& [const_lock, is_exclusive] : ops) {
        auto lock = const_lock;
        lock->CommitEnsureOp(is_exclusive);
    }
}

void EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    auto ctx = SyncContext::GetCurrentOnThisThread();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->EnsureEntitySynced(entity);
}

void SyncContext::EnsureFreshEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    FO_VERIFY_AND_THROW(entity, "Fresh entity capture requires a non-null entity");
    FO_VERIFY_AND_THROW(!entity->IsDestroying(), "Fresh entity capture cannot retain an entity that is being destroyed", entity->GetName(), entity->GetId());
    FO_VERIFY_AND_THROW(!entity->IsDestroyed(), "Fresh entity capture cannot retain a destroyed entity", entity->GetName(), entity->GetId());
    FO_VERIFY_AND_THROW(!entity->GetParentRaw(), "Fresh entity capture requires an unpublished parentless entity", entity->GetName(), entity->GetId());

    EnsureEntitySyncedImpl(entity);
}
""",
    }
    canonical_sources.update(sources)

    return canonical_sources


def replace_function_body(
    functions: list[audit.Function],
    name: str,
    body: str,
) -> list[audit.Function]:
    matches = [function for function in functions if function.name == name]
    assert len(matches) == 1, name
    return [
        replace(function, body=body) if function is matches[0] else function
        for function in functions
    ]


def test_reports_indirect_sync_entities_path() -> None:
    diagnostics = analyze(
        """
void NativeHelper(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntities({});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    NativeHelper(ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Server_Game_Bad"
    assert diagnostics[0].sink == "SyncContext::SyncEntities"
    assert [function.name for function in diagnostics[0].path] == ["Server_Game_Bad", "NativeHelper"]


def test_same_named_sync_context_method_in_wrong_file_exposes_its_sink() -> None:
    # A wrong-file same-named method does not inherit the exact-terminal skip:
    # it stays in the call graph, so its hidden SyncEntity sink is exposed
    diagnostics = analyze(
        """
void SyncContext::EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    SyncEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    ctx->EnsureEntitySynced(entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_exact_ensure_is_allowed_next_to_validate() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    ctx->EnsureEntitySynced(entity);
    ValidateEntityAccess(entity);
}
"""
    )

    assert diagnostics == []


def test_exact_free_ensure_wrapper_is_an_allowed_terminal() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    EnsureEntitySynced(entity);
}
"""
    )

    assert diagnostics == []


def test_exact_validate_implementation_is_an_allowed_terminal() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    ValidateEntityAccess(entity);
}
"""
    )

    assert diagnostics == []


def test_overload_with_different_arity_does_not_contaminate_call_graph() -> None:
    diagnostics = analyze(
        """
void MoveEntity(ptr<ServerEntity> entity, ptr<Critter> target)
{
    FO_TRACE_ZONE(Threading);

    EnsureEntitySynced(entity);
}

void MoveEntity(ptr<ServerEntity> entity, ptr<Map> target, mpos hex)
{
    FO_TRACE_ZONE(Threading);

    EnsureEntitySynced(entity);
    AddToMap(entity, target, hex);
}

void AddToMap(ptr<ServerEntity> entity, ptr<Map> target, mpos hex)
{
    FO_TRACE_ZONE(Threading);

    EnsureEntitySynced(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<ServerEntity> entity, ptr<Critter> target)
{
    MoveEntity(entity, target);
}
"""
    )

    assert diagnostics == []


def test_overload_default_argument_is_included_in_arity_resolution() -> None:
    diagnostics = analyze(
        """
void MoveEntity(ptr<ServerEntity> entity, ptr<Critter> target, bool send = true)
{
    FO_TRACE_ZONE(Threading);

    EnsureEntitySynced(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<ServerEntity> entity, ptr<Critter> target)
{
    MoveEntity(entity, target);
}
"""
    )

    assert diagnostics == []


def test_unrelated_ensure_named_method_exposes_its_sink() -> None:
    # An unrelated class method spelled EnsureEntitySynced is not the exact
    # terminal, so it is traversed and its hidden cover replacement is caught
    diagnostics = analyze(
        """
void ForeignSync::EnsureEntitySynced(ptr<SyncContext> ctx, nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ForeignSync> foreign, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    foreign->EnsureEntitySynced(ctx, entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_qualified_unrelated_ensure_function_exposes_its_sink() -> None:
    diagnostics = analyze(
        """
void ForeignSync::EnsureEntitySynced(ptr<SyncContext> ctx, nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    ForeignSync::EnsureEntitySynced(ctx, entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_same_named_validate_helper_is_not_a_terminal() -> None:
    diagnostics = analyze(
        """
void ValidateEntityAccess(ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    ValidateEntityAccess(ctx, entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_fresh_registration_may_use_private_ensure_fresh() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/EntityManager.cpp": """
void EntityManager::CaptureFreshEntity(ptr<Item> item, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->EnsureFreshEntitySynced(item);
}

void EntityManager::RegisterItem(ptr<Item> item, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    CaptureFreshEntity(item, ctx);
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """

///@ ExportMethod
FO_SCRIPT_API void Server_Game_CreateItem(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<Item> freshItem, ptr<SyncContext> ctx)
{
    manager->RegisterItem(freshItem, ctx);
}
""",
        }
    )

    assert diagnostics == []


def test_trusted_fresh_caller_name_in_wrong_file_is_not_trusted() -> None:
    diagnostics = analyze(
        """
void EntityManager::CaptureFreshEntity(ptr<Item> item, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->EnsureFreshEntitySynced(item);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<Item> item, ptr<SyncContext> ctx)
{
    manager->CaptureFreshEntity(item, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::EnsureFreshEntitySynced"


def test_trusted_fresh_caller_does_not_allow_a_foreign_same_named_method() -> None:
    diagnostics = analyze(
        """
void ForeignSync::EnsureFreshEntitySynced(ptr<SyncContext> ctx, ptr<Item> item)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(item);
}

void EntityManager::CaptureFreshEntity(ptr<Item> item, ptr<ForeignSync> foreign, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    foreign->EnsureFreshEntitySynced(ctx, item);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<Item> item, ptr<ForeignSync> foreign, ptr<SyncContext> ctx)
{
    manager->CaptureFreshEntity(item, foreign, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_trusted_fresh_caller_requires_an_exact_get_context_factory() -> None:
    diagnostics = analyze(
        """
void ForeignSync::EnsureFreshEntitySynced(ptr<SyncContext> ctx, ptr<Item> item)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(item);
}

void EntityManager::CaptureFreshEntity(ptr<Item> item, ptr<ForeignSync> foreign, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    foreign->GetContext().EnsureFreshEntitySynced(ctx, item);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<Item> item, ptr<ForeignSync> foreign, ptr<SyncContext> ctx)
{
    manager->CaptureFreshEntity(item, foreign, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "EnsureFreshEntitySynced"


def test_fresh_capture_is_not_available_to_an_ordinary_export() -> None:
    diagnostics = analyze(
        """
void SyncContext::EnsureFreshEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    SyncEntity(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    ctx->EnsureFreshEntitySynced(entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::EnsureFreshEntitySynced"


def test_custom_registration_is_not_itself_a_trusted_fresh_capture() -> None:
    diagnostics = analyze(
        """
void SyncContext::EnsureFreshEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    SyncEntity(entity);
}

void EntityManager::RegisterCustomEntity(ptr<CustomEntity> entity, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->EnsureFreshEntitySynced(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<CustomEntity> entity, ptr<SyncContext> ctx)
{
    manager->RegisterCustomEntity(entity, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::EnsureFreshEntitySynced"


def test_registration_does_not_hide_cover_replacement() -> None:
    diagnostics = analyze(
        """
void EntityManager::RegisterItem(ptr<Item> item, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(item);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_CreateItem(ptr<ServerEngine> server, ptr<EntityManager> manager, ptr<Item> freshItem, ptr<SyncContext> ctx)
{
    manager->RegisterItem(freshItem, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"
    assert [function.name for function in diagnostics[0].path] == ["Server_Game_CreateItem", "EntityManager::RegisterItem"]


def test_only_explicit_sync_primitive_roots_are_exempt() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncWiden(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->WidenEntities({});
}

void SyncContext::WidenEntities(const_span<ptr<ServerEntity>> extras)
{
    SyncEntities(extras);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncYield(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->YieldLocks();
}

void SyncContext::YieldLocks()
{
    WidenEntities({});
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_SyncRelease(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->Release();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Lock(ptr<ServerEngine> server)
{
    server->Lock();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Unlock(ptr<ServerEngine> server)
{
    server->Unlock();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Ordinary(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntity(nullptr);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Server_Game_Ordinary"


def test_native_cover_probe_cannot_mask_a_missing_script_package() -> None:
    diagnostics = analyze(
        """
bool NativeHelper(ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    return ctx->ValidateAccess(entity);
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    return NativeHelper(ctx, entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::ValidateAccess"


def test_is_entity_locked_allows_only_the_exact_non_diagnosing_probe() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    return IsEntityAccessValid(entity, false);
}
"""
    )

    assert diagnostics == []


def test_is_entity_locked_does_not_hide_cover_replacement() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{
    const bool locked = IsEntityAccessValid(entity, false);
    ctx->SyncEntity(entity);
    return locked;
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Server_Game_IsEntityLocked"
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_is_entity_locked_rejects_a_diagnosing_probe() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    return IsEntityAccessValid(entity, true);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "IsEntityAccessValid"
    assert "exact non-diagnosing" in diagnostics[0].reason


def test_is_entity_locked_rejects_shadow_access_probe_in_wrong_file() -> None:
    diagnostics = analyze_files(
        {
            "SourceExt/ShadowProbe.cpp": """
auto IsEntityAccessValid(nptr<const ServerEntity> entity, bool diagnose) noexcept -> bool
{
    return true;
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API bool Server_Game_IsEntityLocked(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    return IsEntityAccessValid(entity, false);
}
""",
        }
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "IsEntityAccessValid"
    assert "exact non-diagnosing" in diagnostics[0].reason


def test_shadow_explicit_sync_export_in_wrong_file_is_not_exempt() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntity(nullptr);
}
""",
        relative="SourceExt/ShadowSync.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Server_Game_Sync"
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_scoped_sync_context_sync_is_a_header_sink() -> None:
    diagnostics = analyze(
        """
void NativeHelper(ptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext nested;
    nested.Sync(entity);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    NativeHelper(entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "ScopedSyncContext::Sync"


def test_scoped_sync_context_lifecycle_is_visible_without_sync_call() -> None:
    diagnostics = analyze(
        """
void NativeHelper()
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext nested;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server)
{
    NativeHelper();
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "ScopedSyncContext::Lifecycle"


def test_only_reviewed_scoped_sync_context_callers_are_allowed() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/MapManager.cpp": """
void MapManager::CreateLocation()
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext create_scope;
}
""",
            "Engine/Source/Server/Server.cpp": """
void ServerEngine::OnPlayerConnected(ptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext connection_scope;
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<MapManager> maps, ptr<ServerEntity> entity)
{
    maps->CreateLocation();
    server->OnPlayerConnected(entity);
}
""",
        }
    )

    assert diagnostics == []


def test_run_script_context_may_construct_only_an_empty_context() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/Server.cpp": """
void ServerEngine::RunScriptContext()
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext nested;
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server)
{
    server->RunScriptContext();
}
""",
        }
    )

    assert diagnostics == []


def test_run_script_context_does_not_hide_entity_acquisition() -> None:
    acquisitions = {
        "ScopedSyncContext::Sync": "nested.Sync(entity);",
        "SyncContext::SyncEntity": "nested.GetContext().SyncEntity(entity);",
        "SyncContext::SyncEntities": "nested.GetContext().SyncEntities({entity});",
    }
    for expected_sink, acquisition in acquisitions.items():
        diagnostics = analyze_files(
            {
                "Engine/Source/Server/Server.cpp": f"""
void ServerEngine::RunScriptContext(ptr<ServerEntity> entity)
{{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext nested;
    {acquisition}
}}
""",
                "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    server->RunScriptContext(entity);
}
""",
            }
        )

        assert len(diagnostics) == 1
        assert diagnostics[0].sink == expected_sink


def test_shadow_scoped_context_caller_in_wrong_file_is_not_allowed() -> None:
    diagnostics = analyze(
        """
void ServerEngine::RunScriptContext()
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext nested;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server)
{
    server->RunScriptContext();
}
""",
        relative="SourceExt/ShadowRuntime.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "ScopedSyncContext::Lifecycle"


def test_near_named_scoped_sync_context_caller_is_not_allowed() -> None:
    diagnostics = analyze(
        """
void MapManager::CreateLocationCompat()
{
    FO_TRACE_ZONE(Threading);

    ScopedSyncContext create_scope;
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<MapManager> maps)
{
    maps->CreateLocationCompat();
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "ScopedSyncContext::Lifecycle"


def test_sync_context_lifecycle_methods_are_forbidden_sinks() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Activate(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->Activate();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Deactivate(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->Deactivate();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_ReleaseCover(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->Release();
}
"""
    )

    assert len(diagnostics) == 3
    assert {diagnostic.sink for diagnostic in diagnostics} == {
            "SyncContext::Activate",
            "SyncContext::Deactivate",
            "SyncContext::Release",
        }


def test_direct_entity_lock_try_acquire_is_not_an_ensure_substitute() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<EntityLock> lock)
{
    lock->TryAcquire();
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "EntityLock::TryAcquire"


def test_validate_terminal_body_cannot_hide_a_blocking_call() -> None:
    # The cover-retention terminals are skipped by the main traversal, so the
    # dedicated non-blocking check must still catch a cover-replacing call
    # smuggled into a terminal body
    diagnostics = analyze_files(
        {
            "Engine/Source/Common/Entity.h": """
inline void ValidateEntityAccess(nptr<const Entity> entity)
{
    entity->ValidateAccess();
    SyncEntity(entity);
}
"""
        }
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncEntity"
    assert "cover-retention terminal" in diagnostics[0].reason


def test_cover_retention_terminal_bodies_must_stay_nonblocking() -> None:
    # Baseline: the canonical terminal bodies are non-blocking
    functions = collect_files({})
    assert audit.terminal_body_blocking_violations(functions) == []

    impl_cases = {
        "block_state_mutex": "FO_TRACE_ZONE(Threading);\nlock->LockStateMutex();",
        "release_caller_cover": "FO_TRACE_ZONE(Threading);\nRelease();",
        "replace_cover": "FO_TRACE_ZONE(Threading);\nSyncEntity(entity);",
        "sleep": "FO_TRACE_ZONE(Threading);\nstd::this_thread::sleep_for(delay);",
    }
    for name, body in impl_cases.items():
        changed = replace_function_body(
            functions,
            "SyncContext::EnsureEntitySyncedImpl",
            body,
        )
        diagnostics = audit.terminal_body_blocking_violations(changed)
        assert diagnostics, name
        assert all("cover-retention terminal" in diagnostic.reason for diagnostic in diagnostics), diagnostics

    # The exact implementation may perform only the reviewed bounded
    # back-off between failed all-or-nothing internal state-mutex batches
    changed = replace_function_body(
        functions,
        "SyncContext::EnsureEntitySyncedImpl",
        "FO_TRACE_ZONE(Threading);\nBackoffBeforeSyncRetry(0);",
    )
    assert audit.terminal_body_blocking_violations(changed) == []

    # The same helper name in any other terminal remains forbidden
    changed = replace_function_body(
        functions,
        "SyncContext::EnsureEntitySynced",
        "FO_TRACE_ZONE(Threading);\nBackoffBeforeSyncRetry(0);",
    )
    assert audit.terminal_body_blocking_violations(changed)


def test_free_ensure_wrapper_body_cannot_replace_cover() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/EntitySync.cpp": """
void EnsureEntitySynced(nptr<ServerEntity> entity)
{
    FO_TRACE_ZONE(Threading);

    auto ctx = SyncContext::GetCurrentOnThisThread();
    FO_VERIFY_AND_THROW(ctx, "Missing script execution context");
    ctx->SyncEntity(entity);
}
"""
        }
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncEntity"
    assert "cover-retention terminal" in diagnostics[0].reason


def test_typed_entity_release_does_not_alias_sync_context_release() -> None:
    diagnostics = analyze(
        """
void SyncContext::Release()
{
    FO_TRACE_ZONE(Threading);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<Player> player)
{
    player->Release();
}
"""
    )

    assert diagnostics == []


def test_expression_member_release_does_not_alias_entity_lock_release() -> None:
    diagnostics = analyze(
        """
void EntityLock::Release()
{
    FO_TRACE_ZONE(Threading);
}

void FreeValue(ptr<void> value)
{
    FO_TRACE_ZONE(Threading);

    cast_from_void<DynamicRefTypeInstance*>(value.get())->Release();
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<void> value)
{
    FreeValue(value);
}
"""
    )

    assert diagnostics == []


def test_chained_sync_context_receiver_is_not_missed() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    server->RequireCurrentSyncContext()->SyncEntity(entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_sync_context_raw_aliases_do_not_hide_cover_replacement() -> None:
    cases = {
        "get": ("auto raw = ctx.get();", "raw->SyncEntity(entity);", "SyncContext::SyncEntity"),
        "dereference": ("auto& raw = *ctx;", "raw.SyncEntities({entity});", "SyncContext::SyncEntities"),
        "copy": ("auto raw = ctx;", "raw->SyncEntity(entity);", "SyncContext::SyncEntity"),
        "decltype": ("decltype(ctx) raw = ctx;", "raw->SyncEntities({entity});", "SyncContext::SyncEntities"),
        "braced_copy": ("auto raw {ctx};", "raw->SyncEntity(entity);", "SyncContext::SyncEntity"),
        "braced_pointer": ("auto raw {ctx.get()};", "raw->SyncEntity(entity);", "SyncContext::SyncEntity"),
        "braced_decltype": ("decltype(ctx) raw {ctx};", "raw->SyncEntities({entity});", "SyncContext::SyncEntities"),
    }
    for name, (declaration, call, sink) in cases.items():
        diagnostics = analyze(
            f"""
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{{
    {declaration}
    {call}
}}
"""
        )

        assert len(diagnostics) == 1, diagnostics
        assert diagnostics[0].sink == sink


def test_braced_local_receiver_types_are_preserved() -> None:
    cases = {
        "context": ("auto local {SyncContext::GetCurrentOnThisThread()};", "SyncContext"),
        "lock": ("auto local {entity->GetEntityLock()};", "EntityLock"),
        "manager": ("auto& local {server->MapMngr};", "MapManager"),
        "cast": ("auto local {entity.dyn_cast<Entity>()};", "Entity"),
        "pointer": ("auto local {ctx.get()};", "SyncContext"),
        "alias": ("auto& local {*ctx};", "SyncContext"),
        "decltype": ("decltype(ctx) local {ctx};", "SyncContext"),
        "generic": ("auto local {GetGenericObjectAs<Entity>(gen)};", "Entity"),
        "controlled": ("auto local {player->GetControlledCritter()};", "Critter"),
    }
    for name, (declaration, expected_type) in cases.items():
        functions = collect_files({
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": f"""
FO_SCRIPT_API void Server_Game_TypeFixture(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{{
    {declaration}
}}
"""
        })
        function = next(f for f in functions if f.name == "Server_Game_TypeFixture")
        assert audit.collect_variable_types(function).get("local") == expected_type


def test_sync_context_raw_receiver_forms_do_not_hide_cover_replacement() -> None:
    cases = {
        "get": ("ctx.get()->SyncEntity(entity);", "SyncContext::SyncEntity"),
        "addressed_dereference": ("(&*ctx)->SyncEntities({entity});", "SyncContext::SyncEntities"),
        "dereference": ("(*ctx).SyncEntity(entity);", "SyncContext::SyncEntity"),
    }
    for name, (call, sink) in cases.items():
        diagnostics = analyze(
            f"""
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx, ptr<ServerEntity> entity)
{{
    {call}
}}
"""
        )

        assert len(diagnostics) == 1, diagnostics
        assert diagnostics[0].sink == sink


def test_temporary_scoped_sync_context_wrapper_is_not_missed() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ServerEntity> entity)
{
    ScopedSyncContext(server).Sync(entity);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "ScopedSyncContext::Sync"


def test_server_manager_field_chains_remain_in_the_call_graph() -> None:
    diagnostics = analyze(
        """
void MapManager::TransferHidden(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void CritterManager::DestroyHidden(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Critter_Bad(ptr<Critter> self, ptr<SyncContext> ctx)
{
    self->GetEngine()->MapMngr.TransferHidden(ctx);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_AlsoBad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    server->CrMngr.DestroyHidden(ctx);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_LocalManagerBad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    MapManager& map_mngr = server->MapMngr;
    map_mngr.TransferHidden(ctx);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_AutoManagerBad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    auto& cr_mngr = server->CrMngr;
    cr_mngr.DestroyHidden(ctx);
}
"""
    )

    assert len(diagnostics) == 4
    assert {diagnostic.root.name for diagnostic in diagnostics} == {
            "Server_Critter_Bad",
            "Server_Game_AlsoBad",
            "Server_Game_LocalManagerBad",
            "Server_Game_AutoManagerBad",
        }
    assert all(diagnostic.sink == "SyncContext::SyncEntity" for diagnostic in diagnostics)


def test_server_manager_overloads_are_resolved_by_argument_types() -> None:
    diagnostics = analyze(
        """
void ItemManager::MoveItemRetained(ptr<Item> item, ptr<Map> to_map, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ValidateEntityAccess(item);
    ValidateEntityAccess(to_map);
    ignore_unused(ctx);
}

void ItemManager::MoveItemRetained(ptr<Item> item, ptr<Item> to_cont, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ValidateEntityAccess(item);
    ValidateEntityAccess(to_cont);
    ctx->SyncEntity(item);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_MapGood(ptr<ServerEngine> server, ptr<Item> item, ptr<Map> toMap, ptr<SyncContext> ctx)
{
    server->ItemMngr.MoveItemRetained(item, toMap, ctx);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_ContainerBad(ptr<ServerEngine> server, ptr<Item> item, ptr<Item> toCont, ptr<SyncContext> ctx)
{
    server->ItemMngr.MoveItemRetained(item, toCont, ctx);
}
"""
    )

    assert len(diagnostics) == 1, diagnostics
    assert diagnostics[0].root.name == "Server_Game_ContainerBad"
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_unique_owner_untyped_members_remain_in_the_call_graph() -> None:
    diagnostics = analyze(
        """
void ServerEngine::SwitchPlayerCritter(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void EntityManager::DestroyHidden(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Player_Bad(ptr<Player> self, ptr<SyncContext> ctx)
{
    self->GetEngine()->SwitchPlayerCritter(ctx);
}

static void Game_DestroyOne(AngelScript::asIScriptGeneric* gen)
{
    FO_TRACE_ZONE(Threading);

    auto backend = GetScriptBackend(gen->GetEngine());
    auto entity_mngr = backend->GetEntityMngr();
    entity_mngr->DestroyHidden(GetContext(gen));
}

void RegisterBindings(ptr<AngelScript::asIScriptEngine> engine)
{
    engine->RegisterGlobalFunction("void Destroy(Entity)", FO_SCRIPT_GENERIC(Game_DestroyOne), FO_SCRIPT_GENERIC_CONV);
}
"""
    )

    assert len(diagnostics) == 2
    paths = {
        diagnostic.root.name: [function.name for function in diagnostic.path]
        for diagnostic in diagnostics
    }
    assert "ServerEngine::SwitchPlayerCritter" in paths["Server_Player_Bad"]
    assert "EntityManager::DestroyHidden" in paths["Game_DestroyOne"]


def test_auto_dyn_cast_and_try_hold_alias_preserve_receiver_type() -> None:
    diagnostics = analyze(
        """
void Map::RemoveItem(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void Critter::RemoveItem(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);
}

void RemoveFromHolder(ptr<Entity> holder, ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    auto map = holder.dyn_cast<Map>();
    auto map_holder = map.try_hold_ref();
    map_holder->RemoveItem(ctx);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<Entity> holder, ptr<SyncContext> ctx)
{
    RemoveFromHolder(holder, ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert [function.name for function in diagnostics[0].path] == ["Server_Game_Bad", "RemoveFromHolder", "Map::RemoveItem"]
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_continued_line_comment_does_not_create_a_sync_call() -> None:
    source = (
        "///@ ExportMethod\n"
        "FO_SCRIPT_API void Server_Game_Test(ptr<SyncContext> ctx)\n"
        "{\n"
        "    // The next physical line is still a comment \\\n"
        "    ctx->SyncEntity(nullptr);\n"
        "}\n"
    )
    assert analyze(source) == []

    active = source.replace("comment \\\n", "comment\n")
    diagnostics = analyze(active)
    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def analyze_spectator_schedule(expression: str, extra: str = "", preamble: str = "") -> list[audit.Diagnostic]:
    return analyze_files({
        "Engine/Source/Server/Map.cpp": f"""
{preamble}
void Map::ScheduleSpectatorRefresh(ptr<SyncContext> ctx)
{{
    {expression}
    {extra}
}}
""",
        "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Move(ptr<Map> map, ptr<SyncContext> ctx)
{
    map->ScheduleSpectatorRefresh(ctx);
}
""",
    })


def test_spectator_refresh_job_entry_does_not_replace_caller_cover() -> None:
    diagnostics = analyze_spectator_schedule(
        "_engine->ScheduleDelayedCallback(timespan::zero, [ctx]() mutable FO_DEFERRED { ctx->SyncEntity(nullptr); });")
    assert diagnostics == []


def test_spectator_synchronous_cover_replacement_is_still_reported() -> None:
    diagnostics = analyze_spectator_schedule(
        "_engine->ScheduleDelayedCallback(timespan::zero, [ctx]() mutable FO_DEFERRED { ignore_unused(ctx); });",
        "ctx->SyncEntities({});")
    assert any(d.sink == "SyncContext::SyncEntities" for d in diagnostics), diagnostics


def test_spectator_synchronous_scope_exit_stays_visible_beside_callback() -> None:
    diagnostics = analyze_spectator_schedule(
        "_engine->ScheduleDelayedCallback(timespan::zero, [ctx]() mutable FO_DEFERRED { ctx->SyncEntity(nullptr); });",
        "auto cleanup = scope_exit([ctx]() noexcept { ctx->SyncEntities({}); });")
    assert len(diagnostics) == 1, diagnostics
    assert diagnostics[0].sink == "SyncContext::SyncEntities"


def test_spectator_unreviewed_scheduler_callback_is_still_reported() -> None:
    diagnostics = analyze_spectator_schedule(
        "other->ScheduleDelayedCallback(timespan::zero, [ctx]() mutable FO_DEFERRED { ctx->SyncEntity(nullptr); });")
    assert any(d.sink == "SyncContext::SyncEntity" for d in diagnostics), diagnostics


def test_spectator_shadowed_engine_cannot_claim_reviewed_scheduler() -> None:
    diagnostics = analyze_spectator_schedule(
        "auto _engine = other; "
        "_engine->ScheduleDelayedCallback(timespan::zero, [ctx]() mutable FO_DEFERRED { ctx->SyncEntity(nullptr); });")
    assert any(d.sink == "SyncContext::SyncEntity" for d in diagnostics), diagnostics


def test_spectator_lambda_capture_is_still_synchronous() -> None:
    diagnostics = analyze_spectator_schedule(
        "_engine->ScheduleDelayedCallback(timespan::zero, [value = CaptureBeforeSchedule(ctx)]() mutable FO_DEFERRED { ignore_unused(value); });",
        preamble="""
int CaptureBeforeSchedule(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);
    ctx->SyncEntity(nullptr);
    return 1;
}
""")
    assert any(d.sink == "SyncContext::SyncEntity" for d in diagnostics), diagnostics


def test_spectator_raw_literal_cannot_hide_synchronous_call() -> None:
    diagnostics = analyze_spectator_schedule(
        "auto text = R\"tag(_engine->ScheduleDelayedCallback({ fake }))tag\"; "
        "auto count = 1'000; ctx->SyncEntity(nullptr);")
    assert any(d.sink == "SyncContext::SyncEntity" for d in diagnostics), diagnostics


def test_submit_does_not_hide_a_script_reached_callback() -> None:
    diagnostics = analyze(
        """
void ForbiddenLater(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void Schedule(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    Submit([ctx]() { ForbiddenLater(ctx); });
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    Schedule(ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_reviewed_top_level_worker_entry_is_not_part_of_the_caller_context() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/Server.cpp": """
void TopLevelJob(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void ServerEngine::OnTimeEventSchedule(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    Submit([ctx]() { TopLevelJob(ctx); });
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    server->OnTimeEventSchedule(ctx);
}
""",
        }
    )

    assert diagnostics == []


def test_shadow_top_level_scheduler_in_wrong_file_does_not_hide_callback() -> None:
    diagnostics = analyze(
        """
void TopLevelJob(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void ServerEngine::OnTimeEventSchedule(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    Submit([ctx]() { TopLevelJob(ctx); });
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    server->OnTimeEventSchedule(ctx);
}
""",
        relative="SourceExt/ShadowScheduler.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_unreviewed_run_thread_callback_remains_in_call_graph() -> None:
    diagnostics = analyze(
        """
void ForbiddenWorker(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    run_thread("worker", [ctx] { ForbiddenWorker(ctx); });
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_synchronous_lambda_body_remains_in_call_graph() -> None:
    diagnostics = analyze(
        """
void ForbiddenNow(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    scope_exit cleanup([ctx]() { ForbiddenNow(ctx); });
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_run_now_lambda_remains_in_call_graph() -> None:
    diagnostics = analyze(
        """
void ForbiddenNow(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    RunNow([ctx]() { ForbiddenNow(ctx); });
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_markerless_source_extension_export_is_a_root() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Ext(ptr<ServerEngine> server, ptr<SyncContext> ctx);

void NativeHelper(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

void FO_NAMESPACE Server_Game_Ext(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    NativeHelper(ctx);
}
""",
        relative="SourceExt/Fixture.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Server_Game_Ext"
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_registered_generic_callback_is_a_root() -> None:
    diagnostics = analyze(
        """
void NativeHelper(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

static void Game_GenericBad(AngelScript::asIScriptGeneric* gen)
{
    FO_TRACE_ZONE(Threading);

    NativeHelper(GetContext(gen));
}

void RegisterBindings(ptr<AngelScript::asIScriptEngine> engine)
{
    FO_TRACE_ZONE(Threading);

    engine->RegisterObjectMethod("GameSingleton", "void Bad()", FO_SCRIPT_GENERIC(Game_GenericBad), FO_SCRIPT_GENERIC_CONV);
}
""",
        relative="Engine/Source/Scripting/AngelScript/Fixture.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.name == "Game_GenericBad"
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_property_lock_helper_is_outside_entity_cover_contract() -> None:
    diagnostics = analyze_files(
        {
            "Engine/Source/Server/Server.cpp": """
void ServerEngine::LockForPropertyAccess()
{
    FO_TRACE_ZONE(Threading);

    _entityLock->Acquire(1);
}
""",
            "Engine/Source/Scripting/ServerGlobalScriptMethods.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server)
{
    server->LockForPropertyAccess();
}
""",
        }
    )

    assert diagnostics == []


def test_entity_property_lock_terminal_uses_the_exact_base_type() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Good(ptr<ServerEngine> server, ptr<Entity> entity)
{
    entity->LockForPropertyAccessShared();
    entity->UnlockForPropertyAccessShared();
}
"""
    )

    assert diagnostics == []


def test_foreign_property_lock_method_does_not_inherit_the_exemption() -> None:
    diagnostics = analyze(
        """
void ForeignSync::LockForPropertyAccess(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<ForeignSync> foreign, ptr<SyncContext> ctx)
{
    foreign->LockForPropertyAccess(ctx);
}
"""
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


def test_shadow_property_lock_method_in_wrong_file_is_not_exempt() -> None:
    diagnostics = analyze(
        """
void ServerEngine::LockForPropertyAccess(ptr<SyncContext> ctx)
{
    FO_TRACE_ZONE(Threading);

    ctx->SyncEntity(nullptr);
}

///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    server->LockForPropertyAccess(ctx);
}
""",
        relative="SourceExt/ShadowPropertyLock.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntity"


WORKER_POOL_SOURCE = """
static auto GetPoolServer(ptr<WorkerSession> session) -> ptr<ServerEngine>
{
    FO_TRACE_ZONE(Threading);

    return session->Server;
}

static void RunPoolWorker(ptr<WorkerSession> session)
{
    FO_TRACE_ZONE(Threading);

    auto server = GetPoolServer(session);
    server->Lock();
}

///@ ExportMethod
FO_SCRIPT_API bool Server_Game_StartWorkerPool(ptr<ServerEngine> self, ptr<WorkerSession> session)
{
    run_thread("worker", [session] { RunPoolWorker(session); });
    return true;
}
"""


POOL_FACTORY = {"name": "GetPoolServer", "type": "ServerEngine", "reason": "Returns the server that owns a worker session"}
POOL_SCHEDULER = {
    "file": "SourceExt/WorkerPool.cpp",
    "function": "Server_Game_StartWorkerPool",
    "reason": "Each pool worker is a fresh top-level job that acquires its own cover",
}


def test_project_typed_factory_exposes_its_receiver() -> None:
    source = WORKER_POOL_SOURCE.replace('run_thread("worker", [session] { RunPoolWorker(session); });', "RunPoolWorker(session);")

    unconfigured = analyze(source, relative="SourceExt/WorkerPool.cpp")
    configured = analyze_files(
        {"SourceExt/WorkerPool.cpp": source},
        {"schema_version": 1, "sync_scope": {"typed_factories": [POOL_FACTORY]}},
    )

    # Without the reviewed factory the receiver type is unknown and the sink stays invisible
    assert unconfigured == []
    assert len(configured) == 1
    assert configured[0].sink == "ServerEngine::Lock"
    assert configured[0].root.name == "Server_Game_StartWorkerPool"


def test_project_independent_job_scheduler_is_configuration() -> None:
    factory_only = analyze_files(
        {"SourceExt/WorkerPool.cpp": WORKER_POOL_SOURCE},
        {"schema_version": 1, "sync_scope": {"typed_factories": [POOL_FACTORY]}},
    )
    reviewed = analyze_files(
        {"SourceExt/WorkerPool.cpp": WORKER_POOL_SOURCE},
        {"schema_version": 1, "sync_scope": {"typed_factories": [POOL_FACTORY], "independent_job_schedulers": [POOL_SCHEDULER]}},
    )

    assert len(factory_only) == 1
    assert factory_only[0].sink == "ServerEngine::Lock"
    assert reviewed == []


def test_project_scheduler_identity_does_not_cover_a_same_named_function_elsewhere() -> None:
    diagnostics = analyze_files(
        {"SourceExt/Other/WorkerPool.cpp": WORKER_POOL_SOURCE, "SourceExt/WorkerPool.cpp": "static void Unused()\n{\n}\n"},
        {"schema_version": 1, "sync_scope": {"typed_factories": [POOL_FACTORY], "independent_job_schedulers": [POOL_SCHEDULER]}},
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].root.path.as_posix() == "SourceExt/Other/WorkerPool.cpp"


def test_project_deferred_callback_api_is_configuration() -> None:
    source = WORKER_POOL_SOURCE.replace('run_thread("worker", ', "pool->Post(")
    base_config = {"typed_factories": [POOL_FACTORY], "independent_job_schedulers": [POOL_SCHEDULER]}

    unknown_api = analyze_files({"SourceExt/WorkerPool.cpp": source}, {"schema_version": 1, "sync_scope": base_config})
    reviewed_api = analyze_files(
        {"SourceExt/WorkerPool.cpp": source},
        {"schema_version": 1, "sync_scope": {**base_config, "deferred_callback_calls": [{"name": "Post", "reason": "Pool queue entry"}]}},
    )

    assert len(unknown_api) == 1
    assert reviewed_api == []


def test_engine_identity_is_not_matched_by_a_project_file_with_the_same_relative_path() -> None:
    diagnostics = analyze(
        """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Sync(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntities({});
}
""",
        relative="SourceExt/Source/Scripting/ServerGlobalScriptMethods.cpp",
    )

    assert len(diagnostics) == 1
    assert diagnostics[0].sink == "SyncContext::SyncEntities"


def test_excluded_vendored_directory_is_not_audited(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    write_tree(root, {
        "Engine/Source/Server/EntitySync.cpp": "void Placeholder()\n{\n}\n",
        "SourceExt/Vendor/Bad.cpp": """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntities({});
}
""",
    })
    config = {"schema_version": 1, "exclude": [{"path": "SourceExt/Vendor", "reason": "Vendored library"}]}

    included = audit.analyze(collect_context(fixture_context(root)))
    excluded = audit.analyze(collect_context(fixture_context(root, config)))

    assert len(included) == 1
    assert excluded == []


def test_stale_configuration_entries_fail(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    write_tree(root, {"Engine/Source/Server/EntitySync.cpp": "void Placeholder()\n{\n}\n", "SourceExt/Native.cpp": "void Native()\n{\n}\n"})
    config = {
        "schema_version": 1,
        "exclude": [{"path": "SourceExt/Gone", "reason": "Removed vendored library"}],
        "sync_scope": {
            "independent_job_schedulers": [POOL_SCHEDULER],
            "typed_factories": [POOL_FACTORY],
            "deferred_callback_calls": [{"name": "Post", "reason": "Pool queue entry"}],
        },
    }
    context = fixture_context(root, config)

    sources, stale = audit.source_files(context)
    errors = stale + audit.config_staleness(context, audit.collect_functions(sources))

    assert len(errors) == 4
    assert any("SourceExt/Gone" in error for error in errors)
    assert any("Server_Game_StartWorkerPool" in error for error in errors)
    assert any("GetPoolServer" in error for error in errors)
    assert any("Post" in error for error in errors)


@pytest.mark.parametrize(
    ("config", "message"),
    [
        ({"schema_version": 2}, "schema_version"),
        ({"schema_version": 1, "unknown": []}, "unknown key"),
        ({"schema_version": 1, "exclude": [{"path": "SourceExt/Vendor"}]}, "reason"),
        ({"schema_version": 1, "exclude": [{"path": "../Outside", "reason": "x"}]}, "root-relative"),
        ({"schema_version": 1, "sync_scope": {"typed_factories": [{"name": "Get Server", "type": "ServerEngine", "reason": "x"}]}}, "identifier"),
        ({"schema_version": 1, "sync_scope": {"independent_job_schedulers": [{"file": "SourceExt/*.cpp", "function": "F", "reason": "x"}]}}, "wildcards"),
        ({"schema_version": 1, "sync_scope": {"deferred_callback_calls": [{"name": "Post", "reason": "x"}, {"name": "Post", "reason": "y"}]}}, "duplicate"),
    ],
)
def test_invalid_configuration_is_rejected(tmp_path: Path, config: dict[str, object], message: str) -> None:
    root = tmp_path.resolve()
    write_tree(root, {"Engine/Source/Server/EntitySync.cpp": "void Placeholder()\n{\n}\n"})

    with pytest.raises(scope.ConfigError, match=message):
        fixture_context(root, config)


def test_scope_must_not_overlap_the_engine(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    write_tree(root, {"Engine/Source/Server/EntitySync.cpp": "void Placeholder()\n{\n}\n"})

    write_tree(root, {"Engine/Examples/Game/SourceExt/Native.cpp": "void Native()\n{\n}\n"})

    with pytest.raises(scope.ConfigError, match="overlaps"):
        scope.create_context(root, root / "Engine", ["Engine/Source"], None)

    with pytest.raises(scope.ConfigError, match="overlaps"):
        scope.create_context(root, root / "Engine", ["Engine"], None)

    # A sample project inside the Engine checkout is still a separate project scope
    assert scope.create_context(root, root / "Engine", ["Engine/Examples/Game/SourceExt"], None).scopes == ("Engine/Examples/Game/SourceExt",)

    with pytest.raises(scope.ConfigError, match="does not exist"):
        scope.create_context(root, root / "Engine", ["Missing"], None)


BAD_PROJECT_EXPORT = """
///@ ExportMethod
FO_SCRIPT_API void Server_Game_Bad(ptr<ServerEngine> server, ptr<SyncContext> ctx)
{
    ctx->SyncEntities({});
}
"""


def baseline_tree(root: Path) -> None:
    write_tree(root, {
        "Engine/Source/Server/EntitySync.cpp": "void Placeholder()\n{\n}\n",
        "Engine/BuildTools/audit_baselines/sync-scope.tsv": "file\texport\tsink\treason\n",
        "SourceExt/Bad.cpp": BAD_PROJECT_EXPORT,
    })


def run_cli(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CLI), "sync-scope", "--root", str(root), "--engine-root", str(root / "Engine"), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def test_cli_reports_a_project_violation_and_accepts_a_reasoned_baseline_row(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    baseline_tree(root)
    (root / "project-sync.tsv").write_text(
        "file\texport\tsink\treason\nSourceExt/Bad.cpp\tServer_Game_Bad\tSyncContext::SyncEntities\tReviewed legacy path\n",
        encoding="utf-8",
    )

    failing = run_cli(root, "--scope", "SourceExt")
    passing = run_cli(root, "--scope", "SourceExt", "--baseline", "project-sync.tsv")

    assert failing.returncode == 1
    assert "SourceExt/Bad.cpp:3: error: Server_Game_Bad reaches blocking entity-cover replacement" in failing.stdout
    assert passing.returncode == 0, passing.stdout
    assert "1 baselined" in passing.stdout


@pytest.mark.parametrize(
    ("row", "message"),
    [
        ("SourceExt/Bad.cpp\tServer_Game_Bad\tSyncContext::SyncEntities\t", "has no reason"),
        ("SourceExt/Bad.cpp\tServer_Game_Bad\tSyncContext::SyncEntity\tStale sink", "stale baseline row"),
        ("SourceExt/Bad.cpp\tServer_Game_Bad\tSyncContext::SyncEntities", "expected 4 columns"),
    ],
)
def test_cli_rejects_unexplained_stale_and_malformed_rows(tmp_path: Path, row: str, message: str) -> None:
    root = tmp_path.resolve()
    baseline_tree(root)
    (root / "project-sync.tsv").write_text(f"file\texport\tsink\treason\n{row}\n", encoding="utf-8")

    result = run_cli(root, "--scope", "SourceExt", "--baseline", "project-sync.tsv")

    assert result.returncode == 1
    assert message in result.stdout


def test_cli_engine_baseline_row_does_not_cover_a_project_finding(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    baseline_tree(root)
    (root / "Engine/BuildTools/audit_baselines/sync-scope.tsv").write_text(
        "file\texport\tsink\treason\nSourceExt/Bad.cpp\tServer_Game_Bad\tSyncContext::SyncEntities\tWrong owner\n",
        encoding="utf-8",
    )

    result = run_cli(root, "--scope", "SourceExt")

    assert result.returncode == 1
    assert "Server_Game_Bad reaches" in result.stdout
    assert "stale baseline row" in result.stdout


def test_cli_json_summary_and_usage_errors(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    baseline_tree(root)

    result = run_cli(root, "--scope", "SourceExt", "--json")
    usage = run_cli(root, "--scope", "Missing")

    summary = json.loads(result.stdout)["summary"]
    assert result.returncode == 1
    assert summary["violations"] == 1
    assert summary["script_native_roots"] == 1
    assert usage.returncode == 2
    assert "does not exist" in usage.stderr


def test_engine_rule_identities_exist_in_the_engine_sources() -> None:
    context = scope.create_context(ENGINE_ROOT, ENGINE_ROOT, [], None)
    functions = collect_context(context)
    identities = {audit.function_identity(function) for function in functions}
    rules = (
        audit.EXPLICIT_SYNC_EXPORT_IDENTITIES
        | {audit.IS_ENTITY_LOCKED_EXPORT_IDENTITY, audit.ENTITY_ACCESS_PROBE_IDENTITY, audit.FRESH_ENSURE_TERMINAL_IDENTITY}
        | audit.ALLOWED_TERMINAL_IDENTITIES
        | audit.TRUSTED_FRESH_CAPTURE_CALLER_IDENTITIES
        | audit.PROPERTY_LOCK_METHOD_TERMINAL_IDENTITIES
        | audit.SCOPED_SYNC_CONTEXT_ALLOWED_CALLER_IDENTITIES
        | audit.ENGINE_INDEPENDENT_TOP_LEVEL_JOB_SCHEDULER_IDENTITIES
        | {audit.SPECTATOR_REFRESH_SCHEDULER_IDENTITY}
        | audit.NONBLOCKING_TERMINAL_IDENTITIES
    )

    # Class-body definitions in headers are named without their class
    missing = sorted(
        rule
        for rule in rules
        if rule not in identities and not (rule[1].endswith(".h") and (rule[0], rule[1], rule[2].rsplit("::", 1)[-1]) in identities)
    )

    # A renamed Engine function must update the rule instead of silently losing its exemption or check
    assert missing == []
    assert audit.analyze(functions) == []

    for name in audit.ENGINE_DEFERRED_CALLBACK_CALLS:
        assert any(f"{name}(" in function.body for function in functions), name


def test_real_custom_entity_binds_final_lock_before_publication() -> None:
    collected = collect_context(scope.create_context(ENGINE_ROOT, ENGINE_ROOT, [], None))
    assert audit.terminal_body_blocking_violations(collected) == []
    functions = {function.name: function for function in collected}
    fresh = functions["SyncContext::EnsureFreshEntitySynced"]
    create_inner = functions["EntityManager::CreateCustomInnerEntity"]
    load_inner = functions["EntityManager::LoadInnerEntitiesEntry"]
    register = functions["EntityManager::RegisterCustomEntity"]

    fresh_impl = fresh.body.find("EnsureEntitySyncedImpl")
    assert fresh_impl > 0
    for required_guard in (
        "IsDestroying",
        "IsDestroyed",
        "GetParentRaw",
    ):
        assert fresh.body.find(required_guard) >= 0
        assert fresh.body.find(required_guard) < fresh_impl

    # Parent link and nearest-holder lock are bound by one shared helper, in that order
    attach = functions["EntityManager::AttachCustomEntityToHolder"]
    attach_parent = attach.body.find("entity->SetParent(holder_entity)")
    attach_lock = attach.body.find("entity->SetEntityLock(holder_lock)")
    assert 0 <= attach_parent < attach_lock
    assert "RegisterCustomEntity" not in attach.body

    create_allocate = create_inner.body.find("ConstructCustomEntity")
    create_attach = create_inner.body.find("AttachCustomEntityToHolder")
    create_publish = create_inner.body.find("RegisterCustomEntity")
    assert 0 <= create_allocate < create_attach < create_publish
    assert "CaptureFreshEntity" not in create_inner.body

    # Восстановление из документа одно на пакетное чтение записи держателя и на загрузку по одному id;
    # порядок «родитель и замок до публикации» живёт в нём, а не в загрузчиках
    restore = functions["EntityManager::RestoreCustomEntity"]
    load_attach = restore.body.find("AttachCustomEntityToHolder")
    load_publish = restore.body.find("RegisterCustomEntity")
    assert 0 <= load_attach < load_publish
    load_batch = functions["EntityManager::LoadCustomEntities"]
    load_custom = functions["EntityManager::LoadCustomEntity"]
    assert "RestoreCustomEntity" in load_batch.body
    assert "RegisterCustomEntity" not in load_batch.body
    assert "LoadCustomEntities" in load_inner.body
    assert "LoadCustomEntities" in load_custom.body
    assert "RegisterCustomEntity" not in load_custom.body
    assert "SetEntityLock" not in register.body
    assert "EnsureFreshEntitySynced" not in register.body
    assert "GetParentRaw" in register.body
    assert "GetEntityLock" in register.body
    assert "IsLockedByCurrentThread" in register.body
    assert "ValidateEntityAccess" in register.body
