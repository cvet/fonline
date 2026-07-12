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

#include "ConfigFile.h"
#include "ParticleBaker.h"
#include "ProtoBaker.h"
#include "RawCopyBaker.h"
#include "SparkExtension.h"
#include "Test_BakerHelpers.h"

FO_BEGIN_NAMESPACE

static constexpr string_view ValidParticle = R"PARTICLE(
<SPARK>
  <System name="UnitTestParticle">
    <attrib id="groups">
      <Group name="UnitTestGroup">
        <attrib id="capacity" value="4" />
        <attrib id="life time" value="1;1" />
        <attrib id="emitters">
          <StaticEmitter>
            <attrib id="tank" value="1" />
            <attrib id="flow" value="-1" />
            <attrib id="force" value="0" />
            <attrib id="zone">
              <Point>
                <attrib id="position" value="(0,0,0)" />
              </Point>
            </attrib>
            <attrib id="full" value="false" />
          </StaticEmitter>
        </attrib>
        <attrib id="renderer">
          <SparkQuadRenderer>
            <attrib id="draw size" value="64;48" />
            <attrib id="draw in scene" value="true" />
            <attrib id="active" value="true" />
            <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
            <attrib id="texture" value="Particles/TestParticle.png" />
            <attrib id="scale" value="1.5;2" />
            <attrib id="atlas dimensions" value="2;3" />
          </SparkQuadRenderer>
        </attrib>
      </Group>
    </attrib>
  </System>
</SPARK>
)PARTICLE";

static constexpr string_view MalformedParticle = R"(<SPARK><System name="Broken">)";

static constexpr string_view UnknownObjectParticle = R"PARTICLE(
<SPARK>
  <System name="UnknownObjectParticle">
    <attrib id="groups">
      <UnknownParticleGroup />
    </attrib>
  </System>
</SPARK>
)PARTICLE";

static auto MakeTempParticleBakerDir() -> string
{
    FO_STACK_TRACE_ENTRY();

    const std::filesystem::path path = std::filesystem::temp_directory_path() / std::format("fo_particle_baker_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_string(path);
}

static auto BakeValidParticleBinary() -> vector<uint8_t>
{
    FO_STACK_TRACE_ENTRY();

    BakerTests::TestRig rig;
    rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 10);

    ParticleBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), "");

    FO_VERIFY_AND_THROW(rig.Outputs.contains("Particles/UnitTest.fopts"), "Particle test fixture was not baked");
    return rig.Outputs.at("Particles/UnitTest.fopts");
}

enum class ParticleReferenceMutation
{
    Zero,
    OutOfRange,
    WrongType,
};

static void MutateSystemGroupsReference(vector<uint8_t>& binary, ParticleReferenceMutation mutation)
{
    FO_STACK_TRACE_ENTRY();

    const auto require_range = [&binary](size_t offset, size_t length, size_t limit) { FO_VERIFY_AND_THROW(limit <= binary.size() && offset <= limit && length <= limit - offset, "Particle binary fixture has an invalid range"); };
    const auto read_uint32 = [&binary, &require_range](size_t offset, size_t limit) -> uint32_t {
        require_range(offset, sizeof(uint32_t), limit);
        return uint32_t {binary[offset]} | (uint32_t {binary[offset + 1]} << 8) | (uint32_t {binary[offset + 2]} << 16) | (uint32_t {binary[offset + 3]} << 24);
    };
    const auto read_string = [&binary](size_t& offset, size_t limit) -> string {
        string value;
        while (offset < limit) {
            const uint8_t ch = binary[offset++];
            if (ch == 0) {
                return value;
            }
            value += numeric_cast<char>(ch);
        }

        FO_VERIFY_AND_THROW(false, "Particle binary fixture has an unterminated string");
        return {};
    };
    const auto read_bool = [&binary, &require_range](size_t& offset, size_t limit) -> bool {
        require_range(offset, sizeof(uint8_t), limit);
        return binary[offset++] != 0;
    };
    const auto write_uint32 = [&binary, &require_range](size_t offset, uint32_t value) {
        require_range(offset, sizeof(uint32_t), binary.size());
        binary[offset] = numeric_cast<uint8_t>(value & 0xFFU);
        binary[offset + 1] = numeric_cast<uint8_t>((value >> 8) & 0xFFU);
        binary[offset + 2] = numeric_cast<uint8_t>((value >> 16) & 0xFFU);
        binary[offset + 3] = numeric_cast<uint8_t>((value >> 24) & 0xFFU);
    };

    require_range(0, 12, binary.size());
    FO_VERIFY_AND_THROW(binary[0] == uint8_t {'S'} && binary[1] == uint8_t {'P'} && binary[2] == uint8_t {'K'} && binary[3] == 0, "Particle binary fixture has an invalid header");

    const uint32_t payload_size = read_uint32(4, binary.size());
    const uint32_t object_count = read_uint32(8, binary.size());
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(payload_size) == binary.size() - 12, "Particle binary fixture has an invalid payload size");

    size_t reference_offset = string::npos;
    uint32_t original_reference = 0;
    uint32_t system_reference = 0;
    size_t position = 12;

    for (uint32_t object_index = 0; object_index < object_count; ++object_index) {
        const string object_type = read_string(position, binary.size());
        const uint32_t object_data_size = read_uint32(position, binary.size());
        position += sizeof(uint32_t);
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(object_data_size) >= sizeof(uint32_t) && numeric_cast<size_t>(object_data_size) <= binary.size() - position, "Particle binary fixture has an invalid object record");

        const size_t object_end = position + numeric_cast<size_t>(object_data_size);
        if (object_type == "System") {
            FO_VERIFY_AND_THROW(reference_offset == string::npos, "Particle binary fixture has multiple System objects");

            size_t attribute_position = position + sizeof(uint32_t);
            require_range(position, sizeof(uint32_t), object_end);

            const bool name_defined = read_bool(attribute_position, object_end);
            if (name_defined) {
                (void)read_string(attribute_position, object_end);
            }

            const bool shared_defined = read_bool(attribute_position, object_end);
            if (shared_defined) {
                require_range(attribute_position, sizeof(uint8_t), object_end);
                attribute_position += sizeof(uint8_t);
            }

            const bool transform_defined = read_bool(attribute_position, object_end);
            if (transform_defined) {
                const uint32_t transform_value_count = read_uint32(attribute_position, object_end);
                attribute_position += sizeof(uint32_t);
                FO_VERIFY_AND_THROW(numeric_cast<size_t>(transform_value_count) <= (object_end - attribute_position) / sizeof(float32_t), "Particle binary fixture has invalid transform data");
                attribute_position += numeric_cast<size_t>(transform_value_count) * sizeof(float32_t);
            }

            FO_VERIFY_AND_THROW(read_bool(attribute_position, object_end), "Particle binary fixture has no System.groups value");
            const uint32_t group_count = read_uint32(attribute_position, object_end);
            attribute_position += sizeof(uint32_t);
            FO_VERIFY_AND_THROW(group_count == 1 && numeric_cast<size_t>(group_count) <= (object_end - attribute_position) / sizeof(uint32_t), "Particle binary fixture has unexpected System.groups data");

            reference_offset = attribute_position;
            original_reference = read_uint32(reference_offset, object_end);
            system_reference = object_index + 1;
            attribute_position += numeric_cast<size_t>(group_count) * sizeof(uint32_t);
            FO_VERIFY_AND_THROW(attribute_position == object_end, "Particle binary fixture has trailing System attribute data");
        }

        position = object_end;
    }

    FO_VERIFY_AND_THROW(position == binary.size() && reference_offset != string::npos, "Particle binary fixture has no valid System.groups reference");
    FO_VERIFY_AND_THROW(original_reference > 0 && original_reference <= object_count && original_reference != system_reference, "Particle binary fixture has an invalid original System.groups reference");

    uint32_t replacement_reference = 0;
    switch (mutation) {
    case ParticleReferenceMutation::Zero:
        break;
    case ParticleReferenceMutation::OutOfRange:
        FO_VERIFY_AND_THROW(object_count < std::numeric_limits<uint32_t>::max(), "Particle binary fixture cannot represent an out-of-range reference");
        replacement_reference = object_count + 1;
        break;
    case ParticleReferenceMutation::WrongType:
        replacement_reference = system_reference;
        break;
    }

    write_uint32(reference_offset, replacement_reference);
}

TEST_CASE("ParticleBaker", "[particle][baker]")
{
    using namespace BakerTests;

    SECTION("SetupBakersRegistersParticleInCanonicalOrder")
    {
        TestRig rig;
        const vector<unique_ptr<BaseBaker>> bakers = MakeRequestedBakers({string(ProtoBaker::NAME), string(ParticleBaker::NAME), string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 3);
        CHECK(bakers[0]->GetName() == RawCopyBaker::NAME);
        CHECK(bakers[0]->GetOrder() == 4);
        CHECK(bakers[1]->GetName() == ParticleBaker::NAME);
        CHECK(bakers[1]->GetOrder() == 5);
        CHECK(bakers[2]->GetName() == ProtoBaker::NAME);
        CHECK(bakers[2]->GetOrder() == 6);
    }

    SECTION("BakesDeterministicBinaryAndReloadsRendererAttributes")
    {
        TestRig first_rig;
        first_rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 10);

        ParticleBaker first_baker(first_rig.MakeContext());
        first_baker.BakeFiles(first_rig.GetAllSourceFiles(), "");

        REQUIRE(first_rig.Outputs.size() == 1);
        const vector<uint8_t>& first_binary = first_rig.Outputs.at("Particles/UnitTest.fopts");
        REQUIRE_FALSE(first_binary.empty());

        TestRig second_rig;
        second_rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 10);

        ParticleBaker second_baker(second_rig.MakeContext());
        second_baker.BakeFiles(second_rig.GetAllSourceFiles(), "");

        REQUIRE(second_rig.Outputs.size() == 1);
        const vector<uint8_t>& second_binary = second_rig.Outputs.at("Particles/UnitTest.fopts");
        CHECK(second_binary == first_binary);

        SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {first_binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(first_binary.size()));

        REQUIRE(system);
        REQUIRE(system->getNbGroups() == 1);

        const SPK::Ref<SPK::Group>& group = system->getGroup(0);
        SPK::Ref<SPK::FO::SparkQuadRenderer> renderer = SPK::dynamicCast<SPK::FO::SparkQuadRenderer>(group->getRenderer());

        REQUIRE(renderer);
        CHECK(renderer->GetDrawWidth() == 64);
        CHECK(renderer->GetDrawHeight() == 48);
        CHECK(renderer->GetDrawInScene());
        CHECK(renderer->GetEffectName() == "Effects/Particles_ColorAdd.fofx");
        CHECK(renderer->GetTextureName() == "Particles/TestParticle.png");
        CHECK(renderer->getScaleX() == Catch::Approx(1.5f));
        CHECK(renderer->getScaleY() == Catch::Approx(2.0f));
        CHECK(renderer->getAtlasDimensionX() == 2);
        CHECK(renderer->getAtlasDimensionY() == 3);
    }

    SECTION("BakesOnlyExplicitParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/First.fopts", ValidParticle, 10);
        rig.AddSourceFile("Particles/Second.fopts", ValidParticle, 11);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Second.fopts");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Particles/Second.fopts"));
        CHECK_FALSE(rig.Outputs.contains("Particles/First.fopts"));
    }

    SECTION("SkipsExplicitNonParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 10);
        rig.AddSourceFile("Particles/Readme.txt", "not a particle", 11);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Readme.txt");

        CHECK(rig.Outputs.empty());
    }

    SECTION("SkipsMissingExplicitParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 10);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Missing.fopts");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipParticle")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.fopts", ValidParticle, 42);

        vector<pair<string, uint64_t>> checks;
        ParticleBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Particles/UnitTest.fopts", 42});
    }

    SECTION("RejectsMalformedParticleXml")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Malformed.fopts", MalformedParticle);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsUnknownParticleObject")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Unknown.fopts", UnknownObjectParticle);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsTruncatedAndOversizedBinary")
    {
        const vector<uint8_t> binary = BakeValidParticleBinary();
        REQUIRE(binary.size() > 12);

        for (const size_t truncated_size : {size_t {0}, size_t {1}, size_t {11}, binary.size() - 1}) {
            SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(truncated_size));
            CHECK_FALSE(system);
        }

        vector<uint8_t> oversized(12, 0);
        oversized[0] = uint8_t {'S'};
        oversized[1] = uint8_t {'P'};
        oversized[2] = uint8_t {'K'};
        oversized[4] = 0xFF;
        oversized[5] = 0xFF;
        oversized[6] = 0xFF;
        oversized[7] = 0x7F;
        oversized[8] = 1;

        SPK::Ref<SPK::System> oversized_system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {oversized.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(oversized.size()));
        CHECK_FALSE(oversized_system);
    }

    SECTION("RejectsDescriptorSignatureMismatch")
    {
        vector<uint8_t> binary = BakeValidParticleBinary();
        size_t signature_offset = 12;

        while (signature_offset < binary.size() && binary[signature_offset] != 0) {
            ++signature_offset;
        }

        REQUIRE(signature_offset + 1 + sizeof(uint32_t) + sizeof(uint32_t) <= binary.size());
        signature_offset += 1 + sizeof(uint32_t);
        binary[signature_offset] ^= 0xFF;

        SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsZeroSystemGroupReference")
    {
        vector<uint8_t> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::Zero);

        SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsOutOfRangeSystemGroupReference")
    {
        vector<uint8_t> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::OutOfRange);

        SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsWrongTypeSystemGroupReference")
    {
        vector<uint8_t> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::WrongType);

        SPK::Ref<SPK::System> system = SPK::IO::IOManager::get().loadFromBuffer("spk", ptr<const uint8_t> {binary.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("CustomObjectRegistrationIsIdempotentAcrossThreads")
    {
        vector<std::future<void>> registrations;

        for (size_t i = 0; i < 8; ++i) {
            registrations.emplace_back(std::async(std::launch::async, [] { SPK::FO::EnsureSparkParticleObjectsRegistered(); }));
        }
        for (auto& registration : registrations) {
            registration.get();
        }

        CHECK(SPK::IO::IOManager::get().isObjectRegistered<SPK::FO::SparkQuadRenderer>());
    }

    SECTION("BakerDataSourceBakesBinaryOnDemandWithoutRawCopy")
    {
        const string temp_dir = MakeTempParticleBakerDir();
        const string input_dir = strex(temp_dir).combine_path("input").str();
        const string output_dir = strex(temp_dir).combine_path("output").str();
        const string source_path = strex(input_dir).combine_path("Particles/Runtime.fopts").str();
        const string output_path = strex(output_dir).combine_path("Visual/Particles/Runtime.fopts").str();

        ignore_unused(fs_remove_dir_tree(temp_dir));
        REQUIRE(fs_write_file(source_path, ValidParticle));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        auto config = ConfigFile("ParticleBakerDataSource.fomain",
            strex(R"(Baking.BakeOutput = {}
Baking.SingleThreadBaking = true
[ResourcePack]
Name = Visual
InputDirs = input
RecursiveInput = true
Bakers = Particle
)",
                output_dir)
                .str());

        settings.ApplyConfigFile(config, temp_dir);
        BakerDataSource data_source {&settings};
        size_t size = 0;
        uint64_t write_time = 0;

        CHECK_FALSE(fs_exists(output_path));

        const auto first_data = data_source.OpenFile("Particles/Runtime.fopts", size, write_time);
        REQUIRE(first_data);
        REQUIRE(size > 3);
        CHECK(first_data.as_ptr()[0] == uint8_t {'S'});
        CHECK(first_data.as_ptr()[1] == uint8_t {'P'});
        CHECK(first_data.as_ptr()[2] == uint8_t {'K'});
        CHECK(fs_exists(output_path));

        SPK::Ref<SPK::System> first_system = SPK::IO::IOManager::get().loadFromBuffer("spk", first_data.as_ptr().reinterpret_as<const char>().get(), numeric_cast<unsigned>(size));
        REQUIRE(first_system);
        CHECK(first_system->getNbGroups() == 1);

        const auto second_data = data_source.OpenFile("Particles/Runtime.fopts", size, write_time);
        REQUIRE(second_data);
        CHECK(second_data.as_ptr()[0] == uint8_t {'S'});

        CHECK(fs_remove_dir_tree(temp_dir));
    }
}

FO_END_NAMESPACE
