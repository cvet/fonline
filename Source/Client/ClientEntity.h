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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

FO_BEGIN_NAMESPACE();

class FOClient;

class ClientEntity : public Entity
{
public:
    ClientEntity() = delete;
    ClientEntity(const ClientEntity&) = delete;
    ClientEntity(ClientEntity&&) noexcept = delete;
    auto operator=(const ClientEntity&) = delete;
    auto operator=(ClientEntity&&) noexcept = delete;
    ~ClientEntity() override = default;

    [[nodiscard]] auto GetId() const noexcept -> ident_t { return _id; }
    [[nodiscard]] auto GetEngine() const noexcept -> const FOClient* { return _engine.get(); }
    [[nodiscard]] auto GetEngine() noexcept -> FOClient* { return _engine.get(); }
    [[nodiscard]] auto GetName() const noexcept -> string_view override { return _name; }

    void SetId(ident_t id, bool register_entity);
    void DestroySelf();

protected:
    ClientEntity(FOClient* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props);

    virtual void OnDestroySelf() = 0;

    raw_ptr<FOClient> _engine;
    string _name {};

private:
    ident_t _id;
    bool _registered {};
};

class CustomEntityView : public ClientEntity, public EntityProperties
{
public:
    CustomEntityView(FOClient* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props) :
        ClientEntity(engine, id, registrator, props),
        EntityProperties(GetInitRef())
    {
    }

    void OnDestroySelf() override;
};

class CustomEntityWithProtoView : public CustomEntityView, public EntityWithProto
{
public:
    CustomEntityWithProtoView(FOClient* engine, ident_t id, const PropertyRegistrator* registrator, const ProtoEntity* proto) :
        CustomEntityView(engine, id, registrator, &proto->GetProperties()),
        EntityWithProto(proto)
    {
    }
};

FO_END_NAMESPACE();
