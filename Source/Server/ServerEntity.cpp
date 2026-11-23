// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ServerEntity.h"

#include "Server.h"

FO_BEGIN_NAMESPACE

ServerEntity::ServerEntity(ptr<ServerEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props) noexcept :
    Entity(registrar, props, engine->Settings->Server.ServerPropertiesPackData ? base_props : nullptr),
    _engine {engine},
    _id {id}
{
    FO_VALIDATE_ENTITY(NONE);

    // The engine is borrowed, not owned: it owns the property registrars, protos, hashes and managers this
    // entity reads, so no entity may outlive it. This count is what proves that in ~ServerEngine
    _engine->_liveEntityCount.fetch_add(1, std::memory_order_relaxed);
}

ServerEntity::~ServerEntity()
{
    FO_VALIDATE_ENTITY(NONE);

    // Release any leftover parent ref. Destroy sites are expected to call SetParent(nullptr)
    // explicitly before MarkAsDestroyed (so the containment cycle breaks before refcount drop),
    // but if something slipped through, this destructor still releases cleanly
    nptr<ServerEntity> parent {};

    {
        scoped_lock locker {_parentLinkLocker};

        parent = nptr<ServerEntity> {_parent.exchange(nullptr, std::memory_order_acq_rel)};
    }

    if (parent) {
        parent->Release();
    }

    _engine->_liveEntityCount.fetch_sub(1, std::memory_order_acq_rel);
}

void ServerEntity::SetInitCalled() noexcept
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _initCalled = true;
}

void ServerEntity::SetEntityLock(nptr<EntityLock> lock) noexcept
{
    FO_VALIDATE_ENTITY(NONE);
    _entityLock = lock;
}

auto ServerEntity::GetId() const noexcept -> ident_t
{
    FO_VALIDATE_ENTITY(NONE);
    return _id;
}

auto ServerEntity::GetEngine() const noexcept -> ptr<const ServerEngine>
{
    FO_VALIDATE_ENTITY(NONE);
    return _engine;
}

auto ServerEntity::GetEngine() noexcept -> ptr<ServerEngine>
{
    FO_VALIDATE_ENTITY(NONE);
    return _engine;
}

auto ServerEntity::IsInitCalled() const noexcept -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _initCalled;
}

auto ServerEntity::IsPersistent() const noexcept -> bool
{
    FO_VALIDATE_ENTITY(NONE);
    return _isPersistent;
}

auto ServerEntity::GetEntityLock() const noexcept -> nptr<EntityLock>
{
    FO_VALIDATE_ENTITY(NONE);
    return _entityLock;
}

auto ServerEntity::GetSyncWidenEntity() noexcept -> refcount_nptr<ServerEntity>
{
    FO_VALIDATE_ENTITY(NONE);
    return nullptr;
}

auto ServerEntity::GetSyncWidenEntity() const noexcept -> refcount_nptr<const ServerEntity>
{
    FO_VALIDATE_ENTITY(NONE);
    return nullptr;
}

void ServerEntity::SetId(ident_t id) noexcept
{
    FO_VALIDATE_ENTITY(NONE);
    _id = id;
}

void ServerEntity::SetPersistent(bool persistent) noexcept
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _isPersistent = persistent;
}

auto ServerEntity::IsExplicitlyPersistent() const noexcept -> bool
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    auto& props {const_cast<Properties&>(*GetProperties())};
    return EntityProperties(props).GetExplicitlyPersistent();
}

void ServerEntity::SetExplicitlyPersistent(bool explicitly_persistent)
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    EntityProperties(*GetPropertiesForEdit()).SetExplicitlyPersistent(explicitly_persistent);
}

void ServerEntity::ValidateAccess() const
{
    FO_VALIDATE_ENTITY(NONE);

    if (!IsEntityAccessValid(this)) {
        throw ScriptException("Entity access without sync", GetName());
    }
}

auto ServerEntity::GetParent() -> refcount_nptr<ServerEntity>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return GetParentRaw();
}

auto ServerEntity::GetParent() const -> refcount_nptr<const ServerEntity>
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return GetParentRaw();
}

auto ServerEntity::GetParentRaw() const noexcept -> refcount_nptr<ServerEntity>
{
    FO_VALIDATE_ENTITY(NONE);

    // Load and pin under one hold: SetParent replaces the pointer under the same lock and only then drops the
    // link's ref, so a parent reachable here still carries that ref and TryAddRef cannot resurrect a dead object
    scoped_lock locker {_parentLinkLocker};

    return nptr<ServerEntity>(_parent.load(std::memory_order_acquire)).try_hold_ref();
}

void ServerEntity::SetParent(nptr<ServerEntity> parent) noexcept
{
    FO_VALIDATE_ENTITY(NONE);

    if (_parent.load(std::memory_order_relaxed) != nullptr) {
        auto ctx {SyncContext::GetCurrentOnThisThread()};
        auto lock {GetEntityLock()};
        // Reparenting needs the entity's own lock directly, because ancestor coverage is a read-path right only;
        // the exemptions are single-threaded logic, which takes no locks at all, and a thread with no sync context
        FO_VERIFY_AND_CONTINUE(IsSingleThreadedLogic(this) || !ctx || (lock && lock->IsLockedByCurrentThread()), "Reparent of a live entity without holding its own lock", GetName(), GetId());
    }

    if (parent) {
        parent->AddRef();
    }

    // The replacement is published under the link lock and the old ref dropped after it, so no reader can be
    // holding the old pointer without having already pinned it
    nptr<ServerEntity> old {};

    {
        scoped_lock locker {_parentLinkLocker};

        old = nptr<ServerEntity> {_parent.exchange(parent.get(), std::memory_order_acq_rel)};
    }

    if (old) {
        old->Release();
    }
}

auto ServerEntity::FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult
{
    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (callbacks.empty()) {
        return EventResult::ContinueChain;
    }

    // Engine-wide invariant: a primary SyncContext is always active when an event fires
    FO_STRONG_ASSERT(SyncContext::GetCurrentOnThisThread(), "Server entity event fired without active sync context");

    return Entity::FireEvent(callbacks, call);
}

auto CustomEntity::GetName() const noexcept -> string_view
{
    FO_VALIDATE_ENTITY(NONE);
    return _propsRef->GetRegistrar()->GetTypeName();
}

auto CustomEntityWithProto::GetName() const noexcept -> string_view
{
    FO_VALIDATE_ENTITY(NONE);
    return _proto->GetName();
}

FO_END_NAMESPACE
