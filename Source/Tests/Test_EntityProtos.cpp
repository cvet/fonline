// FOnline Engine
// SPDX-License-Identifier: MIT

#include "Common.h"

#include "catch_amalgamated.hpp"

#include "EngineBase.h"
#include "EntityProtos.h"

FO_BEGIN_NAMESPACE

namespace
{
    class TestEntityHolder final : public EntityWithProto
    {
    public:
        explicit TestEntityHolder(ptr<const ProtoEntity> proto) noexcept :
            EntityWithProto(proto)
        {
        }
    };

    class TestLifecycleEntity final : public Entity
    {
    public:
        explicit TestLifecycleEntity(ptr<const PropertyRegistrar> registrar) noexcept :
            Entity(registrar, nullptr, nullptr)
        {
        }
        ~TestLifecycleEntity() override = default;

        [[nodiscard]] auto GetName() const noexcept -> string_view override { return "TestLifecycleEntity"; }
    };

    static void InitEntityProtoTestMetadata(EngineMetadata& meta)
    {
        meta.RegisterSide(EngineSideKind::ServerSide);
        meta.RegisterEntityType("Item", true, false, true, true, true);
        meta.RegisterEntityType("Critter", true, false, true, true, true);
        meta.RegisterEntityType("TestEntity", true, false, true, false, false);
    }
}

TEST_CASE("EntityProtos")
{
    SECTION("BuiltInProtosExposeIdentityAndTypeName")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        hstring knife_pid {meta.Hashes.to_hashed_string("Knife")};
        hstring raider_pid {meta.Hashes.to_hashed_string("Raider")};
        auto item_registrar {meta.GetPropertyRegistrar("Item")};
        auto critter_registrar {meta.GetPropertyRegistrar("Critter")};

        REQUIRE(static_cast<bool>(item_registrar));
        REQUIRE(static_cast<bool>(critter_registrar));

        ProtoItem item_proto {knife_pid, item_registrar};
        ProtoCritter critter_proto {raider_pid, critter_registrar};

        CHECK(item_proto.GetProtoId() == knife_pid);
        CHECK(item_proto.GetTypeName() == meta.Hashes.to_hashed_string("Item"));
        CHECK(item_proto.GetName() == string_view {"Knife"});

        CHECK(critter_proto.GetProtoId() == raider_pid);
        CHECK(critter_proto.GetTypeName() == meta.Hashes.to_hashed_string("Critter"));
        CHECK(critter_proto.GetName() == string_view {"Raider"});
    }

    SECTION("CustomProtoExposesCustomTypeIdentity")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        auto registrar {meta.GetPropertyRegistrar("TestEntity")};
        REQUIRE(static_cast<bool>(registrar));

        hstring custom_pid {meta.Hashes.to_hashed_string("TestProto")};
        ProtoCustomEntity proto {custom_pid, registrar};

        CHECK(proto.GetProtoId() == custom_pid);
        CHECK(proto.GetTypeName() == meta.Hashes.to_hashed_string("TestEntity"));
        CHECK(proto.GetName() == string_view {"TestProto"});
    }

    SECTION("EntityWithProtoKeepsProtoAlive")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        hstring custom_pid {meta.Hashes.to_hashed_string("HeldProto")};
        auto registrar {meta.GetPropertyRegistrar("TestEntity")};
        REQUIRE(static_cast<bool>(registrar));

        optional<TestEntityHolder> holder {};

        {
            refcount_ptr<ProtoEntity> proto {safe_alloc::make_refcounted<ProtoCustomEntity>(custom_pid, registrar)};
            holder.emplace(proto);
        }

        REQUIRE(holder.has_value());

        auto held_proto {holder->GetProto()};
        CHECK(holder->GetProtoId() == custom_pid);
        CHECK(held_proto->GetName() == string_view {"HeldProto"});
        CHECK(held_proto->GetTypeName() == meta.Hashes.to_hashed_string("TestEntity"));
    }

    SECTION("LifecycleLatchesAreVisibleAcrossThreads")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        auto registrar {meta.GetPropertyRegistrar("TestEntity")};
        REQUIRE(static_cast<bool>(registrar));

        refcount_ptr<TestLifecycleEntity> entity {safe_alloc::make_refcounted<TestLifecycleEntity>(registrar)};
        std::atomic_bool reader_started {false};
        std::atomic_bool saw_destroying {false};
        std::atomic_bool saw_destroyed {false};

        std::thread reader {[&] {
            reader_started.store(true, std::memory_order_release);

            nanotime deadline {nanotime::now() + timespan {std::chrono::seconds {5}}};

            while (!entity->IsDestroying() && nanotime::now() < deadline) {
                std::this_thread::yield();
            }

            saw_destroying.store(entity->IsDestroying(), std::memory_order_release);

            while (!entity->IsDestroyed() && nanotime::now() < deadline) {
                std::this_thread::yield();
            }

            saw_destroyed.store(entity->IsDestroyed(), std::memory_order_release);
        }};

        while (!reader_started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        entity->MarkAsDestroying();
        entity->MarkAsDestroyed();
        reader.join();

        CHECK(saw_destroying.load(std::memory_order_acquire));
        CHECK(saw_destroyed.load(std::memory_order_acquire));
    }
}

FO_END_NAMESPACE
