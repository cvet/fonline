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
#include "ParticleBaker.h"
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
    FO_STACK_TRACE_ENTRY();

    auto base = std::filesystem::temp_directory_path() / std::format("lf_{}_{}", name, std::chrono::steady_clock::now().time_since_epoch().count());
    u8string base_utf8 = fs_path_to_u8string(base);
    return utf8_to_char_string(base_utf8.view());
}

static auto MakeBakerSetupConfig(string_view name, string text) -> ConfigFile
{
    FO_STACK_TRACE_ENTRY();

    (void)name;
    u8string strict_text = text;
    return ConfigFile {std::move(strict_text)};
}

static auto RemoveBakerSetupDir(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_remove_dir_tree(path_utf8.view());
}

static auto WriteBakerSetupText(string_view path, u8string_view content) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_write_file_text(path_utf8.view(), content);
}

static auto ReadBakerSetupText(string_view path) -> optional<u8string>
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_read_file_text(path_utf8.view());
}

static auto WriteBakerSetupBytes(string_view path, const_span<byte> content) -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_write_file_bytes(path_utf8.view(), content);
}

static auto ReadBakerSetupBytes(string_view path) -> optional<vector<byte>>
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_read_file_bytes(path_utf8.view());
}

static auto BakerSetupLastWriteTime(string_view path) noexcept -> uint64_t
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_last_write_time(path_utf8.view());
}

static auto BakerSetupPathExists(string_view path) noexcept -> bool
{
    FO_NO_STACK_TRACE_ENTRY();

    u8string path_utf8 = path;
    return fs_exists(path_utf8.view());
}

static auto WriteBakerSetupBinaryFixture(string_view path, vector<byte> content) -> bool
{
    return WriteBakerSetupBytes(path, content);
}

static auto MakeBakerSetupReportPath(string_view output_dir) -> string
{
    FO_STACK_TRACE_ENTRY();

    string normalized_output = strex(output_dir).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.report.json").str();
}

static auto MakeBakerSetupFullReportPath(string_view output_dir) -> string
{
    FO_STACK_TRACE_ENTRY();

    string normalized_output = strex(output_dir).normalize_path_slashes().rtrim("/").str();
    return strex(normalized_output).combine_path("Baking.full.report.json").str();
}

static auto ReadBakerSetupReport(string_view output_dir) -> nlohmann::json
{
    FO_STACK_TRACE_ENTRY();

    auto report_data = ReadBakerSetupText(MakeBakerSetupReportPath(output_dir));
    REQUIRE(report_data.has_value());
    return nlohmann::json::parse(utf8_as_char_view(report_data->view()));
}

static auto FindBakerSetupReportEntry(const nlohmann::json& entries, string_view name) -> const nlohmann::json&
{
    FO_STACK_TRACE_ENTRY();

    REQUIRE(entries.is_array());
    auto it = std::ranges::find_if(entries, [name](const nlohmann::json& entry) { return entry.at("name").get<std::string>() == name; });
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

static auto MakeBakerSetupSpriteMeshTga() -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    constexpr uint16_t width = 16;
    constexpr uint16_t height = 16;
    constexpr size_t header_size = 18;
    vector<byte> data(header_size + numeric_cast<size_t>(width) * height * 4);
    data[2] = byte {2}; // Uncompressed true-color image
    data[12] = byte {numeric_cast<uint8_t>(width & 0xFF)};
    data[13] = byte {numeric_cast<uint8_t>(width >> 8)};
    data[14] = byte {numeric_cast<uint8_t>(height & 0xFF)};
    data[15] = byte {numeric_cast<uint8_t>(height >> 8)};
    data[16] = byte {32};
    data[17] = byte {8}; // Eight attribute bits carry alpha

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            bool opaque = (x < 4 && y < 4) || (x >= width - 4 && y >= height - 4);
            size_t pixel_offset = header_size + (numeric_cast<size_t>(y) * width + x) * 4;
            data[pixel_offset + 0] = byte {255};
            data[pixel_offset + 1] = byte {255};
            data[pixel_offset + 2] = byte {255};
            data[pixel_offset + 3] = byte {numeric_cast<uint8_t>(opaque ? 255 : 0)};
        }
    }

    return data;
}

static void SetBakerSetupFileWriteTime(string_view path, std::filesystem::file_time_type time)
{
    u8string path_utf8 = path;
    std::filesystem::last_write_time(std::filesystem::path {fs_make_path(path_utf8.view())}, time);
}

static auto CalcBakerSetupZipCrc32(const_span<byte> data) noexcept -> uint32_t
{
    uint32_t crc = 0xFFFFFFFF;

    for (byte value : data) {
        crc ^= std::to_integer<uint8_t>(value);

        for (size_t bit = 0; bit != 8; bit++) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xEDB88320 : 0);
        }
    }

    return crc ^ 0xFFFFFFFF;
}

static void AppendBakerSetupZipBytes(vector<byte>& output, const_span<byte> data)
{
    output.insert(output.end(), data.begin(), data.end());
}

static void AppendBakerSetupZipTextBytes(vector<byte>& output, string_view text)
{
    AppendBakerSetupZipBytes(output, make_byte_span(text));
}

static void AppendBakerSetupZipLe16(vector<byte>& output, uint16_t value)
{
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>(value & 0x00FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 8) & 0x00FF)));
}

static void AppendBakerSetupZipLe32(vector<byte>& output, uint32_t value)
{
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>(value & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 8) & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 16) & 0x000000FF)));
    output.emplace_back(static_cast<byte>(numeric_cast<uint8_t>((value >> 24) & 0x000000FF)));
}

static auto MakeBakerSetupStoredZip(string_view file_name, string_view file_content, uint32_t declared_size) -> vector<byte>
{
    vector<byte> zip;
    uint16_t name_size = numeric_cast<uint16_t>(file_name.size());
    uint32_t crc = CalcBakerSetupZipCrc32(make_byte_span(file_content));

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
    AppendBakerSetupZipTextBytes(zip, file_name);
    AppendBakerSetupZipTextBytes(zip, file_content);

    uint32_t central_dir_offset = numeric_cast<uint32_t>(zip.size());

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
    AppendBakerSetupZipTextBytes(zip, file_name);

    uint32_t central_dir_size = numeric_cast<uint32_t>(zip.size() - central_dir_offset);

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

static auto MakeBakerSetupStoredZip(string_view file_name, string_view file_content) -> vector<byte>
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

        auto bakers = MakeRequestedBakers(requested_bakers, rig);

        REQUIRE(bakers.size() == expected_names.size());

        for (size_t i = 0; i < expected_names.size(); i++) {
            CHECK(bakers[i]->GetName() == expected_names[i]);
        }
    }

    SECTION("ReturnsNoBakersWhenRequestIsEmpty")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({}, rig);

        CHECK(bakers.empty());
    }

    SECTION("IgnoresUnknownBakerNames")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({"UnknownBaker", string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 1);
        CHECK(bakers.front()->GetName() == RawCopyBaker::NAME);
    }

#if FO_ENABLE_3D && (FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES)
    SECTION("PreservesCrossPackBakerDependencyStages")
    {
        TestRig rig;
        auto bakers = MakeRequestedBakers({string(MapBaker::NAME), string(ProtoBaker::NAME), string(ModelInfoBaker::NAME), string(ParticleBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 4);

        auto find_order = [&bakers](string_view name) {
            auto it = std::ranges::find_if(bakers, [name](const unique_ptr<BaseBaker>& baker) { return baker->GetName() == name; });
            REQUIRE(it != bakers.end());
            return (*it)->GetOrder();
        };

        CHECK(find_order(ParticleBaker::NAME) < find_order(ModelInfoBaker::NAME));
        CHECK(find_order(ModelInfoBaker::NAME) < find_order(ProtoBaker::NAME));
        CHECK(find_order(ProtoBaker::NAME) < find_order(MapBaker::NAME));
    }
#endif
}

TEST_CASE("BakerDataSource")
{
    string temp_dir = MakeTempBakerSetupDir("baker_data_source");
    u8string temp_dir_utf8 = temp_dir;
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    string prebaked_input_path = strex(input_dir).combine_path("Data/prebaked.json").str();
    string prebaked_output_path = strex(output_dir).combine_path("Core/Data/prebaked.json").str();
    string runtime_input_path = strex(input_dir).combine_path("Data/runtime.json").str();
    string stale_input_path = strex(input_dir).combine_path("Data/stale.json").str();
    string stale_output_path = strex(output_dir).combine_path("Core/Data/stale.json").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    REQUIRE(WriteBakerSetupText(prebaked_input_path, u8"source-prebaked"));
    std::this_thread::sleep_for(std::chrono::milliseconds {2});
    REQUIRE(WriteBakerSetupText(prebaked_output_path, u8"cached-prebaked"));
    REQUIRE(BakerSetupLastWriteTime(prebaked_input_path) <= BakerSetupLastWriteTime(prebaked_output_path));

    REQUIRE(WriteBakerSetupText(runtime_input_path, u8"runtime-source"));
    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/Nested/child.json").str(), u8"nested-source"));
    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/_scratch.json").str(), u8"scratch"));
    REQUIRE(WriteBakerSetupText(stale_input_path, u8"stale-source"));
    REQUIRE(WriteBakerSetupText(stale_output_path, u8"stale-output"));
    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/readme.txt").str(), u8"ignored"));

    auto stale_base_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {5};
    SetBakerSetupFileWriteTime(runtime_input_path, stale_base_time);
    SetBakerSetupFileWriteTime(stale_output_path, stale_base_time);
    SetBakerSetupFileWriteTime(stale_input_path, stale_base_time + std::chrono::minutes {1});
    REQUIRE(BakerSetupLastWriteTime(stale_input_path) > BakerSetupLastWriteTime(stale_output_path));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = MakeBakerSetupConfig("BakerDataSource.fomain",
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

    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    BakerDataSource data_source {&settings};

    CHECK_FALSE(data_source.IsDiskDir());
    CHECK(data_source.GetSourcePath() == u8"Baker");
    CHECK(data_source.IsFileExists("Data/prebaked.json"));
    CHECK(data_source.IsFileExists("Data/runtime.json"));
    CHECK(data_source.IsFileExists("Data/stale.json"));
    CHECK(data_source.IsFileExists("Data/Nested/child.json"));
    CHECK_FALSE(data_source.IsFileExists("Data/_scratch.json"));
    CHECK_FALSE(data_source.IsFileExists("Data/readme.txt"));

    size_t size = 0;
    uint64_t write_time = 0;

    REQUIRE(data_source.GetFileInfo("Data/prebaked.json", size, write_time));
    CHECK(size == sizeof("cached-prebaked") - 1);
    CHECK(write_time == BakerSetupLastWriteTime(prebaked_input_path));

    auto prebaked_data = data_source.OpenFile("Data/prebaked.json", size, write_time);
    REQUIRE(prebaked_data);
    ptr<const byte> prebaked_data_ptr = prebaked_data;
    CHECK(prebaked_data_ptr.reinterpret_as<char>().as_str(size) == "cached-prebaked");

    string runtime_output_path = strex(output_dir).combine_path("Core/Data/runtime.json").str();
    CHECK_FALSE(BakerSetupPathExists(runtime_output_path));

    auto runtime_data = data_source.OpenFile("Data/runtime.json", size, write_time);
    REQUIRE(runtime_data);
    ptr<const byte> runtime_data_ptr = runtime_data;
    CHECK(runtime_data_ptr.reinterpret_as<char>().as_str(size) == "runtime-source");
    REQUIRE(ReadBakerSetupText(runtime_output_path).has_value());
    CHECK(*ReadBakerSetupText(runtime_output_path) == u8string {u8"runtime-source"});
    CHECK(write_time != 0);

    auto stale_data = data_source.OpenFile("Data/stale.json", size, write_time);
    REQUIRE(stale_data);
    ptr<const byte> stale_data_ptr = stale_data;
    CHECK(stale_data_ptr.reinterpret_as<char>().as_str(size) == "stale-source");
    REQUIRE(ReadBakerSetupText(stale_output_path).has_value());
    CHECK(*ReadBakerSetupText(stale_output_path) == u8string {u8"stale-source"});
    CHECK(write_time == BakerSetupLastWriteTime(stale_input_path));

    CHECK_FALSE(data_source.GetFileInfo("Data/missing.json", size, write_time));
    CHECK_FALSE(data_source.OpenFile("Data/missing.json", size, write_time));

    auto flat_json = data_source.GetFileNames("Data", false, "json");
    CHECK(flat_json.size() == 3);
    CHECK(std::ranges::find(flat_json, string {"Data/prebaked.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/runtime.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/stale.json"}) != flat_json.end());
    CHECK(std::ranges::find(flat_json, string {"Data/Nested/child.json"}) == flat_json.end());

    auto recursive_json = data_source.GetFileNames("Data/", true, "json");
    CHECK(recursive_json.size() == 4);
    CHECK(std::ranges::find(recursive_json, string {"Data/Nested/child.json"}) != recursive_json.end());

    auto flat_all = data_source.GetFileNames("Data", false, "");
    CHECK(flat_all.size() == 3);
    CHECK(data_source.GetFileNames("Other", true, "json").empty());
    CHECK(data_source.GetFileNames("Data/Nested/child/extra", true, "json").empty());
    CHECK(data_source.GetFileNames("Data", true, "txt").empty());

    CHECK(RemoveBakerSetupDir(temp_dir));
}

#if FO_ANGELSCRIPT_SCRIPTING && FO_ENABLE_3D
TEST_CASE("BakerDataSourceRegistersPackOutputsInDependencyOrder")
{
    using namespace BakerTests;

    string temp_dir = MakeTempBakerSetupDir("baker_data_source_dependency_order");
    string metadata_input_path = strex(temp_dir).combine_path("metadata_input/Metadata.fos").str();
    string metadata_output_path = strex(temp_dir).combine_path("output/Metadata/Metadata.fometa-client").str();
    string model_input_path = strex(temp_dir).combine_path("model_input/placeholder.txt").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    REQUIRE(WriteBakerSetupText(metadata_input_path, u8"void Placeholder() { }"));
    REQUIRE(WriteBakerSetupText(model_input_path, u8"placeholder"));

    auto source_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {2};
    SetBakerSetupFileWriteTime(metadata_input_path, source_time);
    REQUIRE(WriteBakerSetupBytes(metadata_output_path, MakeEmptyMetadataBlob()));
    SetBakerSetupFileWriteTime(metadata_output_path, source_time + std::chrono::minutes {1});

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = MakeBakerSetupConfig("BakerDataSourceDependencyOrder.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Metadata
InputDirs = metadata_input
IncludePatterns = **/*.fos
Bakers = {}
[ResourcePack]
Name = ModelInfo
InputDirs = model_input
IncludePatterns = **/*.fo3d
Bakers = {}
)",
            strex(temp_dir).combine_path("output").str(), MetadataBaker::NAME, ModelInfoBaker::NAME)
            .str());

    u8string temp_dir_utf8 = temp_dir;
    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    BakerDataSource data_source {&settings};

    CHECK(data_source.IsFileExists("Metadata.fometa-client"));
    CHECK(RemoveBakerSetupDir(temp_dir));
}

TEST_CASE("BakerDataSourceResolvesMetadataReadDuringModelInfoDiscovery")
{
    using namespace BakerTests;

    // Regression: a baker may read another baker's output while the data source is still discovering outputs.
    // ModelInfoBaker builds a BakerClientEngine during the discovery pass, which reads the baked metadata back
    // through the data source (re-entrancy). Reindex must publish the input resources before the discovery loop
    // and each discovered output to the live index as it goes, so this mid-loop on-demand read resolves; before
    // the fix it found neither and threw MetadataNotFoundException, crashing every standalone tool that boots a
    // BakerDataSource. A .fo3d input is required to make ModelInfoBaker build the engine at all - the plain
    // dependency-order case above uses a non-model placeholder, so ModelInfoBaker returns before that point.
    string temp_dir = MakeTempBakerSetupDir("baker_data_source_reentrant_metadata");
    string metadata_input_path = strex(temp_dir).combine_path("metadata_input/Metadata.fos").str();
    string model_desc_path = strex(temp_dir).combine_path("model_input/Test.fo3d").str();
    string model_mesh_path = strex(temp_dir).combine_path("model_input/Body.fbx").str();
    string metadata_output_dir = strex(temp_dir).combine_path("output/Metadata").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    REQUIRE(WriteBakerSetupText(metadata_input_path, u8"void Placeholder() { }"));
    REQUIRE(WriteBakerSetupText(model_desc_path, u8"Model Body.fbx\n"));
    REQUIRE(WriteBakerSetupText(model_mesh_path, u8"placeholder"));

    auto source_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {2};
    SetBakerSetupFileWriteTime(metadata_input_path, source_time);
    SetBakerSetupFileWriteTime(model_desc_path, source_time);
    SetBakerSetupFileWriteTime(model_mesh_path, source_time);

    // RegisterClientStubMetadata reads the server, client and mapper metadata; write all three newer than the
    // source so the re-entrant resolve returns them from disk instead of re-baking mid-discovery.
    array<string_view, 3> metadata_targets = {"server", "client", "mapper"};

    for (string_view target : metadata_targets) {
        string metadata_output_path = strex(metadata_output_dir).combine_path(strex("Metadata.fometa-{}", target).str()).str();
        REQUIRE(WriteBakerSetupBytes(metadata_output_path, MakeEmptyMetadataBlob()));
        SetBakerSetupFileWriteTime(metadata_output_path, source_time + std::chrono::minutes {1});
    }

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = ConfigFile(strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Metadata
InputDirs = metadata_input
IncludePatterns = **/*.fos
Bakers = {}
[ResourcePack]
Name = ModelInfo
InputDirs = model_input
IncludePatterns = **/*
Bakers = {}
)",
        strex(temp_dir).combine_path("output").str(), MetadataBaker::NAME, ModelInfoBaker::NAME)
            .str());

    u8string temp_dir_utf8 = temp_dir;
    settings.ApplyConfigFile(config, temp_dir_utf8);

    // Construction runs Reindex, whose output-discovery pass triggers the re-entrant metadata read.
    CHECK_NOTHROW(BakerDataSource {&settings});

    BakerDataSource data_source {&settings};

    CHECK(data_source.IsFileExists("Metadata.fometa-server"));
    CHECK(data_source.IsFileExists("Metadata.fometa-client"));
    CHECK(data_source.IsFileExists("Metadata.fometa-mapper"));
    CHECK(RemoveBakerSetupDir(temp_dir));
}
#endif

TEST_CASE("BakerDataSourcePrefersLaterResourcePack")
{
    string temp_dir = MakeTempBakerSetupDir("baker_data_source_pack_priority");
    string base_input_path = strex(temp_dir).combine_path("base_input/Data/shared.json").str();
    string override_input_path = strex(temp_dir).combine_path("override_input/Data/shared.json").str();
    string base_output_path = strex(temp_dir).combine_path("output/Base/Data/shared.json").str();
    string override_output_path = strex(temp_dir).combine_path("output/Override/Data/shared.json").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    REQUIRE(WriteBakerSetupText(base_input_path, u8"base-source"));
    REQUIRE(WriteBakerSetupText(override_input_path, u8"override-source"));
    REQUIRE(WriteBakerSetupText(base_output_path, u8"base-output"));
    REQUIRE(WriteBakerSetupText(override_output_path, u8"override-output"));

    auto base_time = std::filesystem::file_time_type::clock::now() - std::chrono::minutes {4};
    auto override_time = base_time + std::chrono::minutes {1};
    SetBakerSetupFileWriteTime(base_input_path, base_time);
    SetBakerSetupFileWriteTime(override_input_path, override_time);
    SetBakerSetupFileWriteTime(base_output_path, base_time + std::chrono::minutes {2});
    SetBakerSetupFileWriteTime(override_output_path, base_time + std::chrono::minutes {2});

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = MakeBakerSetupConfig("BakerDataSourcePackPriority.fomain",
        strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Base
InputDirs = base_input
IncludePatterns = **/*.json
Bakers = {}
[ResourcePack]
Name = Override
InputDirs = override_input
IncludePatterns = **/*.json
Bakers = {}
)",
            strex(temp_dir).combine_path("output").str(), RawCopyBaker::NAME, RawCopyBaker::NAME)
            .str());

    u8string temp_dir_utf8 = temp_dir;
    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    BakerDataSource data_source {&settings};
    size_t size = 0;
    uint64_t write_time = 0;
    auto data = data_source.OpenFile("Data/shared.json", size, write_time);

    REQUIRE(data);
    ptr<const byte> data_ptr = data;
    CHECK(data_ptr.reinterpret_as<char>().as_str(size) == "override-output");
    CHECK(write_time == BakerSetupLastWriteTime(override_input_path));
    CHECK(RemoveBakerSetupDir(temp_dir));
}

TEST_CASE("BakerMasterRawCopy")
{
    string temp_dir = MakeTempBakerSetupDir("master_baker_raw_copy");
    u8string temp_dir_utf8 = temp_dir;
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    string source_path = strex(input_dir).combine_path("Data/keep.json").str();
    string excluded_source_path = strex(input_dir).combine_path("Data/_scratch.json").str();
    string output_path = strex(output_dir).combine_path("Core/Data/keep.json").str();
    string excluded_output_path = strex(output_dir).combine_path("Core/Data/_scratch.json").str();
    string outdated_path = strex(output_dir).combine_path("Core/Data/obsolete.json").str();
    string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();
    string report_path = MakeBakerSetupReportPath(output_dir);
    string full_report_path = MakeBakerSetupFullReportPath(output_dir);

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    vector<byte> source_bytes = {byte {0x00}, byte {0x80}, byte {0xFF}};
    REQUIRE(WriteBakerSetupBytes(source_path, source_bytes));
    REQUIRE(WriteBakerSetupText(excluded_source_path, u8"scratch"));
    REQUIRE(WriteBakerSetupText(outdated_path, u8"obsolete"));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = MakeBakerSetupConfig("MasterBakerRawCopy.fomain",
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

    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    MasterBaker first_baker {&settings};
    REQUIRE(first_baker.BakeAll());
    REQUIRE(ReadBakerSetupBytes(output_path).has_value());
    CHECK(*ReadBakerSetupBytes(output_path) == source_bytes);
    CHECK_FALSE(BakerSetupPathExists(excluded_output_path));
    CHECK_FALSE(BakerSetupPathExists(outdated_path));
    CHECK(ReadBakerSetupText(build_hash_path).has_value());

    REQUIRE(BakerSetupPathExists(report_path));
    REQUIRE(BakerSetupPathExists(full_report_path));
    auto full_report_data = ReadBakerSetupText(full_report_path);
    REQUIRE(full_report_data.has_value());
    u8string report_path_utf8 = report_path;
    u8string output_dir_utf8 = output_dir;
    auto report_parent = std::filesystem::path {fs_make_path(report_path_utf8.view())}.parent_path();
    auto output_path_object = std::filesystem::path {fs_make_path(output_dir_utf8.view())};
    CHECK(report_parent == output_path_object);

    nlohmann::json first_report = ReadBakerSetupReport(output_dir);
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
    CHECK(ReadBakerSetupText(full_report_path) == full_report_data);

    nlohmann::json incremental_report = ReadBakerSetupReport(output_dir);
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

    auto future_source_time = std::filesystem::file_time_type::clock::now() + std::chrono::minutes {1};
    SetBakerSetupFileWriteTime(source_path, future_source_time);
    REQUIRE(BakerSetupLastWriteTime(source_path) > BakerSetupLastWriteTime(output_path));
    auto output_write_time_before_rebake = BakerSetupLastWriteTime(output_path);

    MasterBaker stale_source_baker {&settings};
    REQUIRE(stale_source_baker.BakeAll());
    REQUIRE(ReadBakerSetupBytes(output_path).has_value());
    CHECK(*ReadBakerSetupBytes(output_path) == source_bytes);
    CHECK(BakerSetupLastWriteTime(output_path) >= output_write_time_before_rebake);
    CHECK(ReadBakerSetupText(build_hash_path).has_value());

    CHECK(RemoveBakerSetupDir(temp_dir));
}

TEST_CASE("BakerResourcePacksCanSplitSharedInputDirectoryByGlob")
{
    string temp_dir = MakeTempBakerSetupDir("resource_pack_glob_split");
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/json-keep.json").str(), u8"json"));
    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/text-keep.json").str(), u8"text"));
    REQUIRE(WriteBakerSetupText(strex(input_dir).combine_path("Data/private/secret.json").str(), u8"secret"));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    u8string config_text = strex(R"(Baking.BakeOutput = {}
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
        output_dir, RawCopyBaker::NAME, RawCopyBaker::NAME);
    auto config = ConfigFile(std::move(config_text));

    u8string temp_dir_utf8 = temp_dir;
    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    CHECK(ReadBakerSetupText(strex(output_dir).combine_path("Json/Data/json-keep.json").str()).has_value());
    CHECK_FALSE(BakerSetupPathExists(strex(output_dir).combine_path("Json/Data/text-keep.json").str()));
    CHECK_FALSE(BakerSetupPathExists(strex(output_dir).combine_path("Json/Data/private/secret.json").str()));
    CHECK(ReadBakerSetupText(strex(output_dir).combine_path("Text/Data/text-keep.json").str()).has_value());
    CHECK_FALSE(BakerSetupPathExists(strex(output_dir).combine_path("Text/Data/json-keep.json").str()));

    CHECK(RemoveBakerSetupDir(temp_dir));
}

TEST_CASE("BakerMasterImageReport")
{
    string temp_dir = MakeTempBakerSetupDir("master_baker_image_report");
    string input_dir = strex(temp_dir).combine_path("input").str();
    string output_dir = strex(temp_dir).combine_path("output").str();
    string source_path = strex(input_dir).combine_path("gfx/report.tga").str();
    string output_path = strex(output_dir).combine_path("Art/gfx/report.tga").str();

    ignore_unused(RemoveBakerSetupDir(temp_dir));

    vector<byte> source_data = MakeBakerSetupSpriteMeshTga();
    REQUIRE(WriteBakerSetupBytes(source_path, source_data));

    GlobalSettings settings {true};
    settings.ApplyDefaultSettings();

    auto config = MakeBakerSetupConfig("MasterBakerImageReport.fomain",
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

    u8string temp_dir_utf8 = temp_dir;
    settings.ApplyConfigFile(config, temp_dir_utf8.view());

    MasterBaker baker {&settings};
    REQUIRE(baker.BakeAll());
    REQUIRE(BakerSetupPathExists(output_path));
    REQUIRE(BakerSetupPathExists(strex(output_dir).combine_path("Art/SpriteInfo/Art.foinfo").str()));

    nlohmann::json report = ReadBakerSetupReport(output_dir);
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
    uint64_t unique_frames = frames.at("unique").get<uint64_t>();
    uint64_t mesh_frames = frames.at("mesh").at("count").get<uint64_t>();
    uint64_t quad_frames = frames.at("quad").at("count").get<uint64_t>();
    uint64_t empty_frames = frames.at("empty").at("count").get<uint64_t>();
    CHECK(unique_frames == 1);
    CHECK(mesh_frames + quad_frames + empty_frames == unique_frames);

    float64_t form_percent = frames.at("mesh").at("percent").get<float64_t>() + frames.at("quad").at("percent").get<float64_t>() + frames.at("empty").at("percent").get<float64_t>();
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
    uint64_t mesh_vertices = geometry.at("meshVertices").get<uint64_t>();
    CHECK(geometry.at("meshTriangles") == mesh_triangles);
    CHECK(geometry.at("submittedTriangles") == mesh_triangles + quad_frames * 2);
    CHECK(geometry.at("submittedVertices") == mesh_vertices + quad_frames * 4);

    const nlohmann::json& area = sprite_mesh.at("area");
    uint64_t baseline_double_area = area.at("baselineQuadDoubleArea").get<uint64_t>();
    uint64_t submitted_double_area = area.at("submittedGeometryDoubleArea").get<uint64_t>();
    uint64_t visible_double_area = area.at("visibleDoubleArea").get<uint64_t>();
    CHECK(baseline_double_area == 16 * 16 * 2);
    CHECK(visible_double_area == 2 * 4 * 4 * 2);
    CHECK(submitted_double_area >= visible_double_area);
    CHECK(submitted_double_area <= baseline_double_area);
    CHECK(area.at("savedDoubleArea") == numeric_cast<int64_t>(baseline_double_area - submitted_double_area));

    uint64_t source_texture_pixels = baseline_double_area / 2;
    const nlohmann::json& padding = sprite_mesh.at("padding");
    uint64_t serialized_texture_pixels = padding.at("serializedTexturePixels").get<uint64_t>();
    uint64_t expected_added_texture_pixels = serialized_texture_pixels > source_texture_pixels ? serialized_texture_pixels - source_texture_pixels : 0;
    uint64_t expected_cropped_texture_pixels = source_texture_pixels > serialized_texture_pixels ? source_texture_pixels - serialized_texture_pixels : 0;
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

    BakerTests::OverrideSetting(settings.ForceBaking, false);
    MasterBaker incremental_baker {&settings};
    REQUIRE(incremental_baker.BakeAll());
    REQUIRE(BakerSetupPathExists(strex(output_dir).combine_path("Art/SpriteInfo/Art.foinfo").str()));

    nlohmann::json incremental_report = ReadBakerSetupReport(output_dir);
    CHECK(incremental_report.at("status") == "success");
    CHECK(incremental_report.at("totals").at("filesChanged") == 0);

    CHECK(RemoveBakerSetupDir(temp_dir));
}

TEST_CASE("BakerMasterRawCopyEdges")
{
    SECTION("Force baking deletes previous output")
    {
        string temp_dir = MakeTempBakerSetupDir("master_baker_force_raw_copy");
        u8string temp_dir_utf8 = temp_dir;
        string input_dir = strex(temp_dir).combine_path("input").str();
        string output_dir = strex(temp_dir).combine_path("output").str();
        string source_path = strex(input_dir).combine_path("Data/force.json").str();
        string output_path = strex(output_dir).combine_path("Core/Data/force.json").str();
        string outdated_path = strex(output_dir).combine_path("Core/Data/outdated.json").str();
        string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(RemoveBakerSetupDir(temp_dir));

        REQUIRE(WriteBakerSetupText(source_path, u8"force-source"));
        REQUIRE(WriteBakerSetupText(outdated_path, u8"outdated"));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = MakeBakerSetupConfig("MasterBakerRawCopyForce.fomain",
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

        settings.ApplyConfigFile(config, temp_dir_utf8.view());

        MasterBaker baker {&settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(ReadBakerSetupText(output_path).has_value());
        CHECK(*ReadBakerSetupText(output_path) == u8string {u8"force-source"});
        CHECK_FALSE(BakerSetupPathExists(outdated_path));
        CHECK(ReadBakerSetupText(build_hash_path).has_value());

        CHECK(RemoveBakerSetupDir(temp_dir));
    }

    SECTION("Build hash mismatch forces output rebuild")
    {
        string temp_dir = MakeTempBakerSetupDir("master_baker_hash_raw_copy");
        u8string temp_dir_utf8 = temp_dir;
        string input_dir = strex(temp_dir).combine_path("input").str();
        string output_dir = strex(temp_dir).combine_path("output").str();
        string source_path = strex(input_dir).combine_path("Data/hash.json").str();
        string output_path = strex(output_dir).combine_path("Core/Data/hash.json").str();
        string outdated_path = strex(output_dir).combine_path("Core/Data/outdated.json").str();
        string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(RemoveBakerSetupDir(temp_dir));

        REQUIRE(WriteBakerSetupText(source_path, u8"hash-source"));
        REQUIRE(WriteBakerSetupText(outdated_path, u8"outdated"));
        REQUIRE(WriteBakerSetupText(build_hash_path, u8"old-build"));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = MakeBakerSetupConfig("MasterBakerRawCopyHash.fomain",
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

        settings.ApplyConfigFile(config, temp_dir_utf8.view());

        MasterBaker baker {&settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(ReadBakerSetupText(output_path).has_value());
        CHECK(*ReadBakerSetupText(output_path) == u8string {u8"hash-source"});
        CHECK_FALSE(BakerSetupPathExists(outdated_path));
        REQUIRE(ReadBakerSetupText(build_hash_path).has_value());
        CHECK(*ReadBakerSetupText(build_hash_path) != u8string {u8"old-build"});

        CHECK(RemoveBakerSetupDir(temp_dir));
    }

    SECTION("Missing input directory reports bake failure")
    {
        string temp_dir = MakeTempBakerSetupDir("master_baker_missing_input");
        u8string temp_dir_utf8 = temp_dir;
        string output_dir = strex(temp_dir).combine_path("output").str();
        string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(RemoveBakerSetupDir(temp_dir));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = MakeBakerSetupConfig("MasterBakerRawCopyMissingInput.fomain",
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

        settings.ApplyConfigFile(config, temp_dir_utf8.view());

        MasterBaker baker {&settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(ReadBakerSetupText(build_hash_path).has_value());

        string report_path = MakeBakerSetupReportPath(output_dir);
        REQUIRE(BakerSetupPathExists(report_path));
        u8string report_path_utf8 = report_path;
        u8string output_dir_utf8 = output_dir;
        CHECK(std::filesystem::path {fs_make_path(report_path_utf8.view())}.parent_path() == std::filesystem::path {fs_make_path(output_dir_utf8.view())});

        nlohmann::json report = ReadBakerSetupReport(output_dir);
        CHECK(report.at("schemaVersion") == 1);
        CHECK(report.at("status") == "failed");
        CHECK_FALSE(report.at("failureMessage").get<std::string>().empty());
        CHECK(report.at("totals").at("filesChanged") == 0);
        CHECK(report.at("bakers").is_array());
        CHECK(report.at("packs").is_array());

        CHECK(RemoveBakerSetupDir(temp_dir));
    }
}

TEST_CASE("BakerMasterRawCopyPackInputs")
{
    SECTION("InputFiles zip pack can be baked asynchronously")
    {
        string temp_dir = MakeTempBakerSetupDir("master_baker_input_file_pack");
        u8string temp_dir_utf8 = temp_dir;
        string output_dir = strex(temp_dir).combine_path("output").str();
        string pack_path = strex(temp_dir).combine_path("PackedInput.zip").str();
        string output_path = strex(output_dir).combine_path("Core/Data/from_pack.json").str();
        string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(RemoveBakerSetupDir(temp_dir));

        REQUIRE(WriteBakerSetupBinaryFixture(pack_path, MakeBakerSetupStoredZip("Data/from_pack.json", "packed-source")));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = MakeBakerSetupConfig("MasterBakerRawCopyInputFilePack.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = false
[ResourcePack]
Name = Core
InputFiles = PackedInput.zip
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir_utf8.view());

        MasterBaker baker {&settings};
        REQUIRE(baker.BakeAll());
        REQUIRE(ReadBakerSetupText(output_path).has_value());
        CHECK(*ReadBakerSetupText(output_path) == u8string {u8"packed-source"});
        CHECK(ReadBakerSetupText(build_hash_path).has_value());

        CHECK(RemoveBakerSetupDir(temp_dir));
    }

    SECTION("InputFiles read failure reports bake failure")
    {
        string temp_dir = MakeTempBakerSetupDir("master_baker_input_file_pack_error");
        u8string temp_dir_utf8 = temp_dir;
        string output_dir = strex(temp_dir).combine_path("output").str();
        string pack_path = strex(temp_dir).combine_path("BrokenInput.zip").str();
        string output_path = strex(output_dir).combine_path("Core/Data/broken.json").str();
        string build_hash_path = strex(output_dir).combine_path("Resources.build-hash").str();

        ignore_unused(RemoveBakerSetupDir(temp_dir));

        REQUIRE(WriteBakerSetupBinaryFixture(pack_path, MakeBakerSetupStoredZip("Data/broken.json", "tiny", 32)));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = MakeBakerSetupConfig("MasterBakerRawCopyInputFilePackError.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Core
InputFiles = BrokenInput.zip
Bakers = {}
)",
                output_dir, RawCopyBaker::NAME)
                .str());

        settings.ApplyConfigFile(config, temp_dir_utf8.view());

        MasterBaker baker {&settings};
        CHECK_FALSE(baker.BakeAll());
        CHECK_FALSE(ReadBakerSetupBytes(output_path).has_value());
        CHECK_FALSE(ReadBakerSetupText(build_hash_path).has_value());

        CHECK(RemoveBakerSetupDir(temp_dir));
    }
}

FO_END_NAMESPACE
