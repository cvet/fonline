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

#include "catch_amalgamated.hpp"

#include "Common.h"
#include "DataSerialization.h"
#include "FileSystem.h"
#include "ScriptSystem.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

namespace
{
    auto EchoUtf8View(u8string_view value) -> u8string_view
    {
        return value;
    }

    auto EchoAsciiView(string_view value) -> string
    {
        return string {value};
    }

    void ReplaceUtf8Value(u8string& value)
    {
        value = u8string {u8"замена 🌍"};
    }

    void ReplaceUtf8Array(vector<u8string>& values)
    {
        values.clear();
        values.emplace_back(u8"первый");
        values.emplace_back(u8"второй 🚀");
    }

    void ReplaceUtf8Dict(map<u8string, u8string>& values)
    {
        values.clear();
        values.emplace(u8string {u8"ключ"}, u8string {u8"значение 🌍"});
    }
}

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

        const u8string file_path_utf8 = fs_path_to_u8string(file_path);
        WriteSimpleTga(file_path_utf8.view(), image_size, pixels);

        REQUIRE(std::filesystem::exists(file_path));
        CHECK(std::filesystem::file_size(file_path) == 18 + pixels.size() * sizeof(uint32_t));

        std::ifstream input(file_path, std::ios::binary);
        REQUIRE(input);

        std::array<byte, 18> header {};
        REQUIRE(stream_read_exact(input, header));

        CHECK(header[2] == byte {2});
        CHECK(header[12] == byte {2});
        CHECK(header[13] == byte {0});
        CHECK(header[14] == byte {1});
        CHECK(header[15] == byte {0});
        CHECK(header[16] == byte {32});
        CHECK(header[17] == byte {0x20});

        std::array<uint32_t, 2> stored_pixels {};
        REQUIRE(stream_read_exact(input, make_byte_span(stored_pixels)));

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
}

TEST_CASE("SpriteResourceDecoderReadsCompleteResource")
{
    vector<byte> data;
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

    vector<byte> containing_data {byte {0xAA}, byte {0xBB}, byte {0xCC}};
    containing_data.insert(containing_data.end(), data.begin(), data.end());
    containing_data.insert(containing_data.end(), {byte {0xDD}, byte {0xEE}});
    const const_span<byte> resource_data = const_span<byte> {containing_data}.subspan(3, data.size());
    const SpriteResourceData resource = ReadSpriteResource(resource_data);

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

TEST_CASE("NativeScriptStrictTextBridge")
{
    SECTION("AnyPreservesUtf8Text")
    {
        const any_t value {u8"Привет 🌍"};

        CHECK(NativeScriptText::FromScriptString<u8string>(value) == u8"Привет 🌍");
    }

    SECTION("NativeCallerPreservesUtf8ViewAndReturnValue")
    {
        string script_value = utf8_to_char_string(u8"Привет 🌍");
        string script_result;
        const array<ptr<void>, 1> args {make_ptr(&script_value).void_cast()};
        FuncCallData call {
            .Accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR),
            .ArgsData = args,
            .RetData = make_ptr(&script_result).void_cast(),
        };

        NativeDataCaller::NativeCall<&EchoUtf8View>(call);

        CHECK(script_result == script_value);
    }

    SECTION("NativeCallerRejectsUnicodeForAsciiArgument")
    {
        string script_value = utf8_to_char_string(u8"Не ASCII");
        string script_result;
        const array<ptr<void>, 1> args {make_ptr(&script_value).void_cast()};
        FuncCallData call {
            .Accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR),
            .ArgsData = args,
            .RetData = make_ptr(&script_result).void_cast(),
        };

        CHECK_THROWS_AS(NativeDataCaller::NativeCall<&EchoAsciiView>(call), TextValidationException);
    }

    SECTION("NativeProviderRevalidatesBorrowedStrictViews")
    {
        std::u8string backing = u8"valid";
        const auto strict_view = u8string_view::TryFrom(backing);
        REQUIRE(strict_view.has_value());
        backing[0] = static_cast<char8_t>(0xFF);

        CHECK_THROWS_AS((void)NativeScriptText::ToScriptString(*strict_view), TextValidationException);
    }

    SECTION("NativeCallerWritesBackMutableUtf8Value")
    {
        string script_value = "initial";
        const array<ptr<void>, 1> args {make_ptr(&script_value).void_cast()};
        FuncCallData call {
            .Accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR),
            .ArgsData = args,
        };

        NativeDataCaller::NativeCall<&ReplaceUtf8Value>(call);

        CHECK(script_value == utf8_to_char_string(u8"замена 🌍"));
    }

    SECTION("NativeCallerConvertsMutableUtf8Array")
    {
        vector<u8string> script_values {u8string {u8"old"}, u8string {u8"старое"}};
        NativeDataProvider::StorageEntryType storage;
        const array<ptr<void>, 1> args {NativeDataProvider::NormalizeArg(script_values, storage)};
        FuncCallData call {
            .Accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR),
            .ArgsData = args,
        };

        NativeDataCaller::NativeCall<&ReplaceUtf8Array>(call);

        REQUIRE(script_values.size() == 2);
        CHECK(script_values[0] == u8"первый");
        CHECK(script_values[1] == u8"второй 🚀");
    }

    SECTION("NativeCallerConvertsMutableUtf8Dict")
    {
        map<u8string, u8string> script_values {{u8string {u8"old"}, u8string {u8"value"}}};
        NativeDataProvider::StorageEntryType storage;
        const array<ptr<void>, 1> args {NativeDataProvider::NormalizeArg(script_values, storage)};
        FuncCallData call {
            .Accessor = make_ptr(&NativeDataProvider::NATIVE_DATA_ACCESSOR),
            .ArgsData = args,
        };

        NativeDataCaller::NativeCall<&ReplaceUtf8Dict>(call);

        REQUIRE(script_values.size() == 1);
        CHECK(script_values.begin()->first == u8"ключ");
        CHECK(script_values.begin()->second == u8"значение 🌍");
    }

    SECTION("NativeProviderConvertsStrictArgumentsAndReturnValue")
    {
        ScriptFuncDesc desc;
        desc.Call = [](FuncCallData& call) {
            const string& input = *cast_from_void<const string*>(call.ArgsData[0].get());
            nptr<string> output_ptr = cast_from_void<string*>(call.RetData.as_ptr());
            string& output = *output_ptr;
            output = input + " / script";
        };
        ScriptFunc<u8string, u8string_view> func {make_ptr(&desc)};

        REQUIRE(func.Call(u8"привет 🌍"));

        const u8string result = func.GetResult();
        CHECK(result.view() == u8"привет 🌍 / script");
    }

    SECTION("NativeProviderConvertsUtf8ArrayReturnValue")
    {
        ScriptFuncDesc desc;
        desc.Call = [](FuncCallData& call) {
            string first = utf8_to_char_string(u8"один");
            string second = utf8_to_char_string(u8"два 🌍");
            call.Accessor->ClearArray(call.RetData.as_ptr());
            call.Accessor->AddArrayElement(call.RetData.as_ptr(), make_ptr(&first).void_cast());
            call.Accessor->AddArrayElement(call.RetData.as_ptr(), make_ptr(&second).void_cast());
        };
        ScriptFunc<vector<u8string>> func {make_ptr(&desc)};

        REQUIRE(func.Call());

        const vector<u8string> result = func.GetResult();
        REQUIRE(result.size() == 2);
        CHECK(result[0].view() == u8"один");
        CHECK(result[1].view() == u8"два 🌍");
    }

    SECTION("NativeProviderConvertsUtf8DictReturnValue")
    {
        ScriptFuncDesc desc;
        desc.Call = [](FuncCallData& call) {
            string key = utf8_to_char_string(u8"ключ");
            string value = utf8_to_char_string(u8"значение 🌍");
            call.Accessor->ClearDict(call.RetData.as_ptr());
            call.Accessor->AddDictElement(call.RetData.as_ptr(), make_ptr(&key).void_cast(), make_ptr(&value).void_cast());
        };
        ScriptFunc<map<u8string, u8string>> func {make_ptr(&desc)};

        REQUIRE(func.Call());

        const map<u8string, u8string> result = func.GetResult();
        REQUIRE(result.size() == 1);
        CHECK(result.begin()->first.view() == u8"ключ");
        CHECK(result.begin()->second.view() == u8"значение 🌍");
    }
}

FO_END_NAMESPACE
