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

#include "ConfigFile.h"
#include "ImageBaker.h"
#include "ImageDependencyCache.h"
#include "SpriteResource.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

TEST_CASE("ImageDependencies")
{
    BakerTests::TestRig rig;
    rig.AddSourceFile("gfx/Source.tga", vector<uint8_t> {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 0, 0, 0, 255, 255});
    rig.AddSourceFile("gfx/Child One.fofrm", "Frame = Source.tga\n");
    rig.AddSourceFile("gfx/Root.fofrm", "Frames = \"Child One.fofrm\" \"Child One.fofrm\"\n");

    SECTION("CollectsQuotedAndTransitiveReferences")
    {
        ImageBaker baker {rig.MakeContext()};
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.contains("gfx/Root.fofrm"));
        auto resource = ReadSpriteResource(rig.Outputs.at("gfx/Root.fofrm"));
        REQUIRE(resource.Directions.size() == 1);
        REQUIRE(resource.Directions[0].Frames.size() == 2);
        CHECK(resource.Directions[0].Frames[0].Pixels[0] == ucolor {255, 0, 0, 255});
        CHECK_FALSE(rig.Outputs.contains("gfx/Source.tga"));
        CHECK_FALSE(rig.Outputs.contains("gfx/Child One.fofrm"));
    }
}

TEST_CASE("ImageDependencyPipeline")
{
    string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_dependencies_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    string source_path = strex(input_dir).combine_path("Source.tga").str();
    vector<uint8_t> pixels {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255};
    REQUIRE(fs::write_file(source_path, pixels));
    REQUIRE(fs::write_file(strex(input_dir).combine_path("Source.fofrm"), string_view {"Frame = Source.tga\n"}));

    string output_filter;

    SECTION("SourcesRemainReadableWhenExcludedFromOutputs")
    {
        output_filter = "ExcludePatterns = *.tga\n";
    }

    SECTION("DetectsContentChangeWithSameStat")
    {
    }

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    ConfigFile config {strex("Baking.BakeOutput = {}\nBaking.SingleThreadBaking = True\n[ResourcePack]\nName = Art\nInputDirs = input\n{}Bakers = Image\n", output_dir, output_filter).str()};
    settings.ApplyConfigFile(config, temp_dir);

    MasterBaker initial {&settings};
    REQUIRE(initial.BakeAll());
    string root_output = strex(output_dir).combine_path("Art/Source.fofrm").str();
    REQUIRE(fs::exists(root_output));
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Art/Source.tga")));

    REQUIRE(fs::write_file(strex(output_dir).combine_path("Art/Source.tga"), string_view {"previous standalone output"}));
    auto original_time = std::filesystem::last_write_time(fs::make_path(source_path));
    pixels[19] = 255;
    pixels[20] = 0;
    REQUIRE(fs::write_file(source_path, pixels));
    std::filesystem::last_write_time(fs::make_path(source_path), original_time);

    MasterBaker changed {&settings};
    REQUIRE(changed.BakeAll());
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Art/Source.tga")));

    auto baked = fs::read_file(root_output);
    REQUIRE(baked.has_value());
    auto resource = ReadSpriteResource(make_const_span(*baked));
    CHECK(resource.Directions[0].Frames[0].Pixels[0] == ucolor {0, 255, 0, 255});
}

TEST_CASE("ImageDependencyValidation")
{
    BakerTests::TestRig rig;

    SECTION("RejectsSelfCycle")
    {
        rig.AddSourceFile("A.fofrm", "Frame = A.fofrm\n");
    }

    SECTION("RejectsIndirectCycle")
    {
        rig.AddSourceFile("A.fofrm", "Frame = B.fofrm\n");
        rig.AddSourceFile("B.fofrm", "Frame = A.fofrm\n");
    }

    SECTION("RejectsExcessiveDepthWithChain")
    {
        for (size_t index = 0; index < 65; index++) {
            rig.AddSourceFile(strex("{}.fofrm", index), index < 64 ? strex("Frame = {}.fofrm\n", index + 1).str() : string {"Frame = Source.tga\n"});
        }

        rig.AddSourceFile("Source.tga", vector<uint8_t> {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255});
    }

    ImageBaker baker {rig.MakeContext()};
    CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ImageBakerException);
}

TEST_CASE("ImageDependenciesAcrossPacks")
{
    string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_overlays_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
    string output_dir = strex(temp_dir).combine_path("output").str();
    vector<uint8_t> pixels {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255};
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("lower/Source.tga"), pixels));
    pixels[19] = 255;
    pixels[20] = 0;
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("upper/Source.tga"), pixels));
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("recipes/Root.fofrm"), string_view {"Frame = Source.tga\n"}));
    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    ConfigFile config {strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = True
Baking.RawCopyFileExtensions = tga
[ResourcePack]
Name = Lower
InputDirs = lower
Bakers = Image RawCopy
[ResourcePack]
Name = Upper
InputDirs = upper
Bakers = Image RawCopy
[ResourcePack]
Name = Recipes
InputDirs = recipes
Bakers = Image
)",
        output_dir)
            .str()};
    settings.ApplyConfigFile(config, temp_dir);
    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Lower/Source.tga")));
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Upper/Source.tga")));
    auto baked = fs::read_file(strex(output_dir).combine_path("Recipes/Root.fofrm"));
    REQUIRE(baked.has_value());
    auto resource = ReadSpriteResource(make_const_span(*baked));
    CHECK(resource.Directions[0].Frames[0].Pixels[0] == ucolor {0, 255, 0, 255});

    REQUIRE(fs::remove_file(strex(temp_dir).combine_path("recipes/Root.fofrm")));
    MasterBaker unreferenced {&settings};
    REQUIRE(unreferenced.BakeAll());
    CHECK(fs::exists(strex(output_dir).combine_path("Lower/Source.tga")));
    CHECK(fs::exists(strex(output_dir).combine_path("Upper/Source.tga")));
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Recipes/Root.fofrm")));
}

TEST_CASE("ImageDependencySnapshots")
{
    string output_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_snapshots_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&output_dir]() noexcept { (void)fs::remove_dir_tree(output_dir); });
    BakerTests::TestRig rig;
    BakerTests::OverrideSetting(rig.Settings.Baking.BakeOutput, output_dir);
    rig.AddSourceFile("A.toy", vector<uint8_t> {10});
    rig.AddSourceFile("B.toy", vector<uint8_t> {30});
    rig.AddSourceFile("A.fofrm", "Frame = A.toy\n");
    rig.AddSourceFile("B.fofrm", "Frame = B.toy\n");
    bool fail_write = false;
    auto options = rig.MakeContextOptions();
    options.WriteData = [&](string_view path, const_span<uint8_t> data) {
        if (fail_write && path == "A.fofrm") {
            throw ImageBakerException("Injected output write failure");
        }

        bool written = fs::write_file(strex(output_dir).combine_path("TestPack").combine_path(path), data);
        REQUIRE(written);
        rig.Outputs.insert_or_assign(string {path}, vector<uint8_t> {data.begin(), data.end()});

        return BakingWriteResult::Changed;
    };

    auto context = safe_alloc::make_shared<BakingContext>(std::move(options));
    ImageBaker baker {context};
    map<string, size_t> decodes;
    baker.AddLoader(
        [&decodes](string_view path, string_view, FileReader reader, const ImageImport::Context&) {
            decodes[string {path}]++;
            ImageImport::FrameCollection collection;
            [[maybe_unused]] uint16_t sequence_size {};
            uint32_t duration_ms {100};
            bool have_dirs {};
            sequence_size = 1;
            duration_ms = 100;
            collection.Main.Frames.push_back(ImageImport::FrameShot {.Width = 1, .Height = 1, .Data = {reader.GetUInt8(), 0, 0, 255}});

            if (!have_dirs) {
                for (auto& direction : collection.Dirs) {
                    direction.Frames.clear();
                }
            }
            else {
                for (int32_t direction = GameSettings::MAP_DIR_COUNT - 1; direction < ImageImport::MAX_DIRS_MINUS_ONE; direction++) {
                    collection.Dirs[direction].Frames.clear();
                }
            }

            for (int32_t direction = 0; direction < (have_dirs ? GameSettings::MAP_DIR_COUNT : 1); direction++) {
                auto& frames = direction == 0 ? collection.Main : collection.Dirs[direction - 1];

                for (size_t index = 0; index < frames.Frames.size(); index++) {
                    auto& shot = frames.Frames[index];

                    if (shot.Shared && shot.SharedIndex < index) {
                        uint16_t reference = shot.SharedIndex;
                        shot = frames.Frames[reference];
                        shot.Data.clear();
                        shot.Shared = true;
                        shot.SharedIndex = reference;
                    }
                }

                ImageImport::SetFrameSequenceDuration(frames, duration_ms);
            }

            return collection;
        },
        {"toy"});
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    REQUIRE(decodes["A.toy"] == 1);
    REQUIRE(decodes["B.toy"] == 1);

    rig.Outputs.clear();
    rig.AddSourceFile("A.toy", vector<uint8_t> {20});

    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    CHECK(decodes["A.toy"] == 2);
    CHECK(decodes["B.toy"] == 1);
    CHECK(rig.Outputs.contains("A.fofrm"));
    CHECK_FALSE(rig.Outputs.contains("B.fofrm"));

    ImageOutputCandidate output {.Pack = "TestPack", .ResourcePath = "A.fofrm"};
    string cache_path = GetImageDependencyCachePath(rig.Settings, output);
    auto old_snapshot = fs::read_file(cache_path);
    REQUIRE(old_snapshot.has_value());

    rig.AddSourceFile("A.toy", vector<uint8_t> {22});
    fail_write = true;

    CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ImageBakerException);
    CHECK(fs::read_file(cache_path) == old_snapshot);

    fail_write = false;
    baker.BakeFiles(rig.GetAllSourceFiles(), "");

    auto recovered = ReadSpriteResource(rig.Outputs.at("A.fofrm"));
    CHECK(recovered.Directions[0].Frames[0].Pixels[0] == ucolor {22, 0, 0, 255});

    REQUIRE(fs::write_file(cache_path, string_view {"corrupt snapshot"}));
    size_t previous_decodes = decodes["A.toy"];
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    CHECK(decodes["A.toy"] == previous_decodes + 1);
    CHECK(decodes["B.toy"] == 1);
    baker.BakeFiles(rig.GetAllSourceFiles(), "");
    CHECK(decodes["A.toy"] == previous_decodes + 1);
}

TEST_CASE("ImageDependencyLazyMatchesFull")
{
    string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_lazy_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
    string output_dir = strex(temp_dir).combine_path("output").str();
    string source_path = strex(temp_dir).combine_path("input/Source.tga").str();
    string recipe_path = strex(temp_dir).combine_path("input/Root.fofrm").str();
    vector<uint8_t> pixels {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255};
    REQUIRE(fs::write_file(source_path, pixels));
    REQUIRE(fs::write_file(recipe_path, string_view {"Frame = Source.tga\nSize = 1 1\n"}));
    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    ConfigFile config {strex("Baking.BakeOutput = {}\nBaking.SingleThreadBaking = True\n[ResourcePack]\nName = Art\nInputDirs = input\nBakers = Image\n", output_dir).str()};
    settings.ApplyConfigFile(config, temp_dir);
    MasterBaker initial {&settings};
    REQUIRE(initial.BakeAll());
    auto data_source = safe_alloc::make_unique<BakerDataSource>(&settings);
    ptr<BakerDataSource> source = data_source;
    FileSystem resources;
    resources.AddCustomSource(std::move(data_source));
    auto source_time = std::filesystem::last_write_time(fs::make_path(source_path));
    auto recipe_time = std::filesystem::last_write_time(fs::make_path(recipe_path));
    pixels[19] = 255;
    pixels[20] = 0;
    REQUIRE(fs::write_file(source_path, pixels));
    REQUIRE(fs::write_file(recipe_path, string_view {"Frame = Source.tga\nSize = 2 1\n"}));
    std::filesystem::last_write_time(fs::make_path(source_path), source_time);
    std::filesystem::last_write_time(fs::make_path(recipe_path), recipe_time);
    REQUIRE(source->Reindex());
    File image_file = resources.ReadFile("Root.fofrm");
    REQUIRE(image_file);
    auto image = ReadSpriteResource(image_file.GetDataSpan());
    CHECK(image.Directions[0].Frames[0].Size == isize32 {2, 1});
    CHECK(image.Directions[0].Frames[0].Pixels[0] == ucolor {0, 255, 0, 255});
    string root_output = strex(output_dir).combine_path("Art/Root.fofrm").str();
    uint64_t root_write_time = fs::last_write_time(root_output);
    File info_file = resources.ReadFile("SpriteInfo/Art.foinfo");
    REQUIRE(info_file);
    auto info = ReadSpriteInfoFile("SpriteInfo/Art.foinfo", info_file.GetStr());
    REQUIRE(info.size() == 1);
    CHECK(info[0].Info.Directions[0].Frames[0].Size == isize32 {2, 1});
    CHECK(fs::last_write_time(root_output) == root_write_time);
    BakerTests::OverrideSetting(settings.Baking.ForceBaking, true);
    MasterBaker forced {&settings};
    REQUIRE(forced.BakeAll());
    auto full_output = fs::read_file(root_output);
    REQUIRE(full_output.has_value());
    CHECK(std::ranges::equal(make_const_span(*full_output), image_file.GetDataSpan()));
    CHECK(fs::read_file(strex(output_dir).combine_path("Art/SpriteInfo/Art.foinfo")) == info_file.GetStr());
}

#if FO_ENABLE_3D
TEST_CASE("ImageDependenciesExposeTextureMetadataBeforeModelBaking")
{
    string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_model_pipeline_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("metadata/Empty.cs"), string_view {"// Metadata fixture\n"}));
    REQUIRE(fs::write_file(strex(input_dir).combine_path("Source.tga"), vector<uint8_t> {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255}));
    REQUIRE(fs::write_file(strex(input_dir).combine_path("Root.fofrm"), string_view {"Frame = Source.tga\n"}));
    REQUIRE(fs::write_file(strex(input_dir).combine_path("Body.obj"), string_view {"o Body\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"}));
    REQUIRE(fs::write_file(strex(input_dir).combine_path("Model.fo3d"), string_view {"Model Body.obj\nTexture 0 Root.fofrm\n"}));
    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    ConfigFile config {strex("Baking.BakeOutput = {}\nBaking.SingleThreadBaking = True\n[ResourcePack]\nName = Metadata\nInputDirs = metadata\nBakers = Metadata\n[ResourcePack]\nName = Art\nInputDirs = input\nBakers = Image ModelMesh ModelInfo\n", output_dir).str()};
    settings.ApplyConfigFile(config, temp_dir);
    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    CHECK(fs::exists(strex(output_dir).combine_path("Art/Model.fo3d")));
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Art/Source.tga")));
}

TEST_CASE("ImageDependenciesInvalidateModelsWhenTheLastImageRootIsConsumed")
{
    string temp_dir = fs::path_to_string(std::filesystem::temp_directory_path() / std::format("fo_image_last_root_{}", std::chrono::steady_clock::now().time_since_epoch().count()));
    auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs::remove_dir_tree(temp_dir); });
    string output_dir = strex(temp_dir).combine_path("output").str();
    vector<uint8_t> pixels {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 0, 0, 255, 255};
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("metadata/Empty.cs"), string_view {"// Metadata fixture\n"}));
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("art/Source.tga"), pixels));
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("recipes/Other.tga"), pixels));
    string recipe = strex(temp_dir).combine_path("recipes/Root.fofrm").str();
    REQUIRE(fs::write_file(recipe, string_view {"Frame = Other.tga\n"}));
    REQUIRE(fs::write_file(strex(temp_dir).combine_path("models/Body.obj"), string_view {"o Body\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"}));
    string model = strex(temp_dir).combine_path("models/Model.fo3d").str();
    REQUIRE(fs::write_file(model, string_view {"Model Body.obj\nTexture 0 Source.tga\n"}));
    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();
    ConfigFile config {strex("Baking.BakeOutput = {}\nBaking.SingleThreadBaking = True\n[ResourcePack]\nName = Metadata\nInputDirs = metadata\nBakers = Metadata\n[ResourcePack]\nName = Art\nInputDirs = art\nBakers = Image\n[ResourcePack]\nName = Recipes\nInputDirs = recipes\nIncludePatterns = Root.fofrm\nBakers = Image\n[ResourcePack]\nName = Models\nInputDirs = models\nBakers = ModelMesh ModelInfo\n", output_dir).str()};
    settings.ApplyConfigFile(config, temp_dir);
    MasterBaker initial {&settings};
    REQUIRE(initial.BakeAll());
    REQUIRE(fs::exists(strex(output_dir).combine_path("Art/Source.tga")));
    REQUIRE(fs::write_file(recipe, string_view {"Frame = Source.tga\n"}));
    MasterBaker changed {&settings};
    CHECK_FALSE(changed.BakeAll());
    REQUIRE(fs::write_file(model, string_view {"Model Body.obj\nTexture 0 Root.fofrm\n"}));
    MasterBaker repaired {&settings};
    REQUIRE(repaired.BakeAll());
    CHECK_FALSE(fs::exists(strex(output_dir).combine_path("Art/Source.tga")));
    CHECK(fs::exists(strex(output_dir).combine_path("Models/Model.fo3d")));
}
#endif

FO_END_NAMESPACE
