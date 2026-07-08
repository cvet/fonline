//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ServerEntity.h"
#include "Server.h"

FO_BEGIN_NAMESPACE

ServerEntity::ServerEntity(ptr<ServerEngine> engine, ident_t id, ptr<const PropertyRegistrator> registrator, nptr<const Properties> props, nptr<const Properties> base_props) noexcept :
    Entity(registrator, props, engine->Settings->ServerPropertiesPackData ? base_props : nullptr),
    _engine {engine},
    _id {id}
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
}

ServerEntity::~ServerEntity()
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    // Release any leftover parent ref. Destroy sites are expected to call SetParent(nullptr)
    // explicitly before MarkAsDestroyed (so the containment cycle breaks before refcount drop),
    // but if something slipped through, this destructor still releases cleanly.
    if (nptr<ServerEntity> nullable_parent = _parent.load(std::memory_order_relaxed); nullable_parent) {
        auto parent = nullable_parent.as_ptr();
        parent->Release();
    }
}

void ServerEntity::SetInitCalled() noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _initCalled = true;
}

void ServerEntity::SetEntityLock(nptr<EntityLock> lock) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    _entityLock = lock;
}

auto ServerEntity::GetId() const noexcept -> ident_t
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _id;
}

auto ServerEntity::GetEngine() const noexcept -> ptr<const ServerEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _engine;
}

auto ServerEntity::GetEngine() noexcept -> ptr<ServerEngine>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _engine;
}

auto ServerEntity::IsInitCalled() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return _initCalled;
}

auto ServerEntity::IsPersistent() const noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _isPersistent;
}

auto ServerEntity::GetEntityLock() const noexcept -> nptr<EntityLock>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _entityLock;
}

auto ServerEntity::GetSyncWidenEntity() noexcept -> nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nullptr;
}

auto ServerEntity::GetSyncWidenEntity() const noexcept -> nptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nullptr;
}

void ServerEntity::SetId(ident_t id) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    _id = id;
}

void ServerEntity::SetPersistent(bool persistent) noexcept
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    _isPersistent = persistent;
}

auto ServerEntity::IsExplicitlyPersistent() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    auto& props = const_cast<Properties&>(*GetProperties());
    return EntityProperties(props).GetExplicitlyPersistent();
}

void ServerEntity::SetExplicitlyPersistent(bool explicitly_persistent)
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED, NOT_DESTROYING);
    EntityProperties(*GetPropertiesForEdit()).SetExplicitlyPersistent(explicitly_persistent);
}

void ServerEntity::ValidateAccess() const
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (!IsEntityAccessValid(this)) {
        throw ScriptException("Entity access without sync", GetName());
    }
}

auto ServerEntity::GetParent() -> refcount_nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return nptr<ServerEntity>(_parent.load(std::memory_order_acquire)).try_hold_ref();
}

auto ServerEntity::GetParent() const -> refcount_nptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
    return nptr<const ServerEntity>(_parent.load(std::memory_order_acquire)).try_hold_ref();
}

auto ServerEntity::GetParentRaw() const noexcept -> refcount_nptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return nptr<ServerEntity>(_parent.load(std::memory_order_acquire)).try_hold_ref();
}

void ServerEntity::SetParent(nptr<ServerEntity> parent) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);

    if (_parent.load(std::memory_order_relaxed) != nullptr) {
        nptr<const SyncContext> ctx = SyncContext::GetCurrentOnThisThread();
        nptr<const EntityLock> lock = GetEntityLock();
        // Strict access model: reparenting a live entity requires its OWN lock held directly (ancestor
        // coverage is a read-path right only; there is no empty-context free pass). The only exemption
        // is a thread with no sync context at all (non-server threads). Stop-the-world owners
        // (ServerEngine::Lock / Shutdown) satisfy this naturally: their reparents run through the
        // capture paths (EnsureEntitySynced), which take the entity's own lock.
        FO_VERIFY_AND_CONTINUE(!ctx || (lock && lock->IsLockedByCurrentThread()), "Reparent of a live entity without holding its own lock", GetName(), GetId());
    }

    if (parent) {
        parent->AddRef();
    }

    nptr<ServerEntity> old = _parent.exchange(parent.get(), std::memory_order_acq_rel);

    if (old) {
        old->Release();
    }
}

auto ServerEntity::FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult
{
    FO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);

    if (callbacks.empty()) {
        return EventResult::ContinueChain;
    }

    // Engine-wide invariant: a primary SyncContext is always active when an event fires.
    FO_STRONG_ASSERT(SyncContext::GetCurrentOnThisThread(), "Server entity event fired without active sync context");

    bool had_exception = false;

    // Iterate a copy — callbacks vector may be changed/invalidated during cycle work.
    for (const auto& cb : copy(callbacks)) {
        EventResult result = EventResult::ContinueChain;

        try {
            result = cb.Callback(call);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
            had_exception = true;

            if (cb.HasExplicitResult) {
                return EventResult::StopChain;
            }
        }

        if (result == EventResult::StopChain) {
            return EventResult::StopChain;
        }
    }

    return had_exception ? EventResult::StopChain : EventResult::ContinueChain;
}

auto CustomEntity::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _propsRef->GetRegistrator()->GetTypeName();
}

auto CustomEntityWithProto::GetName() const noexcept -> string_view
{
    FO_NO_STACK_TRACE_ENTRY();

    FO_VALIDATE_ENTITY(NONE);
    return _proto->GetName();
}

FO_END_NAMESPACE
