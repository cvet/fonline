//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/

#include "catch_amalgamated.hpp"

#include "AngelScriptBaker.h"
#include "ConfigBaker.h"
#include "ConfigFile.h"
#include "EffectBaker.h"
#include "ImageBaker.h"
#include "MapBaker.h"
#include "MetadataBaker.h"
#include "ModelInfoBaker.h"
#include "ModelMeshBaker.h"
#include "ProtoBaker.h"
#include "ProtoTextBaker.h"
#include "RawCopyBaker.h"
#include "Test_BakerHelpers.h"
#include "TextBaker.h"

FO_BEGIN_NAMESPACE

static auto MakeTempBakerSetupDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

static void SetBakerSetupFileWriteTime(string_view path, std::filesystem::file_time_type time)
{
    std::filesystem::last_write_time(std::filesystem::path {fs_make_path(path)}, time);
}

static auto CalcBakerSetupZipCrc32(string_view data) noexcept -> uint32_t
{
    uint32_t crc = 0xFFFFFFFF;

    for (const char ch : data) {
        crc ^= numeric_cast<uint8_t>(ch);

        for (size_t bit = 0; bit != 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320 : 0);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

static void AppendBakerSetupZipLe16(string& output, uint16_t value)
{
    const uint8_t byte0 = numeric_cast<uint8_t>(value & 0x00FF);
    const uint8_t byte1 = numeric_cast<uint8_t>((value >> 8) & 0x00FF);

    output.append(reinterpret_cast<const char*>(&byte0), sizeof(byte0));
    output.append(reinterpret_cast<const char*>(&byte1), sizeof(byte1));
}

static void AppendBakerSetupZipLe32(string& output, uint32_t value)
{
    const uint8_t byte0 = numeric_cast<uint8_t>(value & 0x000000FF);
    const uint8_t byte1 = numeric_cast<uint8_t>((value >> 8) & 0x000000FF);
    const uint8_t byte2 = numeric_cast<uint8_t>((value >> 16) & 0x000000FF);
    const uint8_t byte3 = numeric_cast<uint8_t>((value >> 24) & 0x000000FF);

    output.append(reinterpret_cast<const char*>(&byte0), sizeof(byte0));
    output.append(reinterpret_cast<const char*>(&byte1), sizeof(byte1));
    output.append(reinterpret_cast<const char*>(&byte2), sizeof(byte2));
    output.append(reinterpret_cast<const char*>(&byte3), sizeof(byte3));
}

static auto MakeBakerSetupStoredZip(string_view file_name, string_view file_content, uint32_t declared_size) -> string
{
    string zip;
    const uint16_t name_size = numeric_cast<uint16_t>(file_name.size());
    const uint32_t crc = CalcBakerSetupZipCrc32(file_content);

    AppendBakerSetupZipLe32(zip, 0x04034B50);
    AppendBakerSetupZipLe16(zip, 20);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe32(zip, crc);
    AppendBakerSetupZipLe32(zip, declared_size);
    AppendBakerSetupZipLe32(zip, declared_size);
    AppendBakerSetupZipLe16(zip, name_size);
    AppendBakerSetupZipLe16(zip, 0);
    zip.append(file_name);
    zip.append(file_content);

    const uint32_t central_dir_offset = numeric_cast<uint32_t>(zip.size());

    AppendBakerSetupZipLe32(zip, 0x02014B50);
    AppendBakerSetupZipLe16(zip, 20);
    AppendBakerSetupZipLe16(zip, 20);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe32(zip, crc);
    AppendBakerSetupZipLe32(zip, declared_size);
    AppendBakerSetupZipLe32(zip, declared_size);
    AppendBakerSetupZipLe16(zip, name_size);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe32(zip, 0);
    AppendBakerSetupZipLe32(zip, 0);
    zip.append(file_name);

    const uint32_t central_dir_size = numeric_cast<uint32_t>(zip.size() - central_dir_offset);

    AppendBakerSetupZipLe32(zip, 0x06054B50);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 0);
    AppendBakerSetupZipLe16(zip, 1);
    AppendBakerSetupZipLe16(zip, 1);
    AppendBakerSetupZipLe32(zip, central_dir_size);
    AppendBakerSetupZipLe32(zip, central_dir_offset);
    AppendBakerSetupZipLe16(zip, 0);

    return zip;
}

static auto MakeBakerSetupStoredZip(string_view file_name, string_view file_content) -> string
{
    return MakeBakerSetupStoredZip(file_name, file_content, numeric_cast<uint32_t>(file_content.size()));
}

TEST_CASE("BakerSetup")
{
    using namespace BakerTests;

    SECTION("ReturnsBakersInCanonicalSetupOrder")
    {
        TestRig rig;
        vector<string> requested_bakers {
            string(TextBaker::NAME),
            string(MapBaker::NAME),
            string(RawCopyBaker::NAME),
            string(ProtoBaker::NAME),
            string(ImageBaker::NAME),
            string(EffectBaker::NAME),
            string(ProtoTextBaker::NAME),
        };
        vector<string> expected_names {};

#if FO_ANGELSCRIPT_SCRIPTING
        requested_bakers.emplace_back(string(AngelScriptBaker::NAME));
        requested_bakers.emplace_back(string(ConfigBaker::NAME));
        requested_bakers.emplace_back(string(MetadataBaker::NAME));

        expected_names.emplace_back(string(MetadataBaker::NAME));
        expected_names.emplace_back(string(ConfigBaker::NAME));
#endif

        expected_names.emplace_back(string(RawCopyBaker::NAME));
        expected_names.emplace_back(string(ImageBaker::NAME));
        expected_names.emplace_back(string(EffectBaker::NAME));
        expected_names.emplace_back(string(ProtoBaker::NAME));
        expected_names.emplace_back(string(MapBaker::NAME));
        expected_names.emplace_back(string(TextBaker::NAME));
        expected_names.emplace_back(string(ProtoTextBaker::NAME));

#if FO_ENABLE_3D
        requested_bakers.emplace_back(string(ModelInfoBaker::NAME));
        requested_bakers.emplace_back(string(ModelMeshBaker::NAME));

        expected_names.emplace_back(string(ModelMeshBaker::NAME));
        expected_names.emplace_back(string(ModelInfoBaker::NAME));
#endif

#if FO_ANGELSCRIPT_SCRIPTING
        expected_names.emplace_back(string(AngelScriptBaker::NAME));
#endif

        const auto bakers = MakeRequestedBakers(requested_bakers, rig);

        REQUIRE(bakers.size() == expected_names.size());

        for (size_t i = 0; i < expected_names.size(); i++) {
            CHECK(bakers[i]->GetName() == expected_names[i]);
        }
    }

    SECTION("ReturnsNoBakersWhenRequestIsEmpty")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({}, rig);

        CHECK(bakers.empty());
    }

    SECTION("IgnoresUnknownBakerNames")
    {
        TestRig rig;
        const auto bakers = MakeRequestedBakers({"UnknownBaker", string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == RawCopyBaker::NAME);
    }
}

TEST_CASE("BakerDataSource")
{
    const string temp_dir = MakeTempBakerSetupDir("baker_data_source");
    const string input_dir = strex(temp_dir).combine_path("input").str();
    const string output_dir = strex(temp_dir).combine_path("output").str();
    const string prebaked_input_path = strex(input_dir).combine_path("Data/prebaked.json").str();
    const string prebaked_output_path = strex(output_dir).combine_path("Core/Data/prebaked.json").str();
    const string runtime_input_path = strex(input_dir).combine_path("Data/runtime.json").str();
    const string stale_input_path = strex(input_dir).combine_path("Data/stale.json").str();
    const string stale_output_path = strex(output_dir).combine_path("Core/Data/stale.json").str();

    ignore_unused(fs_remove_dir_tree(temp_dir));

    REQUIRE(fs_write_file(prebaked_input_path, string_view {"source-prebaked"}));
    std::this_thread::sleep_for(std::chrono::milliseconds {2});
    REQUIRE(fs_write_file(prebaked_output_path, string_view {"cached-prebaked"}));
    REQUIRE(fs_last_write_time(prebaked_input_path) <= fs_last_write_time(prebaked_output_path));

    REQUIRE(fs_write_file(runtime_input_path, string_view {"runtime-source"}));
    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/Nested/child.json").str(), string_view {"nested-source"}));
    REQUIRE(fs_write_file(stale_input_path, string_view {"stale-source"}));
    REQUIRE(fs_write_file(stale_output_path, string_view {"stale-output"}));
    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/readme.txt").str(), string_view {"ignored"}));

    const auto stale_base_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {5};
    SetBakerSetupFileWriteTime(runtime_input_path, stale_base_time);
    SetBakerSetupFileWriteTime(stale_output_path, stale_base_time);
    SetBakerSetupFileWriteTime(stale_input_path, stale_base_time + std::chrono::minutes {1});
    REQUIRE(fs_last_write_time(stale_input_path) > fs_last_write_time(stale_output_path));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile("BakerDataSource.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = input
RecursiveInput = true
Bakers = {}
)",
            output_dir, RawCopyBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    BakerDataSource data_source {settings};

    CHECK_FALSE(data_source.IsDiskDir());
    CHECK(data_source.GetPackName() == "Baker");
    CHECK(data_source.IsFileExists("Data/prebaked.json"));
    CHECK(data_source.IsFileExists("Data/runtime.json"));
    CHECK(data_source.IsFileExists("Data/stale.json"));
    CHECK(data_source.IsFileExists("Data/Nested/child.json"));
    CHECK_FALSE(data_source.IsFileExists("Data/readme.txt"));

    size_t size = 0;
    uint64_t write_time = 0;

    REQUIRE(data_source.GetFileInfo("Data/prebaked.json", size, write_time));
    CHECK(size == string_view {"cached-prebaked"}.size());
    CHECK(write_time == fs_last_write_time(prebaked_input_path));

    const auto prebaked_data = data_source.OpenFile("Data/prebaked.json", size, write_time);
    REQUIRE(prebaked_data);
    CHECK(string_view {reinterpret_cast<const char*>(prebaked_data.get()), size} == "cached-prebaked");

    const string runtime_output_path = strex(output_dir).combine_path("Core/Data/runtime.json").str();
    CHECK_FALSE(fs_exists(runtime_output_path));

    const auto runtime_data = data_source.OpenFile("Data/runtime.json", size, write_time);
    REQUIRE(runtime_data);
    CHECK(string_view {reinterpret_cast<const char*>(runtime_data.get()), size} == "runtime-source");
    REQUIRE(fs_read_file(runtime_output_path).has_value());
    CHECK(*fs_read_file(runtime_output_path) == "runtime-source");
    CHECK(write_time != 0);

    const auto stale_data = data_source.OpenFile("Data/stale.json", size, write_time);
    REQUIRE(stale_data);
    CHECK(string_view {reinterpret_cast<const char*>(stale_data.get()), size} == "stale-source");
    REQUIRE(fs_read_file(stale_output_path).has_value());
    CHECK(*fs_read_file(stale_output_path) == "stale-source");
    CHECK(write_time == fs_last_write_time(stale_input_path));

    CHECK_FALSE(data_source.GetFileInfo("Data/missing.json", size, write_time));
    CHECK_FALSE(data_source.OpenFile("Data/missing.json", size, write_time));

    const auto flat_json = data_source.GetFileNames("Data", false, "json");
    CHECK(flat_json.size() == 3);
    CHECK(std::ranges::find(flat_json, string {"Data/prebaked.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/runtime.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/stale.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/Nested/child.json"}) == flat_json.end());

    const auto recursive_json = data_source.GetFileNames("Data/", true, "json");
    CHECK(recursive_json.size() == 4);
    CHECK(std::ranges::find(recursive_json, string {"Data/Nested/child.json"}) != recursive_json.end());

    const auto flat_all = data_source.GetFileNames("Data", false, "");
    CHECK(flat_all.size() == 3);
    CHECK(data_source.GetFileNames("Other", true, "json").empty());
    CHECK(data_source.GetFileNames("Data/Nested/child/extra", true, "json").empty());
    CHECK(data_source.GetFileNames("Data", true, "txt").empty());

    CHECK(fs_remove_dir_tree(temp_dir));
}

TEST_CASE("BakerMasterRawCopy")
{
    const string temp_dir = MakeTempBakerSetupDir("master_baker_raw_copy");
    const string input_dir = strex(temp_dir).combine_path("input").str();
    const string output_dir = strex(temp_dir).combine_path("output").str();
    const string source_path = strex(input_dir).combine_path("Data/keep.json").str();
    const string output_path = strex(output_dir).combine_path("Core/Data/keep.json").str();
    const string outdated_path = strex(output_dir).combine_path("Core/Data/obsolete.json").str();
    const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

    ignore_unused(fs_remove_dir_tree(temp_dir));

    REQUIRE(fs_write_file(source_path, string_view {"raw-copy"}));
    REQUIRE(fs_write_file(outdated_path, string_view {"obsolete"}));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile("MasterBakerRawCopy.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = input
RecursiveInput = true
Bakers = {}
)",
            output_dir, RawCopyBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    MasterBaker first_baker {settings};
    REQUIRE(first_baker.BakeAll());
    REQUIRE(fs_read_file(output_path).has_value());
    CHECK(*fs_read_file(output_path) == "raw-copy");
    CHECK_FALSE(fs_exists(outdated_path));
    CHECK(fs_read_file(build_hash_path).has_value());

    const auto future_source_time = std::filesystem::file_time_type::clock::now() + std::chrono::minutes {1};
    SetBakerSetupFileWriteTime(source_path, future_source_time);
    REQUIRE(fs_last_write_time(source_path) > fs_last_write_time(output_path));
    const auto output_write_time_before_rebake = fs_last_write_time(output_path);

    MasterBaker second_baker {settings};
    REQUIRE(second_baker.BakeAll());
    REQUIRE(fs_read_file(output_path).has_value());
    CHECK(*fs_read_file(output_path) == "raw-copy");
    CHECK(fs_last_write_time(output_path) >= output_write_time_before_rebake);
    CHECK(fs_read_file(build_hash_path).has_value());

    CHECK(fs_remove_dir_tree(temp_dir));
}

TEST_CASE("BakerMasterRawCopyEdges")
{
    SECTION("Force baking deletes previous output")
    {
        const string temp_dir = MakeTempBakerSetupDir("master_baker_force_raw_copy");
        const string input_dir = strex(temp_dir).combine_path("input").str();
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string source_path = strex(input_dir).combine_path("Data/force.json").str();
        const string output_path = strex(output_dir).combine_path("Core/Data/force.json").str();
        const string outdated_path = strex(output_dir).combine_path("Core/Data/outdated.json").str();
        const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));

        REQUIRE(fs_write_file(source_path, string_view {"force-source"}));
        REQUIRE(fs_write_file(outdated_path, string_view {"outdated"}));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("MasterBakerRawCopyForce.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.ForceBaking = true
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = input
RecursiveInput = true
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(fs_read_file(output_path).has_value());
        CHECK(*fs_read_file(output_path) == "force-source");
        CHECK_FALSE(fs_exists(outdated_path));
        CHECK(fs_read_file(build_hash_path).has_value());

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("Build hash mismatch forces output rebuild")
    {
        const string temp_dir = MakeTempBakerSetupDir("master_baker_hash_raw_copy");
        const string input_dir = strex(temp_dir).combine_path("input").str();
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string source_path = strex(input_dir).combine_path("Data/hash.json").str();
        const string output_path = strex(output_dir).combine_path("Core/Data/hash.json").str();
        const string outdated_path = strex(output_dir).combine_path("Core/Data/outdated.json").str();
        const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));

        REQUIRE(fs_write_file(source_path, string_view {"hash-source"}));
        REQUIRE(fs_write_file(outdated_path, string_view {"outdated"}));
        REQUIRE(fs_write_file(build_hash_path, string_view {"old-build"}));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("MasterBakerRawCopyHash.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = input
RecursiveInput = true
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(fs_read_file(output_path).has_value());
        CHECK(*fs_read_file(output_path) == "hash-source");
        CHECK_FALSE(fs_exists(outdated_path));
        REQUIRE(fs_read_file(build_hash_path).has_value());
        CHECK(*fs_read_file(build_hash_path) != "old-build");

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("Missing input directory reports bake failure")
    {
        const string temp_dir = MakeTempBakerSetupDir("master_baker_missing_input");
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("MasterBakerRawCopyMissingInput.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = missing
RecursiveInput = true
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(fs_read_file(build_hash_path).has_value());

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

TEST_CASE("BakerMasterRawCopyPackInputs")
{
    SECTION("InputFiles zip pack can be baked asynchronously")
    {
        const string temp_dir = MakeTempBakerSetupDir("master_baker_input_file_pack");
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string pack_path = strex(temp_dir).combine_path("PackedInput.zip").str();
        const string output_path = strex(output_dir).combine_path("Core/Data/from_pack.json").str();
        const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));

        REQUIRE(fs_write_file(pack_path, MakeBakerSetupStoredZip("Data/from_pack.json", "packed-source")));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("MasterBakerRawCopyInputFilePack.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = false
[ResourcePack]
Name = Core
InputFiles = PackedInput.zip
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(fs_read_file(output_path).has_value());
        CHECK(*fs_read_file(output_path) == "packed-source");
        CHECK(fs_read_file(build_hash_path).has_value());

        CHECK(fs_remove_dir_tree(temp_dir));
    }

    SECTION("InputFiles read failure reports bake failure")
    {
        const string temp_dir = MakeTempBakerSetupDir("master_baker_input_file_pack_error");
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string pack_path = strex(temp_dir).combine_path("BrokenInput.zip").str();
        const string output_path = strex(output_dir).combine_path("Core/Data/broken.json").str();
        const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));

        REQUIRE(fs_write_file(pack_path, MakeBakerSetupStoredZip("Data/broken.json", "tiny", 32)));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("MasterBakerRawCopyInputFilePackError.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputFiles = BrokenInput.zip
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(fs_read_file(output_path).has_value());
        CHECK_FALSE(fs_read_file(build_hash_path).has_value());

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
