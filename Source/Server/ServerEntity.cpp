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

ServerEntity::ServerEntity(ServerEngine* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props, const Properties* base_props) noexcept :
    Entity(registrator, props, engine->Settings.ServerPropertiesPackData ? base_props : nullptr),
    _engine {engine},
    _id {id}
{
    FO_STACK_TRACE_ENTRY();
}

ServerEntity::~ServerEntity()
{
    FO_NO_STACK_TRACE_ENTRY();

    // Release any leftover parent ref. Destroy sites are expected to call SetParent(nullptr)
    // explicitly before MarkAsDestroyed (so the containment cycle breaks before refcount drop),
    // but if something slipped through, this destructor still releases cleanly.
    if (auto* p = _parent.load(std::memory_order_relaxed); p != nullptr) {
        p->Release();
    }
}

void ServerEntity::SetId(ident_t id) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _id = id;
}

void ServerEntity::SetPersistent(bool persistent) noexcept
{
    FO_STACK_TRACE_ENTRY();

    _isPersistent = persistent;
}

auto ServerEntity::IsExplicitlyPersistent() const noexcept -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto& props = const_cast<Properties&>(GetProperties());
    return EntityProperties(props).GetExplicitlyPersistent();
}

void ServerEntity::SetExplicitlyPersistent(bool explicitly_persistent)
{
    FO_STACK_TRACE_ENTRY();

    EntityProperties(GetPropertiesForEdit()).SetExplicitlyPersistent(explicitly_persistent);
}

void ServerEntity::ValidateAccess() const
{
    FO_NO_STACK_TRACE_ENTRY();

    if (!IsEntityAccessValid(this)) {
        throw ScriptException("Entity access without sync", GetName());
    }
}

auto ServerEntity::GetParent() -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(this);
    return refcount_ptr<ServerEntity>(_parent.load(std::memory_order_acquire));
}

auto ServerEntity::GetParent() const -> refcount_ptr<const ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    ValidateEntityAccess(this);
    return refcount_ptr<const ServerEntity>(_parent.load(std::memory_order_acquire));
}

auto ServerEntity::GetParentRaw() const noexcept -> refcount_ptr<ServerEntity>
{
    FO_NO_STACK_TRACE_ENTRY();

    ServerEntity* p = _parent.load(std::memory_order_acquire);

    if (p == nullptr || !p->TryAddRef()) {
        return {};
    }

    return refcount_ptr<ServerEntity>(refcount_ptr<ServerEntity>::adopt, p);
}

void ServerEntity::SetParent(ServerEntity* parent) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    if (parent != nullptr) {
        parent->AddRef();
    }

    ServerEntity* old = _parent.exchange(parent, std::memory_order_acq_rel);

    if (old != nullptr) {
        old->Release();
    }
}

auto ServerEntity::FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult
{
    FO_STACK_TRACE_ENTRY();

    if (callbacks.empty()) {
        return EventResult::ContinueChain;
    }

    // Engine-wide invariant: a primary SyncContext is always active when an event fires.
    FO_STRONG_ASSERT(SyncContext::GetCurrentOnThisThread() != nullptr);

    bool had_exception = false;

    // Iterate a copy — callbacks vector may be changed/invalidated during cycle work.
    for (const auto& cb : copy(callbacks)) {
        EventResult result = EventResult::ContinueChain;

        try {
            // Wrap this callback in its own nested SyncContext on top of the dispatcher's
            // primary cover. Inner `Sync::Lock(...)` calls only mutate the nested layer, so
            // the primary's locks (the event's entity args) are preserved across the chain
            // and the next sibling sees them locked again.
            SyncContext nested;
            nested.Activate();
            auto cleanup = scope_exit([&]() noexcept {
                nested.Release();
                nested.Deactivate();
            });

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

FO_END_NAMESPACE
