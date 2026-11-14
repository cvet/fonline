// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ClientEntity.h"

#include "Client.h"

FO_BEGIN_NAMESPACE

ClientEntity::ClientEntity(ptr<ClientEngine> engine, ident_t id, ptr<const PropertyRegistrar> registrar, nptr<const Properties> props, nptr<const Properties> base_props) :
    Entity(registrar, props, engine->Settings->Client.ClientPropertiesPackData ? base_props : nullptr),
    _engine {engine},
    _id {id}
{
    _name = GetTypeName();

    // The engine is borrowed, not owned: it owns the property registrars, protos, hashes and views this entity
    // reads, so no entity may outlive it. This count is what proves that in ~ClientEngine
    _engine->_liveEntityCount.fetch_add(1, std::memory_order_relaxed);

    if (_id) {
        _engine->RegisterEntity(this);
        _registered = true;
    }
}

ClientEntity::~ClientEntity()
{
    // Usually retired through DestroySelf, but the last owner (a script handle to a received item) may release it instead, and the
    // registry holds borrows, so that release must take the entry with it or a lookup by id returns freed memory
    if (_registered) {
        _engine->UnregisterEntity(this);
        _registered = false;
    }

    _engine->_liveEntityCount.fetch_sub(1, std::memory_order_acq_rel);
}

void ClientEntity::SetId(ident_t id, bool register_entity)
{
    FO_VERIFY_AND_THROW(!_id, "Id is already set");
    FO_VERIFY_AND_THROW(id, "Id is empty");

    _id = id;

    if (register_entity) {
        _engine->RegisterEntity(this);
        _registered = true;
    }
}

void ClientEntity::DestroySelf()
{
    FO_TRACE_ZONE(Entity);

    MarkAsDestroying();

    auto ensure_unregister {scope_exit([this]() noexcept {
        if (_registered) {
            safe_call([this]() { _engine->UnregisterEntity(this); });
            _registered = false;
        }
    })};

    OnDestroySelf();

    if (_registered) {
        _engine->UnregisterEntity(this);
        _registered = false;
    }

    if (HasInnerEntities()) {
        auto inner_entities {GetInnerEntities()};
        FO_VERIFY_AND_THROW(inner_entities, "Inner entities collection is null");

        for (auto& entities : *inner_entities | std::views::values) {
            for (auto& entity : entities) {
                auto custom_entity {require_refcount_ptr(entity.dyn_cast<CustomEntityView>())};
                custom_entity->DestroySelf();
            }
        }

        ClearInnerEntities();
    }

    MarkAsDestroyed();
}

void CustomEntityView::OnDestroySelf()
{
}

FO_END_NAMESPACE
