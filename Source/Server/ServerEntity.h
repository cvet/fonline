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

#pragma once

#include "Common.h"

#include "Entity.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

class EntityLock;
class ServerEngine;

class ServerEntity : public Entity
{
    friend class EntityManager;

public:
    ServerEntity() = delete;
    ServerEntity(const ServerEntity&) = delete;
    ServerEntity(ServerEntity&&) noexcept = delete;
    auto operator=(const ServerEntity&) = delete;
    auto operator=(ServerEntity&&) noexcept = delete;
    ~ServerEntity() override;

    [[nodiscard]] auto GetId() const noexcept -> ident_t override { return _id; }
    [[nodiscard]] auto GetEngine() const noexcept -> const ServerEngine* { return _engine.get(); }
    [[nodiscard]] auto GetEngine() noexcept -> ServerEngine* { return _engine.get(); }
    [[nodiscard]] auto IsInitCalled() const noexcept -> bool { return _initCalled; }
    [[nodiscard]] auto IsPersistent() const noexcept -> bool { return _isPersistent; }
    [[nodiscard]] auto IsExplicitlyPersistent() const noexcept -> bool;
    [[nodiscard]] auto GetEntityLock() const noexcept -> EntityLock* { return _entityLock.get(); }

    void ValidateAccess() const override;

    void LockForPropertyAccess() noexcept override;
    void UnlockForPropertyAccess() noexcept override;
    void LockForPropertyAccessShared() noexcept override;
    void UnlockForPropertyAccessShared() noexcept override;

    [[nodiscard]] auto GetParent() -> refcount_ptr<ServerEntity>;
    [[nodiscard]] auto GetParent() const -> refcount_ptr<const ServerEntity>;

    template<typename T>
    [[nodiscard]] auto GetParent() -> refcount_ptr<T>
    {
        ValidateEntityAccess(this);
        return refcount_ptr<T>(dynamic_cast<T*>(_parent.load(std::memory_order_acquire)));
    }
    template<typename T>
    [[nodiscard]] auto GetParent() const -> refcount_ptr<const T>
    {
        ValidateEntityAccess(this);
        return refcount_ptr<const T>(dynamic_cast<const T*>(_parent.load(std::memory_order_acquire)));
    }

    // Unchecked parent accessor — for the lock machinery only.
    [[nodiscard]] auto GetParentRaw() const noexcept -> refcount_ptr<ServerEntity>;

    // Return the entity that should be auto-widened into the SyncContext alongside this one,
    // outside of the parent-chain.
    [[nodiscard]] virtual auto GetSyncWidenEntity() noexcept -> ServerEntity* { return nullptr; }

    void SetInitCalled() noexcept { _initCalled = true; }
    void SetEntityLock(EntityLock* lock) noexcept { _entityLock = lock; }
    void SetParent(ServerEntity* parent) noexcept;

protected:
    ServerEntity(ServerEngine* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props, const Properties* base_props) noexcept;

    auto FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult override;

    raw_ptr<ServerEngine> _engine;

private:
    void SetId(ident_t id) noexcept; // Invoked by EntityManager
    void SetPersistent(bool persistent) noexcept; // Invoked by EntityManager
    void SetExplicitlyPersistent(bool explicitly_persistent); // Invoked by EntityManager

    ident_t _id;
    bool _initCalled {};
    bool _isPersistent {};
    mutable raw_ptr<EntityLock> _entityLock {};
    std::atomic<ServerEntity*> _parent {};
};

class CustomEntity : public ServerEntity, public EntityProperties
{
public:
    CustomEntity(ServerEngine* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props, const Properties* base_props = nullptr) noexcept :
        ServerEntity(engine, id, registrator, props, base_props),
        EntityProperties(GetInitRef())
    {
    }

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _propsRef->GetRegistrator()->GetTypeName(); }
};

class CustomEntityWithProto : public CustomEntity, public EntityWithProto
{
public:
    CustomEntityWithProto(ServerEngine* engine, ident_t id, const PropertyRegistrator* registrator, const ProtoEntity* proto) noexcept :
        CustomEntity(engine, id, registrator, &proto->GetProperties(), &proto->GetProperties()),
        EntityWithProto(proto)
    {
    }

    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _proto->GetName(); }
};

FO_END_NAMESPACE
