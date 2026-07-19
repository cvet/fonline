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

FO_DISABLE_WARNINGS_PUSH()
#include <json.hpp>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static auto MakeTempBakerSetupDir(string_view name) -> string
{
    const auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(base);
}

static auto MakeBakerSetupReportPath(string_view output_dir) -> string
{
    FO_STACK_TRACE_ENTRY();

    const string normalized_output = strex(output_dir).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.report.json").str();
}

static auto MakeBakerSetupFullReportPath(string_view output_dir) -> string
{
    FO_STACK_TRACE_ENTRY();

    const string normalized_output = strex(output_dir).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.full.report.json").str();
}

static auto ReadBakerSetupReport(string_view output_dir) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    const auto report_data = fs_read_file(MakeBakerSetupReportPath(output_dir));
    REQUIRE(report_data.has_value());
    return nlohmann::json::parse(*report_data);
}

static auto FindBakerSetupReportEntry(const nlohmann::json& entries, string_view name) -> const nlohmann::json&
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(entries.is_array());
    const auto it = std::ranges::find_if(entries, [name](const nlohmann::json& entry) { return entry.at("name").get<std::string>() == name; });
    REQUIRE(it != entries.end());
    return *it;
}

static auto SumBakerSetupReportCounts(const nlohmann::json& entries) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(entries.is_array());
    uint64_t total = 0;
    for (const nlohmann::json& entry : entries) {
        total += entry.at("count").get<uint64_t>();
    }
    return total;
}

static auto MakeBakerSetupSpriteMeshTga() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint16_t width = 16;
    constexpr uint16_t height = 16;
    constexpr size_t header_size = 18;
    vector<uint8_t> data(header_size + numeric_cast<size_t>(width) * height * 4);
    data[2] = 2; // Uncompressed true-color image
    data[12] = numeric_cast<uint8_t>(width & 0xFF);
    data[13] = numeric_cast<uint8_t>(width >> 8);
    data[14] = numeric_cast<uint8_t>(height & 0xFF);
    data[15] = numeric_cast<uint8_t>(height >> 8);
    data[16] = 32;
    data[17] = 8; // Eight attribute bits carry alpha

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            const bool opaque = (x < 4 && y < 4) || (x >= width - 4 && y >= height - 4);
            const size_t pixel_offset = header_size + (numeric_cast<size_t>(y) * width + x) * 4;
            data[pixel_offset + 0] = 255;
            data[pixel_offset + 1] = 255;
            data[pixel_offset + 2] = 255;
            data[pixel_offset + 3] = opaque ? 255 : 0;
        }
    }

    return data;
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
    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/_scratch.json").str(), string_view {"scratch"}));
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
IncludePatterns = **/*.json
ExcludePatterns = **/_*.json
Bakers = {}
)",
            output_dir, RawCopyBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    BakerDataSource data_source {&settings};

    CHECK_FALSE(data_source.IsDiskDir());
    CHECK(data_source.GetPackName() == "Baker");
    CHECK(data_source.IsFileExists("Data/prebaked.json"));
    CHECK(data_source.IsFileExists("Data/runtime.json"));
    CHECK(data_source.IsFileExists("Data/stale.json"));
    CHECK(data_source.IsFileExists("Data/Nested/child.json"));
    CHECK_FALSE(data_source.IsFileExists("Data/_scratch.json"));
    CHECK_FALSE(data_source.IsFileExists("Data/readme.txt"));

    size_t size = 0;
    uint64_t write_time = 0;

    REQUIRE(data_source.GetFileInfo("Data/prebaked.json", size, write_time));
    CHECK(size == string_view {"cached-prebaked"}.size());
    CHECK(write_time == fs_last_write_time(prebaked_input_path));

    const auto prebaked_data = data_source.OpenFile("Data/prebaked.json", size, write_time);
    REQUIRE(prebaked_data);
    ptr<const uint8_t> prebaked_data_ptr = prebaked_data;
    CHECK(prebaked_data_ptr.reinterpret_as<char>().as_str(size) == "cached-prebaked");

    const string runtime_output_path = strex(output_dir).combine_path("Core/Data/runtime.json").str();
    CHECK_FALSE(fs_exists(runtime_output_path));

    const auto runtime_data = data_source.OpenFile("Data/runtime.json", size, write_time);
    REQUIRE(runtime_data);
    ptr<const uint8_t> runtime_data_ptr = runtime_data;
    CHECK(runtime_data_ptr.reinterpret_as<char>().as_str(size) == "runtime-source");
    REQUIRE(fs_read_file(runtime_output_path).has_value());
    CHECK(*fs_read_file(runtime_output_path) == "runtime-source");
    CHECK(write_time != 0);

    const auto stale_data = data_source.OpenFile("Data/stale.json", size, write_time);
    REQUIRE(stale_data);
    ptr<const uint8_t> stale_data_ptr = stale_data;
    CHECK(stale_data_ptr.reinterpret_as<char>().as_str(size) == "stale-source");
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
    const string excluded_source_path = strex(input_dir).combine_path("Data/_scratch.json").str();
    const string output_path = strex(output_dir).combine_path("Core/Data/keep.json").str();
    const string excluded_output_path = strex(output_dir).combine_path("Core/Data/_scratch.json").str();
    const string outdated_path = strex(output_dir).combine_path("Core/Data/obsolete.json").str();
    const string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();
    const string report_path = MakeBakerSetupReportPath(output_dir);
    const string full_report_path = MakeBakerSetupFullReportPath(output_dir);

    ignore_unused(fs_remove_dir_tree(temp_dir));

    REQUIRE(fs_write_file(source_path, string_view {"raw-copy"}));
    REQUIRE(fs_write_file(excluded_source_path, string_view {"scratch"}));
    REQUIRE(fs_write_file(outdated_path, string_view {"obsolete"}));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile("MasterBakerRawCopy.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputDirs = input
IncludePatterns = **/*.json
ExcludePatterns = **/_*.json
Bakers = {}
)",
            output_dir, RawCopyBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    MasterBaker first_baker {&settings};
    REQUIRE(first_baker.BakeAll());
    REQUIRE(fs_read_file(output_path).has_value());
    CHECK(*fs_read_file(output_path) == "raw-copy");
    CHECK_FALSE(fs_exists(excluded_output_path));
    CHECK_FALSE(fs_exists(outdated_path));
    CHECK(fs_read_file(build_hash_path).has_value());

    REQUIRE(fs_exists(report_path));
    REQUIRE(fs_exists(full_report_path));
    const auto full_report_data = fs_read_file(full_report_path);
    REQUIRE(full_report_data.has_value());
    const auto report_parent = std::filesystem::path {fs_make_path(report_path)}.parent_path();
    const auto output_path_object = std::filesystem::path {fs_make_path(output_dir)};
    CHECK(report_parent == output_path_object);

    const nlohmann::json first_report = ReadBakerSetupReport(output_dir);
    CHECK(first_report.at("schemaVersion") == 1);
    CHECK(first_report.at("status") == "success");
    CHECK(first_report.at("failureMessage") == "");
    CHECK(first_report.at("bakeOutput") == output_dir);
    CHECK(first_report.at("mode").at("forceRequested") == false);
    CHECK(first_report.at("mode").at("fullRebuild") == true);
    CHECK(first_report.at("mode").at("rebuildReason") == "missing_build_hash");
    CHECK(first_report.at("totals").at("packs") == 1);
    CHECK(first_report.at("totals").at("bakers") == 1);
    CHECK(first_report.at("totals").at("bakerRuns") == 1);
    CHECK(first_report.at("totals").at("inputFiles") == 1);
    CHECK(first_report.at("totals").at("outputsScheduled") == 1);
    CHECK(first_report.at("totals").at("outputsUpToDate") == 0);
    CHECK(first_report.at("totals").at("outputsSubmitted") == 1);
    CHECK(first_report.at("totals").at("filesChanged") == 1);

    const nlohmann::json& first_raw_copy = FindBakerSetupReportEntry(first_report.at("bakers"), RawCopyBaker::NAME);
    CHECK(first_raw_copy.at("status") == "success");
    CHECK(first_raw_copy.at("invocations") == 1);
    CHECK(first_raw_copy.at("successfulInvocations") == 1);
    CHECK(first_raw_copy.at("failedInvocations") == 0);
    CHECK(first_raw_copy.at("availableInputFiles") == 1);
    CHECK(first_raw_copy.at("outputs").at("checked").at("count") == 1);
    CHECK(first_raw_copy.at("outputs").at("scheduled").at("count") == 1);
    CHECK(first_raw_copy.at("outputs").at("upToDate").at("count") == 0);
    CHECK(first_raw_copy.at("outputs").at("submitted").at("count") == 1);
    CHECK(first_raw_copy.at("outputs").at("changed").at("count") == 1);

    const nlohmann::json& first_core = FindBakerSetupReportEntry(first_report.at("packs"), "Core");
    CHECK(first_core.at("inputs").at("count") == 1);
    CHECK(first_core.at("outputs").at("changed").at("count") == 1);
    CHECK(FindBakerSetupReportEntry(first_core.at("bakers"), RawCopyBaker::NAME).at("status") == "success");

    MasterBaker incremental_baker {&settings};
    REQUIRE(incremental_baker.BakeAll());
    CHECK(fs_read_file(full_report_path) == full_report_data);

    const nlohmann::json incremental_report = ReadBakerSetupReport(output_dir);
    CHECK(incremental_report.at("status") == "success");
    CHECK(incremental_report.at("mode").at("fullRebuild") == false);
    CHECK(incremental_report.at("mode").at("rebuildReason") == "incremental");
    CHECK(incremental_report.at("totals").at("outputsScheduled") == 0);
    CHECK(incremental_report.at("totals").at("outputsUpToDate") == 1);
    CHECK(incremental_report.at("totals").at("outputsSubmitted") == 0);
    CHECK(incremental_report.at("totals").at("filesChanged") == 0);

    const nlohmann::json& incremental_raw_copy = FindBakerSetupReportEntry(incremental_report.at("bakers"), RawCopyBaker::NAME);
    CHECK(incremental_raw_copy.at("outputs").at("checked").at("count") == 1);
    CHECK(incremental_raw_copy.at("outputs").at("scheduled").at("count") == 0);
    CHECK(incremental_raw_copy.at("outputs").at("upToDate").at("count") == 1);
    CHECK(incremental_raw_copy.at("outputs").at("cacheHitPercent").get<float64_t>() == 100.0);
    CHECK(incremental_raw_copy.at("outputs").at("submitCalls") == 0);

    const auto future_source_time = std::filesystem::file_time_type::clock::now() + std::chrono::minutes {1};
    SetBakerSetupFileWriteTime(source_path, future_source_time);
    REQUIRE(fs_last_write_time(source_path) > fs_last_write_time(output_path));
    const auto output_write_time_before_rebake = fs_last_write_time(output_path);

    MasterBaker stale_source_baker {&settings};
    REQUIRE(stale_source_baker.BakeAll());
    REQUIRE(fs_read_file(output_path).has_value());
    CHECK(*fs_read_file(output_path) == "raw-copy");
    CHECK(fs_last_write_time(output_path) >= output_write_time_before_rebake);
    CHECK(fs_read_file(build_hash_path).has_value());

    CHECK(fs_remove_dir_tree(temp_dir));
}

TEST_CASE("BakerResourcePacksCanSplitSharedInputDirectoryByGlob")
{
    const string temp_dir = MakeTempBakerSetupDir("resource_pack_glob_split");
    const string input_dir = strex(temp_dir).combine_path("input").str();
    const string output_dir = strex(temp_dir).combine_path("output").str();

    ignore_unused(fs_remove_dir_tree(temp_dir));

    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/json-keep.json").str(), string_view {"json"}));
    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/text-keep.json").str(), string_view {"text"}));
    REQUIRE(fs_write_file(strex(input_dir).combine_path("Data/private/secret.json").str(), string_view {"secret"}));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile("ResourcePackGlobSplit.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Json
InputDirs = input
IncludePatterns = **/json-*.json
ExcludePatterns = **/private/**
Bakers = {}
[ResourcePack]
Name = Text
InputDirs = input
IncludePatterns = **/text-*.json
Bakers = {}
)",
            output_dir, RawCopyBaker::NAME, RawCopyBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    CHECK(fs_read_file(strex(output_dir).combine_path("Json/Data/json-keep.json").str()).has_value());
    CHECK_FALSE(fs_exists(strex(output_dir).combine_path("Json/Data/text-keep.json").str()));
    CHECK_FALSE(fs_exists(strex(output_dir).combine_path("Json/Data/private/secret.json").str()));
    CHECK(fs_read_file(strex(output_dir).combine_path("Text/Data/text-keep.json").str()).has_value());
    CHECK_FALSE(fs_exists(strex(output_dir).combine_path("Text/Data/json-keep.json").str()));

    CHECK(fs_remove_dir_tree(temp_dir));
}

TEST_CASE("BakerMasterImageReport")
{
    const string temp_dir = MakeTempBakerSetupDir("master_baker_image_report");
    const string input_dir = strex(temp_dir).combine_path("input").str();
    const string output_dir = strex(temp_dir).combine_path("output").str();
    const string source_path = strex(input_dir).combine_path("gfx/report.tga").str();
    const string output_path = strex(output_dir).combine_path("Art/gfx/report.tga").str();

    ignore_unused(fs_remove_dir_tree(temp_dir));

    const vector<uint8_t> source_data = MakeBakerSetupSpriteMeshTga();
    REQUIRE(fs_write_file(source_path, {source_data.data(), source_data.size()}));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile("MasterBakerImageReport.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.ForceBaking = True
Baking.SingleThreadBaking = True
SpriteMesh.Enabled = True
SpriteMesh.AlphaThreshold = 0
SpriteMesh.MaxTriangles = 8
SpriteMesh.AreaSavingsWeight = 100.0
[ResourcePack]
Name = Art
InputDirs = input
Bakers = {}
)",
            output_dir, ImageBaker::NAME)
            .str());

    settings.ApplyConfigFile(config, temp_dir);

    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    REQUIRE(fs_exists(output_path));
    REQUIRE(fs_exists(strex(output_dir).combine_path("Art/SpriteInfo/Art.foinfo").str()));

    const nlohmann::json report = ReadBakerSetupReport(output_dir);
    CHECK(report.at("schemaVersion") == 1);
    CHECK(report.at("status") == "success");
    CHECK(report.at("totals").at("inputFiles") == 1);
    CHECK(report.at("totals").at("filesChanged") == 2);

    const nlohmann::json& image = FindBakerSetupReportEntry(report.at("bakers"), ImageBaker::NAME);
    REQUIRE(image.at("details").contains("spriteMesh"));
    const nlohmann::json& sprite_mesh = image.at("details").at("spriteMesh");
    CHECK(sprite_mesh.at("settings").at("enabled") == true);
    CHECK(sprite_mesh.at("settings").at("alphaThreshold") == 0);
    CHECK(sprite_mesh.at("settings").at("maxTriangles") == 8);
    CHECK(sprite_mesh.at("settings").at("baseDilation").is_number_integer());

    const nlohmann::json& frames = sprite_mesh.at("frames");
    const uint64_t unique_frames = frames.at("unique").get<uint64_t>();
    const uint64_t mesh_frames = frames.at("mesh").at("count").get<uint64_t>();
    const uint64_t quad_frames = frames.at("quad").at("count").get<uint64_t>();
    const uint64_t empty_frames = frames.at("empty").at("count").get<uint64_t>();
    CHECK(unique_frames == 1);
    CHECK(mesh_frames + quad_frames + empty_frames == unique_frames);

    const float64_t form_percent = frames.at("mesh").at("percent").get<float64_t>() + frames.at("quad").at("percent").get<float64_t>() + frames.at("empty").at("percent").get<float64_t>();
    CHECK(std::abs(form_percent - 100.0) < 0.000001);

    const nlohmann::json& triangle_histogram = sprite_mesh.at("triangleHistogram");
    const nlohmann::json& source_component_histogram = sprite_mesh.at("sourceComponentHistogram");
    const nlohmann::json& dilated_component_histogram = sprite_mesh.at("dilatedComponentHistogram");
    CHECK(triangle_histogram.is_array());
    CHECK(source_component_histogram.is_array());
    CHECK(dilated_component_histogram.is_array());
    CHECK(SumBakerSetupReportCounts(triangle_histogram) == mesh_frames);
    CHECK(SumBakerSetupReportCounts(source_component_histogram) == unique_frames);
    CHECK(SumBakerSetupReportCounts(dilated_component_histogram) <= unique_frames);

    const nlohmann::json& selection_score = sprite_mesh.at("selectionScore");
    CHECK((selection_score.at("minimum").is_number() || selection_score.at("minimum").is_null()));
    CHECK((selection_score.at("maximum").is_number() || selection_score.at("maximum").is_null()));
    CHECK(sprite_mesh.at("largestRejectedCandidateSavings").is_array());

    uint64_t mesh_triangles = 0;
    for (const nlohmann::json& entry : triangle_histogram) {
        mesh_triangles += entry.at("triangles").get<uint64_t>() * entry.at("count").get<uint64_t>();
    }
    const nlohmann::json& geometry = sprite_mesh.at("geometry");
    const uint64_t mesh_vertices = geometry.at("meshVertices").get<uint64_t>();
    CHECK(geometry.at("meshTriangles") == mesh_triangles);
    CHECK(geometry.at("submittedTriangles") == mesh_triangles + quad_frames * 2);
    CHECK(geometry.at("submittedVertices") == mesh_vertices + quad_frames * 4);

    const nlohmann::json& area = sprite_mesh.at("area");
    const uint64_t baseline_double_area = area.at("baselineQuadDoubleArea").get<uint64_t>();
    const uint64_t submitted_double_area = area.at("submittedGeometryDoubleArea").get<uint64_t>();
    const uint64_t visible_double_area = area.at("visibleDoubleArea").get<uint64_t>();
    CHECK(baseline_double_area == 16 * 16 * 2);
    CHECK(visible_double_area == 2 * 4 * 4 * 2);
    CHECK(submitted_double_area >= visible_double_area);
    CHECK(submitted_double_area <= baseline_double_area);
    CHECK(area.at("savedDoubleArea") == numeric_cast<int64_t>(baseline_double_area - submitted_double_area));

    const uint64_t source_texture_pixels = baseline_double_area / 2;
    const nlohmann::json& padding = sprite_mesh.at("padding");
    const uint64_t serialized_texture_pixels = padding.at("serializedTexturePixels").get<uint64_t>();
    const uint64_t expected_added_texture_pixels = serialized_texture_pixels > source_texture_pixels ? serialized_texture_pixels - source_texture_pixels : 0;
    const uint64_t expected_cropped_texture_pixels = source_texture_pixels > serialized_texture_pixels ? source_texture_pixels - serialized_texture_pixels : 0;
    CHECK(padding.at("addedTexturePixels") == expected_added_texture_pixels);
    CHECK(padding.at("framesExpanded") == (expected_added_texture_pixels != 0 ? 1 : 0));

    const nlohmann::json& cropping = sprite_mesh.at("cropping");
    CHECK(cropping.at("savedTexturePixels") == expected_cropped_texture_pixels);
    CHECK(cropping.at("savedTextureBytesRgba") == expected_cropped_texture_pixels * 4);
    CHECK(cropping.at("framesCropped") == (expected_cropped_texture_pixels != 0 ? 1 : 0));
    CHECK(sprite_mesh.at("largestCroppingSavings").is_array());

    const nlohmann::json& diagnostic_rows = mesh_frames != 0 ? sprite_mesh.at("mostComplexMeshes") : sprite_mesh.at("largestMissedSavings");
    REQUIRE(diagnostic_rows.size() == 1);
    const nlohmann::json& diagnostic_row = diagnostic_rows.front();
    CHECK((diagnostic_row.at("dilatedComponents").is_number_integer() || diagnostic_row.at("dilatedComponents").is_null()));
    CHECK((diagnostic_row.at("selectionScore").is_number() || diagnostic_row.at("selectionScore").is_null()));
    CHECK(diagnostic_row.at("croppingSavedPixels").is_number_unsigned());

    const nlohmann::json& art_pack = FindBakerSetupReportEntry(report.at("packs"), "Art");
    const nlohmann::json& pack_image = FindBakerSetupReportEntry(art_pack.at("bakers"), ImageBaker::NAME);
    CHECK(pack_image.at("details").at("spriteMesh").at("frames").at("unique") == unique_frames);

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
IncludePatterns = **
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {&settings};
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
IncludePatterns = **
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {&settings};
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
IncludePatterns = **
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir);

        MasterBaker baker {&settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(fs_read_file(build_hash_path).has_value());

        const string report_path = MakeBakerSetupReportPath(output_dir);
        REQUIRE(fs_exists(report_path));
        CHECK(std::filesystem::path {fs_make_path(report_path)}.parent_path() == std::filesystem::path {fs_make_path(output_dir)});

        const nlohmann::json report = ReadBakerSetupReport(output_dir);
        CHECK(report.at("schemaVersion") == 1);
        CHECK(report.at("status") == "failed");
        CHECK_FALSE(report.at("failureMessage").get<std::string>().empty());
        CHECK(report.at("totals").at("filesChanged") == 0);
        CHECK(report.at("bakers").is_array());
        CHECK(report.at("packs").is_array());

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

        MasterBaker baker {&settings};
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

        MasterBaker baker {&settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(fs_read_file(output_path).has_value());
        CHECK_FALSE(fs_read_file(build_hash_path).has_value());

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
