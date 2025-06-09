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

#include "ClientEntity.h"
#include "Client.h"

FO_BEGIN_NAMESPACE();

ClientEntity::ClientEntity(FOClient* engine, ident_t id, const PropertyRegistrator* registrator, const Properties* props) :
    Entity(registrator, props),
    _engine {engine},
    _id {id}
{
    FO_STACK_TRACE_ENTRY();

    _name = GetTypeName();

    if (_id) {
        _engine->RegisterEntity(this);
        _registered = true;
    }
}

void ClientEntity::SetId(ident_t id, bool register_entity)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT(!_id);
    FO_RUNTIME_ASSERT(id);

    _id = id;

    if (register_entity) {
        _engine->RegisterEntity(this);
        _registered = true;
    }
}

void ClientEntity::DestroySelf()
{
    FO_STACK_TRACE_ENTRY();

    MarkAsDestroying();

    OnDestroySelf();

    if (_registered) {
        _engine->UnregisterEntity(this);
    }

    if (HasInnerEntities()) {
        for (auto& entities : GetInnerEntities() | std::views::values) {
            for (auto& entity : entities) {
                auto* custom_entity = dynamic_cast<CustomEntityView*>(entity.get());
                FO_RUNTIME_ASSERT(custom_entity);

                custom_entity->DestroySelf();
            }
        }

        ClearInnerEntities();
    }

    MarkAsDestroyed();
}

void CustomEntityView::OnDestroySelf()
{
    FO_STACK_TRACE_ENTRY();
}

FO_END_NAMESPACE();
