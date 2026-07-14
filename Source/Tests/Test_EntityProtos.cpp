//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
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
        explicit TestLifecycleEntity(ptr<const PropertyRegistrator> registrator) noexcept :
            Entity(registrator, nullptr, nullptr)
        {
            FO_NO_STACK_TRACE_ENTRY();
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

        const hstring knife_pid = meta.Hashes.ToHashedString("Knife");
        const hstring raider_pid = meta.Hashes.ToHashedString("Raider");
        auto item_registrator = meta.GetPropertyRegistrator("Item");
        auto critter_registrator = meta.GetPropertyRegistrator("Critter");

        REQUIRE(static_cast<bool>(item_registrator));
        REQUIRE(static_cast<bool>(critter_registrator));

        ProtoItem item_proto {knife_pid, item_registrator};
        ProtoCritter critter_proto {raider_pid, critter_registrator};

        CHECK(item_proto.GetProtoId() == knife_pid);
        CHECK(item_proto.GetTypeName() == meta.Hashes.ToHashedString("Item"));
        CHECK(item_proto.GetName() == string_view {"Knife"});

        CHECK(critter_proto.GetProtoId() == raider_pid);
        CHECK(critter_proto.GetTypeName() == meta.Hashes.ToHashedString("Critter"));
        CHECK(critter_proto.GetName() == string_view {"Raider"});
    }

    SECTION("CustomProtoExposesCustomTypeIdentity")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        auto registrator = meta.GetPropertyRegistrator("TestEntity");
        REQUIRE(static_cast<bool>(registrator));

        const hstring custom_pid = meta.Hashes.ToHashedString("TestProto");
        ProtoCustomEntity proto {custom_pid, registrator};

        CHECK(proto.GetProtoId() == custom_pid);
        CHECK(proto.GetTypeName() == meta.Hashes.ToHashedString("TestEntity"));
        CHECK(proto.GetName() == string_view {"TestProto"});
    }

    SECTION("EntityWithProtoKeepsProtoAlive")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        const hstring custom_pid = meta.Hashes.ToHashedString("HeldProto");
        auto registrator = meta.GetPropertyRegistrator("TestEntity");
        REQUIRE(static_cast<bool>(registrator));

        optional<TestEntityHolder> holder;

        {
            refcount_ptr<ProtoEntity> proto = SafeAlloc::MakeRefCounted<ProtoCustomEntity>(custom_pid, registrator);
            holder.emplace(proto);
        }

        REQUIRE(holder.has_value());

        auto held_proto = holder->GetProto();
        CHECK(holder->GetProtoId() == custom_pid);
        CHECK(held_proto->GetName() == string_view {"HeldProto"});
        CHECK(held_proto->GetTypeName() == meta.Hashes.ToHashedString("TestEntity"));
    }

    SECTION("LifecycleLatchesAreVisibleAcrossThreads")
    {
        EngineMetadata meta {[] { }};
        InitEntityProtoTestMetadata(meta);

        auto registrator = meta.GetPropertyRegistrator("TestEntity");
        REQUIRE(static_cast<bool>(registrator));

        refcount_ptr<TestLifecycleEntity> entity = SafeAlloc::MakeRefCounted<TestLifecycleEntity>(registrator);
        std::atomic_bool reader_started {false};
        std::atomic_bool saw_destroying {false};
        std::atomic_bool saw_destroyed {false};

        std::thread reader {[&] {
            reader_started.store(true, std::memory_order_release);

            const nanotime deadline = nanotime::now() + timespan {std::chrono::seconds {5}};
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
