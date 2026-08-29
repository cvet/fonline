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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

    [[nodiscard]] auto GetId() const noexcept -> ident_t override;
    [[nodiscard]] auto GetEngine() const noexcept -> ptr<const ServerEngine>;
    [[nodiscard]] auto GetEngine() noexcept -> ptr<ServerEngine>;
    [[nodiscard]] auto IsInitCalled() const noexcept -> bool;
    [[nodiscard]] auto IsPersistent() const noexcept -> bool;
    [[nodiscard]] auto IsExplicitlyPersistent() const noexcept -> bool;
    [[nodiscard]] auto GetEntityLock() const noexcept -> nptr<EntityLock>;

    void ValidateAccess() const override;

    [[nodiscard]] auto GetParent() -> refcount_nptr<ServerEntity>;
    [[nodiscard]] auto GetParent() const -> refcount_nptr<const ServerEntity>;

    template<typename T>
    [[nodiscard]] auto GetParent() -> refcount_nptr<T>
    {
        FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
        return GetParentRaw().dyn_cast<T>();
    }
    template<typename T>
    [[nodiscard]] auto GetParent() const -> refcount_nptr<const T>
    {
        FO_VALIDATE_ENTITY(LOCKED, NOT_DESTROYED);
        return GetParentRaw().dyn_cast<const T>();
    }

    // Unchecked parent accessor — for the lock machinery only
    [[nodiscard]] auto GetParentRaw() const noexcept -> refcount_nptr<ServerEntity>;

    // Return an owning handle to the entity that should be auto-widened into the SyncContext alongside this
    // one, outside of the parent-chain. Owning, because the caller reads the link before covering its target
    [[nodiscard]] virtual auto GetSyncWidenEntity() noexcept -> refcount_nptr<ServerEntity>;
    [[nodiscard]] virtual auto GetSyncWidenEntity() const noexcept -> refcount_nptr<const ServerEntity>;

    void SetInitCalled() noexcept;
    void SetEntityLock(nptr<EntityLock> lock) noexcept;
    void SetParent(nptr<ServerEntity> parent) noexcept;

protected:
    ServerEntity(ptr<ServerEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props) noexcept;

    auto FireEvent(const vector<EventCallbackData>& callbacks, FuncCallData& call) noexcept -> EventResult override;

    ptr<ServerEngine> _engine;

private:
    void SetId(ident_t id) noexcept; // Invoked by EntityManager
    void SetPersistent(bool persistent) noexcept; // Invoked by EntityManager
    void SetExplicitlyPersistent(bool explicitly_persistent); // Invoked by EntityManager

    ident_t _id;
    bool _initCalled {};
    bool _isPersistent {};
    mutable nptr<EntityLock> _entityLock {};
    mutable atomic_mutex _parentLinkLocker {};
    std::atomic<ServerEntity*> _parent {};
};

class CustomEntity : public ServerEntity, public EntityProperties
{
public:
    CustomEntity(ptr<ServerEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props = nullptr) noexcept :
        ServerEntity(engine, id, registrar, props, base_props),
        EntityProperties(*GetInitRef())
    {
        FO_VALIDATE_ENTITY(NONE);
    }

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
};

class CustomEntityWithProto : public CustomEntity, public EntityWithProto
{
public:
    CustomEntityWithProto(ptr<ServerEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, ptr<const ProtoEntity> proto) noexcept :
        CustomEntity(engine, id, registrar, proto->GetProperties(), proto->GetProperties()),
        EntityWithProto(proto)
    {
        FO_VALIDATE_ENTITY(NONE);
    }

    [[nodiscard]] auto GetName() const noexcept -> string_view override;
};

FO_END_NAMESPACE
