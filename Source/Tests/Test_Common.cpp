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

#include "catch_amalgamated.hpp"

#include "Common.h"
#include "DataSerialization.h"
#include "FileSystem.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

TEST_CASE("CommonEvents")
{
    SECTION("DispatchAndManualUnsubscribe")
    {
        EventObserver<int32_t> observer;
        EventDispatcher<int32_t> dispatch(&observer);
        EventUnsubscriber unsubscriber;
        int32_t sum = 0;

        unsubscriber += (observer += [&](int32_t value) { sum += value; });
        unsubscriber += (observer += [&](int32_t value) { sum += value * 10; });

        dispatch(2);
        CHECK(sum == 22);

        unsubscriber.Unsubscribe();
        dispatch(3);
        CHECK(sum == 22);

        unsubscriber.Unsubscribe();
        dispatch(4);
        CHECK(sum == 22);
    }

    SECTION("DestructorUnsubscribes")
    {
        EventObserver<int32_t> observer;
        EventDispatcher<int32_t> dispatch(&observer);
        int32_t calls = 0;

        {
            EventUnsubscriber unsubscriber;
            unsubscriber += (observer += [&](int32_t value) { calls += value; });

            dispatch(5);
            CHECK(calls == 5);
        }

        dispatch(7);
        CHECK(calls == 5);
    }

    SECTION("MoveKeepsSubscriptionOwnership")
    {
        EventObserver<int32_t> observer;
        EventDispatcher<int32_t> dispatch(&observer);
        int32_t calls = 0;

        EventUnsubscriber original;
        original += (observer += [&](int32_t value) { calls += value; });

        EventUnsubscriber moved = std::move(original);

        dispatch(3);
        CHECK(calls == 3);

        moved.Unsubscribe();
        dispatch(8);
        CHECK(calls == 3);
    }

    SECTION("DispatchCopiesValueArgumentsForEverySubscriber")
    {
        EventObserver<string> observer;
        EventDispatcher<string> dispatch(&observer);
        EventUnsubscriber unsubscriber;
        vector<string> received;

        unsubscriber += (observer += [&](string value) { received.emplace_back(std::move(value)); });
        unsubscriber += (observer += [&](string value) { received.emplace_back(std::move(value)); });

        dispatch(string {"payload"});

        REQUIRE(received.size() == 2);
        CHECK(received[0] == "payload");
        CHECK(received[1] == "payload");
    }
}

TEST_CASE("CommonUtilities")
{
    SECTION("WriteSimpleTgaCreatesFileWithExpectedHeader")
    {
        auto temp_root = std::filesystem::temp_directory_path() / "lf_common_tests" / std::to_string(std::random_device {}());
        auto file_path = temp_root / "nested" / "sample.tga";

        isize32 image_size {2, 1};
        vector<ucolor> pixels;
        pixels.emplace_back(ucolor {1, 2, 3, 4});
        pixels.emplace_back(ucolor {5, 6, 7, 8});

        WriteSimpleTga(string(file_path.string()), image_size, pixels);

        REQUIRE(std::filesystem::exists(file_path));
        CHECK(std::filesystem::file_size(file_path) == 18 + pixels.size() * sizeof(uint32_t));

        std::ifstream input(file_path, std::ios::binary);
        REQUIRE(input);

        std::array<uint8_t, 18> header {};
        input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(header.size()));

        CHECK(header[2] == 2);
        CHECK(header[12] == 2);
        CHECK(header[13] == 0);
        CHECK(header[14] == 1);
        CHECK(header[15] == 0);
        CHECK(header[16] == 32);
        CHECK(header[17] == 0x20);

        std::array<uint32_t, 2> stored_pixels {};
        input.read(reinterpret_cast<char*>(stored_pixels.data()), static_cast<std::streamsize>(sizeof(stored_pixels)));
        REQUIRE(input.gcount() == static_cast<std::streamsize>(sizeof(stored_pixels)));

        // A TrueColor TGA stores pixels in B, G, R, A order, so the writer swaps red and blue
        auto to_bgra = [](ucolor c) -> uint32_t {
            std::swap(c.comp.r, c.comp.b);
            return c.rgba;
        };

        CHECK(stored_pixels[0] == to_bgra(pixels[0]));
        CHECK(stored_pixels[1] == to_bgra(pixels[1]));

        input.close();

        uintmax_t removed = std::filesystem::remove_all(temp_root);
        CHECK(removed > 0);
    }

    SECTION("SeededRandomGeneratorProducesValues")
    {
        auto generator = MakeSeededRandomGenerator();

        // Two generators need not differ, since random_device may be deterministic, but the engine must not walk
        // mt19937's fixed default sequence, which would mean it was never seeded
        std::mt19937 default_seeded;
        bool matches_default_sequence = true;

        for (int32_t i = 0; i < 4; i++) {
            if (generator() != default_seeded()) {
                matches_default_sequence = false;
            }
        }

        CHECK_FALSE(matches_default_sequence);

        // And that it is immediately usable by its consumers
        std::uniform_int_distribution<int32_t> distribution {10, 20};

        for (int32_t i = 0; i < 8; i++) {
            int32_t value = distribution(generator);
            CHECK(value >= 10);
            CHECK(value <= 20);
        }
    }

    SECTION("PackagedBuildAccessorsAreConsistent")
    {
        // An unpackaged test build reports no runtime name; a packaged one must name it
        CHECK(IsPackaged() == !GetPackagedRuntimeName().empty());
    }
}

TEST_CASE("CommonFrameBalancer")
{
    SECTION("DisabledBalancerDoesNotWait")
    {
        FrameBalancer balancer {false, 100, 0};

        nanotime start = nanotime::now();
        balancer.StartLoop();
        balancer.EndLoop();

        CHECK(nanotime::now() - start < timespan {std::chrono::milliseconds {50}});
    }

    SECTION("ZeroSleepYieldsInsteadOfSleeping")
    {
        FrameBalancer balancer {true, 0, 0};

        nanotime start = nanotime::now();
        balancer.StartLoop();
        balancer.EndLoop();

        CHECK(nanotime::now() - start < timespan {std::chrono::milliseconds {50}});
    }

    SECTION("PositiveSleepWaitsForTheRequestedTime")
    {
        FrameBalancer balancer {true, 5, 0};

        nanotime start = nanotime::now();
        balancer.StartLoop();
        balancer.EndLoop();

        CHECK(nanotime::now() - start >= timespan {std::chrono::milliseconds {4}});
    }

    SECTION("FixedFpsBalancesTheIdleTime")
    {
        // A negative sleep hands control to the fixed-fps arm, which pads each loop up to the frame budget
        FrameBalancer balancer {true, -1, 200};

        nanotime start = nanotime::now();

        for (int32_t i = 0; i < 3; i++) {
            balancer.StartLoop();
            balancer.EndLoop();
        }

        CHECK(nanotime::now() - start >= timespan {std::chrono::milliseconds {5}});
    }
}

TEST_CASE("CommonRemoteCallWireSizes")
{
    auto make_primitive = [](string_view name, size_t size) {
        BaseTypeDesc type;
        type.Name = string {name};
        type.Size = size;
        type.IsPrimitive = true;
        return type;
    };

    SECTION("PlainAndReferenceTypesReportTheirMinimumSize")
    {
        CHECK(GetRemoteCallSimpleValueMinWireSize(make_primitive("int32", sizeof(int32_t))) == sizeof(int32_t));
        CHECK(GetRemoteCallSimpleValueMinWireSize(make_primitive("int64", sizeof(int64_t))) == sizeof(int64_t));

        BaseTypeDesc enum_type;
        enum_type.Name = "TestEnum";
        enum_type.Size = sizeof(uint16_t);
        enum_type.IsEnum = true;
        CHECK(GetRemoteCallSimpleValueMinWireSize(enum_type) == sizeof(uint16_t));

        // A string only has to carry its length prefix to be well formed
        BaseTypeDesc string_type;
        string_type.Name = "string";
        string_type.IsString = true;
        CHECK(GetRemoteCallSimpleValueMinWireSize(string_type) == sizeof(uint32_t));

        BaseTypeDesc hash_type;
        hash_type.Name = "hstring";
        hash_type.IsHashedString = true;
        CHECK(GetRemoteCallSimpleValueMinWireSize(hash_type) == sizeof(hstring::hash_t));
    }

    SECTION("StructSizesAreTheSumOfTheirFields")
    {
        auto layout = SafeAlloc::MakeShared<StructLayoutDesc>();
        layout->Fields.emplace_back();
        layout->Fields.back().Type = make_primitive("int32", sizeof(int32_t));
        layout->Fields.emplace_back();
        layout->Fields.back().Type = make_primitive("int16", sizeof(int16_t));

        BaseTypeDesc struct_type;
        struct_type.Name = "TestStruct";
        struct_type.IsStruct = true;
        struct_type.StructLayout = layout;

        CHECK(GetRemoteCallSimpleValueMinWireSize(struct_type) == sizeof(int32_t) + sizeof(int16_t));
    }

    SECTION("UnsupportedAndMalformedTypesAreRejected")
    {
        BaseTypeDesc zero_sized;
        zero_sized.Name = "BadPrimitive";
        zero_sized.IsPrimitive = true;
        CHECK_THROWS_AS(GetRemoteCallSimpleValueMinWireSize(zero_sized), VerificationException);

        BaseTypeDesc no_layout;
        no_layout.Name = "BadStruct";
        no_layout.IsStruct = true;
        CHECK_THROWS_AS(GetRemoteCallSimpleValueMinWireSize(no_layout), VerificationException);

        BaseTypeDesc unsupported;
        unsupported.Name = "Unsupported";
        CHECK_THROWS_AS(GetRemoteCallSimpleValueMinWireSize(unsupported), NotSupportedException);
    }
}

TEST_CASE("SpriteResourceDecoderReadsCompleteResource")
{
    vector<uint8_t> data;
    DataWriter writer {data};
    vector<ucolor> pixels {ucolor {1, 2, 3, 4}, ucolor {5, 6, 7, 8}};

    writer.Write<uint8_t>(SPRITE_RESOURCE_MAGIC);
    writer.Write<uint8_t>(SPRITE_RESOURCE_VERSION);
    writer.Write<uint16_t>(uint16_t {2});
    writer.Write<uint16_t>(uint16_t {75});
    writer.Write<uint8_t>(uint8_t {1});

    writer.Write<uint8_t>(uint8_t {0});
    writer.Write<int16_t>(int16_t {-3});
    writer.Write<int16_t>(int16_t {4});
    writer.Write<uint16_t>(uint16_t {2});
    writer.Write<uint16_t>(uint16_t {1});
    writer.Write<int16_t>(int16_t {5});
    writer.Write<int16_t>(int16_t {-6});
    writer.WriteObjectVector(pixels);
    writer.Write<uint8_t>(static_cast<uint8_t>(SpriteMeshKind::Mesh));
    writer.Write<uint16_t>(uint16_t {3});
    writer.Write<uint32_t>(uint32_t {3});
    writer.Write<uint16_t>(uint16_t {2});
    writer.Write<uint16_t>(uint16_t {1});
    writer.Write<int32_t>(int32_t {0});
    writer.Write<int32_t>(int32_t {0});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint16_t>(uint16_t {2});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint16_t>(uint16_t {1});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint16_t>(uint16_t {1});
    writer.Write<uint16_t>(uint16_t {2});

    writer.Write<uint8_t>(uint8_t {1});
    writer.Write<uint16_t>(uint16_t {0});
    writer.Write<uint8_t>(SPRITE_RESOURCE_MAGIC);

    vector<uint8_t> containing_data {0xAA, 0xBB, 0xCC};
    containing_data.insert(containing_data.end(), data.begin(), data.end());
    containing_data.insert(containing_data.end(), {0xDD, 0xEE});
    const_span<uint8_t> resource_data = const_span<uint8_t> {containing_data}.subspan(3, data.size());
    SpriteResourceData resource = ReadSpriteResource(resource_data);

    REQUIRE(resource.Animation.Sprite.has_value());
    const SpriteInfo& sprite_info = *resource.Animation.Sprite;
    CHECK(sprite_info.FrameCount == 2);
    CHECK(sprite_info.Duration == std::chrono::milliseconds {75});
    REQUIRE(sprite_info.Directions.size() == 1);
    REQUIRE(sprite_info.Directions.front().Frames.size() == 2);
    CHECK(sprite_info.Directions.front().Frames[0].Offset == ipos32 {-3, 4});
    CHECK(sprite_info.Directions.front().Frames[0].Size == isize32 {2, 1});
    CHECK(sprite_info.Directions.front().Frames[0].NextOffset == ipos32 {5, -6});
    REQUIRE(sprite_info.Directions.front().Frames[1].SharedFrameIndex.has_value());
    CHECK(*sprite_info.Directions.front().Frames[1].SharedFrameIndex == 0);
    CHECK(sprite_info.Directions.front().Frames[1].Offset == ipos32 {-3, 4});
    CHECK(sprite_info.Directions.front().Frames[1].Size == isize32 {2, 1});
    CHECK(sprite_info.Directions.front().Frames[1].NextOffset == ipos32 {5, -6});
    REQUIRE(resource.Directions.size() == 1);

    const SpriteResourceDirectionData& direction = resource.Directions.front();
    REQUIRE(direction.Frames.size() == 2);

    const SpriteResourceFrameData& frame = direction.Frames[0];
    CHECK_FALSE(frame.SharedFrameIndex.has_value());
    CHECK(frame.Offset == ipos32 {-3, 4});
    CHECK(frame.Size == isize32 {2, 1});
    CHECK(frame.NextOffset == ipos32 {5, -6});
    CHECK(frame.Pixels == pixels);
    REQUIRE(frame.Mesh.has_value());
    CHECK(frame.Mesh->SourceSize == isize32 {2, 1});
    CHECK(frame.Mesh->SourceOffset == ipos32 {});
    CHECK(frame.Mesh->Vertices == vector<ipos32> {{0, 0}, {2, 0}, {0, 1}});
    CHECK(frame.Mesh->Indices == vector<uint16_t> {0, 1, 2});

    const SpriteResourceFrameData& shared_frame = direction.Frames[1];
    REQUIRE(shared_frame.SharedFrameIndex.has_value());
    CHECK(*shared_frame.SharedFrameIndex == 0);
    CHECK(shared_frame.Pixels.empty());
    CHECK_FALSE(shared_frame.Mesh.has_value());

    SpriteResourceFrameData cropped_frame;
    cropped_frame.Size = {3, 3};
    cropped_frame.Pixels = {
        ucolor {1, 0, 0},
        ucolor {2, 0, 0},
        ucolor {3, 0, 0},
        ucolor {4, 0, 0},
        ucolor {5, 0, 0},
        ucolor {6, 0, 0},
        ucolor {7, 0, 0},
        ucolor {8, 0, 0},
        ucolor {9, 0, 0},
    };
    cropped_frame.Mesh = SpriteMeshData {
        .SourceSize = {4, 3},
        .SourceOffset = {1, -1},
        .Indices = {0, 1, 2},
    };

    SpriteResourceImageData restored_image = ExtractSpriteResourceFrameImage(std::move(cropped_frame));
    CHECK(restored_image.Size == isize32 {4, 3});
    CHECK(restored_image.Pixels ==
        vector<ucolor> {
            ucolor {},
            ucolor {4, 0, 0},
            ucolor {5, 0, 0},
            ucolor {6, 0, 0},
            ucolor {},
            ucolor {7, 0, 0},
            ucolor {8, 0, 0},
            ucolor {9, 0, 0},
            ucolor {},
            ucolor {},
            ucolor {},
            ucolor {},
        });
}

FO_END_NAMESPACE
