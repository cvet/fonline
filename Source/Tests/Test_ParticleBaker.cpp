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
#include "EffekseerExtension.h"
#include "ParticleBaker.h"
#include "ProtoBaker.h"
#include "RawCopyBaker.h"
#include "SparkExtension.h"
#include "Test_BakerHelpers.h"
#include "Test_ParticleFixtures.h"

#if FO_SPARK_PARTICLES
FO_DISABLE_WARNINGS_PUSH()
#include "SPARK.h"
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

#if FO_SPARK_PARTICLES
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
            <attrib id="draw in scene" value="true" />
            <attrib id="active" value="true" />
            <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
            <attrib id="texture" value="TestParticle.png" />
            <attrib id="scale" value="1.5;2" />
            <attrib id="atlas dimensions" value="2;3" />
          </SparkQuadRenderer>
        </attrib>
      </Group>
    </attrib>
  </System>
</SPARK>
)PARTICLE";

static constexpr string_view NonEmittingParticle = R"PARTICLE(
<SPARK>
  <System name="NonEmittingParticle">
    <attrib id="groups">
      <Group name="NonEmittingGroup">
        <attrib id="capacity" value="4" />
        <attrib id="life time" value="1;1" />
        <attrib id="emitters">
          <StaticEmitter>
            <attrib id="tank" value="0" />
            <attrib id="flow" value="0" />
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
            <attrib id="draw in scene" value="true" />
            <attrib id="active" value="true" />
            <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
            <attrib id="texture" value="TestParticle.png" />
            <attrib id="scale" value="1.5;2" />
            <attrib id="atlas dimensions" value="2;3" />
          </SparkQuadRenderer>
        </attrib>
      </Group>
    </attrib>
  </System>
</SPARK>
)PARTICLE";

static constexpr string_view FullyTransparentParticle = R"PARTICLE(
<SPARK>
  <System name="FullyTransparentParticle">
    <attrib id="groups">
      <Group name="FullyTransparentGroup">
        <attrib id="capacity" value="4" />
        <attrib id="life time" value="1;1" />
        <attrib id="color interpolator">
          <ColorGraphInterpolator>
            <attrib id="graph keys" value="0;1" />
            <attrib id="graph values" value="0x00000000;0x00000000" />
            <attrib id="graph values 2" value="0x00000000;0x00000000" />
            <attrib id="looping enabled" value="false" />
          </ColorGraphInterpolator>
        </attrib>
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
            <attrib id="draw in scene" value="true" />
            <attrib id="active" value="true" />
            <attrib id="effect" value="Effects/Particles_ColorAdd.fofx" />
            <attrib id="texture" value="TestParticle.png" />
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
#endif

#if FO_SPARK_PARTICLES
static constexpr string_view UnknownObjectParticle = R"PARTICLE(
<SPARK>
  <System name="UnknownObjectParticle">
    <attrib id="groups">
      <UnknownParticleGroup />
    </attrib>
  </System>
</SPARK>
)PARTICLE";
#endif

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
static auto MakeTempParticleBakerDir() -> u8string
{
    FO_STACK_TRACE_ENTRY();

    std::filesystem::path path = std::filesystem::temp_directory_path() / std::format("fo_particle_baker_{}", std::chrono::steady_clock::now().time_since_epoch().count());
    return fs_path_to_u8string(path);
}
#endif

#if FO_SPARK_PARTICLES
static auto BakeValidParticleBinary() -> vector<byte>
{
    FO_STACK_TRACE_ENTRY();

    BakerTests::TestRig rig;
    rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

    ParticleBaker baker(rig.MakeContext());
    baker.BakeFiles(rig.GetAllSourceFiles(), "");

    FO_VERIFY_AND_THROW(rig.Outputs.contains("Particles/UnitTest.spk"), "Particle test fixture was not baked");
    return rig.Outputs.at("Particles/UnitTest.spk");
}

enum class ParticleReferenceMutation
{
    Zero,
    OutOfRange,
    WrongType,
};

static void MutateSystemGroupsReference(vector<byte>& binary, ParticleReferenceMutation mutation)
{
    FO_STACK_TRACE_ENTRY();

    auto require_range = [&binary](size_t offset, size_t length, size_t limit) { FO_VERIFY_AND_THROW(limit <= binary.size() && offset <= limit && length <= limit - offset, "Particle binary fixture has an invalid range"); };
    auto read_uint32 = [&binary, &require_range](size_t offset, size_t limit) -> uint32_t {
        require_range(offset, sizeof(uint32_t), limit);
        return std::to_integer<uint32_t>(binary[offset]) | (std::to_integer<uint32_t>(binary[offset + 1]) << 8) | (std::to_integer<uint32_t>(binary[offset + 2]) << 16) | (std::to_integer<uint32_t>(binary[offset + 3]) << 24);
    };
    auto read_string = [&binary](size_t& offset, size_t limit) -> string {
        string value;
        while (offset < limit) {
            uint8_t ch = std::to_integer<uint8_t>(binary[offset++]);
            if (ch == 0) {
                return value;
            }
            value += numeric_cast<char>(ch);
        }

        FO_VERIFY_AND_THROW(false, "Particle binary fixture has an unterminated string");
        return {};
    };
    auto read_bool = [&binary, &require_range](size_t& offset, size_t limit) -> bool {
        require_range(offset, sizeof(uint8_t), limit);
        return binary[offset++] != byte {0};
    };
    auto write_uint32 = [&binary, &require_range](size_t offset, uint32_t value) {
        require_range(offset, sizeof(uint32_t), binary.size());
        binary[offset] = static_cast<byte>(value & 0xFFU);
        binary[offset + 1] = static_cast<byte>((value >> 8) & 0xFFU);
        binary[offset + 2] = static_cast<byte>((value >> 16) & 0xFFU);
        binary[offset + 3] = static_cast<byte>((value >> 24) & 0xFFU);
    };

    require_range(0, 12, binary.size());
    FO_VERIFY_AND_THROW(binary[0] == byte {'S'} && binary[1] == byte {'P'} && binary[2] == byte {'K'} && binary[3] == byte {0}, "Particle binary fixture has an invalid header");

    uint32_t payload_size = read_uint32(4, binary.size());
    uint32_t object_count = read_uint32(8, binary.size());
    FO_VERIFY_AND_THROW(numeric_cast<size_t>(payload_size) == binary.size() - 12, "Particle binary fixture has an invalid payload size");

    size_t reference_offset = string::npos;
    uint32_t original_reference = 0;
    uint32_t system_reference = 0;
    size_t position = 12;

    for (uint32_t object_index = 0; object_index < object_count; ++object_index) {
        string object_type = read_string(position, binary.size());
        uint32_t object_data_size = read_uint32(position, binary.size());
        position += sizeof(uint32_t);
        FO_VERIFY_AND_THROW(numeric_cast<size_t>(object_data_size) >= sizeof(uint32_t) && numeric_cast<size_t>(object_data_size) <= binary.size() - position, "Particle binary fixture has an invalid object record");

        size_t object_end = position + numeric_cast<size_t>(object_data_size);
        if (object_type == "System") {
            FO_VERIFY_AND_THROW(reference_offset == string::npos, "Particle binary fixture has multiple System objects");

            size_t attribute_position = position + sizeof(uint32_t);
            require_range(position, sizeof(uint32_t), object_end);

            bool name_defined = read_bool(attribute_position, object_end);
            if (name_defined) {
                (void)read_string(attribute_position, object_end);
            }

            bool shared_defined = read_bool(attribute_position, object_end);
            if (shared_defined) {
                require_range(attribute_position, sizeof(uint8_t), object_end);
                attribute_position += sizeof(uint8_t);
            }

            bool transform_defined = read_bool(attribute_position, object_end);
            if (transform_defined) {
                uint32_t transform_value_count = read_uint32(attribute_position, object_end);
                attribute_position += sizeof(uint32_t);
                FO_VERIFY_AND_THROW(numeric_cast<size_t>(transform_value_count) <= (object_end - attribute_position) / sizeof(float32_t), "Particle binary fixture has invalid transform data");
                attribute_position += numeric_cast<size_t>(transform_value_count) * sizeof(float32_t);
            }

            FO_VERIFY_AND_THROW(read_bool(attribute_position, object_end), "Particle binary fixture has no System.groups value");
            uint32_t group_count = read_uint32(attribute_position, object_end);
            attribute_position += sizeof(uint32_t);
            FO_VERIFY_AND_THROW(group_count == 1 && numeric_cast<size_t>(group_count) <= (object_end - attribute_position) / sizeof(uint32_t), "Particle binary fixture has unexpected System.groups data");

            reference_offset = attribute_position;
            original_reference = read_uint32(reference_offset, object_end);
            system_reference = object_index + 1;
            attribute_position += numeric_cast<size_t>(group_count) * sizeof(uint32_t);

            bool bounds_defined = read_bool(attribute_position, object_end);
            if (bounds_defined) {
                uint32_t bounds_value_count = read_uint32(attribute_position, object_end);
                attribute_position += sizeof(uint32_t);
                FO_VERIFY_AND_THROW(numeric_cast<size_t>(bounds_value_count) <= (object_end - attribute_position) / sizeof(float32_t), "Particle binary fixture has invalid baked-bounds data");
                attribute_position += numeric_cast<size_t>(bounds_value_count) * sizeof(float32_t);
            }

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

static auto GenerateSparkRandomValues(SPK::SPKContext& context, uint32_t& random_seed, size_t count) -> vector<uint32_t>
{
    FO_STACK_TRACE_ENTRY();

    SPK::RandomSeedScope random_seed_scope {context, random_seed};
    vector<uint32_t> values;
    values.reserve(count);

    for (size_t i = 0; i < count; i++) {
        values.emplace_back(SPK_RANDOM(context, 0U, 0x7FFFFFFFU));
    }

    return values;
}

TEST_CASE("SPARK random streams", "[particle][spark]")
{
    SECTION("SeededStreamsIgnoreInterleavedEffects")
    {
        SPK::SPKContext context;
        uint32_t first_seed = 123456U;
        uint32_t replay_seed = first_seed;
        uint32_t noise_seed = 987654U;

        vector<uint32_t> interleaved_values = GenerateSparkRandomValues(context, first_seed, 3);
        (void)GenerateSparkRandomValues(context, noise_seed, 32);

        vector<uint32_t> suffix = GenerateSparkRandomValues(context, first_seed, 3);
        interleaved_values.insert(interleaved_values.end(), suffix.begin(), suffix.end());

        vector<uint32_t> replay_values = GenerateSparkRandomValues(context, replay_seed, 6);

        CHECK(interleaved_values == replay_values);
        CHECK(first_seed == replay_seed);
    }

    SECTION("ContextsDoNotShareRandomState")
    {
        SPK::SPKContext first_context;
        SPK::SPKContext second_context;
        first_context.setRandomSeed(13579U);
        second_context.setRandomSeed(24680U);

        (void)SPK_RANDOM(first_context, 0U, 0x7FFFFFFFU);

        CHECK(second_context.getRandomSeed() == 24680U);
        CHECK(first_context.getRandomSeed() != second_context.getRandomSeed());

        SPK::FO::EnsureSparkParticleObjectsRegistered(first_context);
        CHECK(SPK::FO::IsSparkParticleObjectRegistered(first_context));
        CHECK_FALSE(SPK::FO::IsSparkParticleObjectRegistered(second_context));

        SPK::Ref<SPK::System> first_system = SPK::System::create(first_context, false);
        SPK::Ref<SPK::System> second_system = SPK::System::create(second_context, false);
        first_system->useConstantStep(0.1f);

        CHECK(first_system->getStepMode() == SPK::STEP_MODE_CONSTANT);
        CHECK(second_system->getStepMode() == SPK::STEP_MODE_REAL);
    }
}

TEST_CASE("SPARK baked bounds", "[particle][spark]")
{
    using namespace BakerTests;

    SPK::SPKContext spark_context;
    SPK::FO::EnsureSparkParticleObjectsRegistered(spark_context);
    SPK::IO::IOManager& spark_io = spark_context.getIOManager();

    auto load_spk = [&spark_io](const vector<byte>& binary) -> SPK::Ref<SPK::System> { return spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(binary.size())); };

    // The baker simulates the effect and records its extent so the runtime frames an emitting instance from a static
    // measurement instead of computing an axis-aligned bounding box every frame. Position extent and billboard radius
    // are recorded apart: the fixture emits one motionless particle at the origin from a point zone, so its position
    // box is degenerate and the whole extent lives in the quad radius. That radius is the group's graphical radius
    // (default 1) times the renderer's quad diagonal, sqrt(1.5^2 + 2^2) = 2.5 - it must not leak into the box, because
    // the runtime transforms the box with the emitter's world placement but never scales the quad.
    SECTION("BakerComputesAndStoresBoundsFromSimulation")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        const vector<byte>& binary = rig.Outputs.at("Particles/UnitTest.spk");
        REQUIRE_FALSE(binary.empty());

        SPK::Ref<SPK::System> system = load_spk(binary);
        REQUIRE(system);

        SPK::Vector3D bounds_min = system->getBakedBoundsMin();
        SPK::Vector3D bounds_max = system->getBakedBoundsMax();

        CHECK(bounds_min.x == Catch::Approx(0.0f));
        CHECK(bounds_min.y == Catch::Approx(0.0f));
        CHECK(bounds_min.z == Catch::Approx(0.0f));
        CHECK(bounds_max.x == Catch::Approx(0.0f));
        CHECK(bounds_max.y == Catch::Approx(0.0f));
        CHECK(bounds_max.z == Catch::Approx(0.0f));
        CHECK(system->getBakedBillboardRadius() == Catch::Approx(2.5f));
    }

    // An explicit box must survive the binary save/load unchanged so the runtime reads back exactly what the baker
    // measured.
    SECTION("ExplicitBoundsSurviveBinaryRoundtrip")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        SPK::Ref<SPK::System> system = load_spk(rig.Outputs.at("Particles/UnitTest.spk"));
        REQUIRE(system);

        SPK::Vector3D expected_min(-1.5f, -2.0f, -3.25f);
        SPK::Vector3D expected_max(4.0f, 5.5f, 6.75f);
        float expected_radius = 0.375f;
        system->setBakedBounds(expected_min, expected_max, expected_radius);

        std::ostringstream oss(std::ios::binary);
        REQUIRE(spark_io.save("spk", oss, system));

        std::string str = oss.str();
        const_span<byte> serialized = make_byte_span(str);
        vector<byte> resaved(serialized.begin(), serialized.end());

        SPK::Ref<SPK::System> reloaded = load_spk(resaved);
        REQUIRE(reloaded);

        CHECK(reloaded->getBakedBoundsMin().x == Catch::Approx(expected_min.x));
        CHECK(reloaded->getBakedBoundsMin().y == Catch::Approx(expected_min.y));
        CHECK(reloaded->getBakedBoundsMin().z == Catch::Approx(expected_min.z));
        CHECK(reloaded->getBakedBoundsMax().x == Catch::Approx(expected_max.x));
        CHECK(reloaded->getBakedBoundsMax().y == Catch::Approx(expected_max.y));
        CHECK(reloaded->getBakedBoundsMax().z == Catch::Approx(expected_max.z));
        CHECK(reloaded->getBakedBillboardRadius() == Catch::Approx(expected_radius));
    }

    // Baked bounds are mandatory: a particle system that never emits cannot be measured, so the baker rejects it
    // rather than shipping a system without a frame extent.
    SECTION("RejectsSystemThatEmitsNoParticles")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/NonEmitting.spark", NonEmittingParticle, 10);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("no visible particles"));
    }

    // A fully transparent particle draws nothing, so it must not reserve frame space. Measuring it would inflate every
    // effect whose colour graph fades out at the end of life, where the particle is also at its largest.
    SECTION("RejectsSystemWhoseParticlesAreFullyTransparent")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Transparent.spark", FullyTransparentParticle, 10);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("no visible particles"));
    }
}
#endif

#if FO_EFFEKSEER_PARTICLES
TEST_CASE("Effekseer model payload validation", "[particle][effekseer]")
{
    vector<byte> valid = ParticleTests::MakeFixtureModelPayload();
    CHECK_FALSE(ValidateEffekseerModelPayload(valid));

    SECTION("RejectsPayloadAboveResourceBudget")
    {
        vector<byte> oversized(EFFEKSEER_MODEL_PAYLOAD_SIZE_MAX + 1);
        CHECK(ValidateEffekseerModelPayload(oversized));
    }

    SECTION("RejectsEveryTruncatedPrefix")
    {
        for (size_t prefix_size = 0; prefix_size < valid.size(); prefix_size++) {
            CAPTURE(prefix_size);
            CHECK(ValidateEffekseerModelPayload({valid.data(), prefix_size}));
        }
    }

    auto write_int32 = [](vector<byte>& data, size_t offset, int32_t value) {
        REQUIRE(offset + sizeof(int32_t) <= data.size());
        uint32_t encoded = std::bit_cast<uint32_t>(value);

        for (size_t byte = 0; byte < sizeof(uint32_t); byte++) {
            data[offset + byte] = static_cast<std::byte>(numeric_cast<uint8_t>((encoded >> (byte * 8)) & 0xffu));
        }
    };

    SECTION("RejectsUnsupportedVersion")
    {
        vector<byte> invalid = valid;
        write_int32(invalid, 0, 7);
        CHECK(ValidateEffekseerModelPayload(invalid));
    }

    SECTION("RejectsInvalidFrameCount")
    {
        vector<byte> invalid = valid;
        write_int32(invalid, 3 * sizeof(int32_t), 0);
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, 3 * sizeof(int32_t), EFFEKSEER_MODEL_FRAME_COUNT_MAX + 1);
        CHECK(ValidateEffekseerModelPayload(invalid));
    }

    SECTION("RejectsInvalidVertexCount")
    {
        vector<byte> invalid = valid;
        write_int32(invalid, 4 * sizeof(int32_t), -1);
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, 4 * sizeof(int32_t), std::numeric_limits<int32_t>::max());
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, 4 * sizeof(int32_t), EFFEKSEER_MODEL_VERTEX_COUNT_MAX + 1);
        CHECK(ValidateEffekseerModelPayload(invalid));
    }

    constexpr size_t face_count_offset = 5 * sizeof(int32_t) + 4 * 68;
    constexpr size_t first_face_index_offset = face_count_offset + sizeof(int32_t);

    SECTION("RejectsInvalidFaceCount")
    {
        vector<byte> invalid = valid;
        write_int32(invalid, face_count_offset, -1);
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, face_count_offset, std::numeric_limits<int32_t>::max());
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, face_count_offset, EFFEKSEER_MODEL_FACE_COUNT_MAX + 1);
        CHECK(ValidateEffekseerModelPayload(invalid));
    }

    SECTION("RejectsOutOfRangeFaceIndexes")
    {
        vector<byte> invalid = valid;
        write_int32(invalid, first_face_index_offset, -1);
        CHECK(ValidateEffekseerModelPayload(invalid));

        write_int32(invalid, first_face_index_offset, 4);
        CHECK(ValidateEffekseerModelPayload(invalid));
    }

    SECTION("AllowsIgnoredLegacyTrailer")
    {
        valid.insert(valid.end(), {byte {0xde}, byte {0xad}, byte {0xbe}, byte {0xef}});
        CHECK_FALSE(ValidateEffekseerModelPayload(valid));
    }
}

TEST_CASE("Effekseer baked bounds trailer", "[particle][effekseer]")
{
    SECTION("RoundtripsBounds")
    {
        vector<byte> binary {byte {'S'}, byte {'K'}, byte {'F'}, byte {'E'}, byte {0x10}, byte {0x20}, byte {0x30}};
        size_t payload_size = binary.size();
        vec3 expected_min {-1.5f, -2.0f, -3.25f};
        vec3 expected_max {4.0f, 5.5f, 6.75f};
        float32_t expected_radius = 0.375f;

        AppendEffekseerBoundsTrailer(binary, expected_min, expected_max, expected_radius);
        CHECK(binary.size() == payload_size + 36);

        EffekseerBoundsTrailer trailer = ReadEffekseerBoundsTrailer(binary);
        CHECK(trailer.PayloadSize == payload_size);
        CHECK(trailer.PositionMin.x == Catch::Approx(expected_min.x));
        CHECK(trailer.PositionMin.y == Catch::Approx(expected_min.y));
        CHECK(trailer.PositionMin.z == Catch::Approx(expected_min.z));
        CHECK(trailer.PositionMax.x == Catch::Approx(expected_max.x));
        CHECK(trailer.PositionMax.y == Catch::Approx(expected_max.y));
        CHECK(trailer.PositionMax.z == Catch::Approx(expected_max.z));
        CHECK(trailer.BillboardRadius == Catch::Approx(expected_radius));
    }

    SECTION("ThrowsOnBinaryWithoutTrailer")
    {
        vector<byte> binary {byte {'S'}, byte {'K'}, byte {'F'}, byte {'E'}, byte {0x10}, byte {0x20}, byte {0x30}};

        CHECK_THROWS(ReadEffekseerBoundsTrailer(binary));
    }

    SECTION("ThrowsOnTruncatedBinary")
    {
        vector<byte> binary {byte {0x01}, byte {0x02}, byte {0x03}};

        CHECK_THROWS(ReadEffekseerBoundsTrailer(binary));
    }
}
#endif

TEST_CASE("ParticleBaker", "[particle][baker]")
{
    using namespace BakerTests;

#if FO_SPARK_PARTICLES
    SPK::SPKContext spark_context;
    SPK::FO::EnsureSparkParticleObjectsRegistered(spark_context);
    SPK::IO::IOManager& spark_io = spark_context.getIOManager();
#endif

    SECTION("SetupBakersRegistersParticleInCanonicalOrder")
    {
        TestRig rig;
        vector<unique_ptr<BaseBaker>> bakers = MakeRequestedBakers({string(ProtoBaker::NAME), string(ParticleBaker::NAME), string(RawCopyBaker::NAME)}, rig);

        REQUIRE(bakers.size() == 3);
        CHECK(bakers[0]->GetName() == RawCopyBaker::NAME);
        CHECK(bakers[0]->GetOrder() == 4);
        CHECK(bakers[1]->GetName() == ParticleBaker::NAME);
        CHECK(bakers[1]->GetOrder() == 5);
        CHECK(bakers[2]->GetName() == ProtoBaker::NAME);
        CHECK(bakers[2]->GetOrder() == 7);
    }

#if FO_SPARK_PARTICLES
    SECTION("BakesDeterministicBinaryAndReloadsRendererAttributes")
    {
        TestRig first_rig;
        first_rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker first_baker(first_rig.MakeContext());
        first_baker.BakeFiles(first_rig.GetAllSourceFiles(), "");

        REQUIRE(first_rig.Outputs.size() == 1);
        const vector<byte>& first_binary = first_rig.Outputs.at("Particles/UnitTest.spk");
        REQUIRE_FALSE(first_binary.empty());

        TestRig second_rig;
        second_rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker second_baker(second_rig.MakeContext());
        second_baker.BakeFiles(second_rig.GetAllSourceFiles(), "");

        REQUIRE(second_rig.Outputs.size() == 1);
        const vector<byte>& second_binary = second_rig.Outputs.at("Particles/UnitTest.spk");
        CHECK(second_binary == first_binary);

        SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(first_binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(first_binary.size()));

        REQUIRE(system);
        REQUIRE(system->getNbGroups() == 1);
        CHECK(&system->getContext() == &spark_context);

        const SPK::Ref<SPK::Group>& group = system->getGroup(0);
        const SPK::Ref<SPK::Renderer>& renderer = group->getRenderer();

        REQUIRE(group->getNbEmitters() == 1);
        REQUIRE(renderer);
        REQUIRE(SPK::FO::IsSparkQuadRenderer(*renderer));

        const SPK::Ref<SPK::Emitter>& emitter = group->getEmitter(0);
        SPK::FO::SparkQuadRendererData renderer_data = SPK::FO::GetSparkQuadRendererData(*renderer);

        CHECK(&group->getContext() == &spark_context);
        CHECK(&emitter->getContext() == &spark_context);
        CHECK(&emitter->getZone()->getContext() == &spark_context);
        CHECK(&renderer->getContext() == &spark_context);
        CHECK(renderer_data.DrawInScene);
        CHECK(renderer_data.EffectName == "Effects/Particles_ColorAdd.fofx");
        CHECK(renderer_data.TextureName == "TestParticle.png");
        CHECK(renderer_data.ScaleX == Catch::Approx(1.5f));
        CHECK(renderer_data.ScaleY == Catch::Approx(2.0f));
        CHECK(renderer_data.AtlasDimensionX == 2);
        CHECK(renderer_data.AtlasDimensionY == 3);
    }

    SECTION("BakesOnlyExplicitParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/First.spark", ValidParticle, 10);
        rig.AddSourceFile("Particles/Second.spark", ValidParticle, 11);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Second.spk");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Particles/Second.spk"));
        CHECK_FALSE(rig.Outputs.contains("Particles/First.spk"));
    }

    SECTION("SkipsExplicitNonParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);
        rig.AddSourceFile("Particles/Readme.txt", "not a particle", 11);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Readme.txt");

        CHECK(rig.Outputs.empty());
    }

    SECTION("SkipsMissingExplicitParticleTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Missing.spk");

        CHECK(rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipParticle")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 42);

        vector<pair<string, uint64_t>> checks;
        ParticleBaker baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return false;
        }));
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 1);
        CHECK(checks.front() == pair<string, uint64_t> {"Particles/UnitTest.spk", 42});
    }

    SECTION("RejectsMalformedParticleXml")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Malformed.spark", MalformedParticle);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsAbsoluteSparkTexturePath")
    {
        string particle {ValidParticle};
        string relative_path = "TestParticle.png";
        size_t path_pos = particle.find(relative_path);
        REQUIRE(path_pos != string::npos);
        particle.replace(path_pos, relative_path.size(), "C:/Outside/TestParticle.png");

        TestRig rig;
        rig.AddSourceFile("Particles/Absolute.spark", particle);
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("SPARK particle texture path must be relative"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("AllowsParentRelativeSparkTexturePathInsideResourceSource")
    {
        string particle {ValidParticle};
        string relative_path = "TestParticle.png";
        size_t path_pos = particle.find(relative_path);
        REQUIRE(path_pos != string::npos);
        particle.replace(path_pos, relative_path.size(), "../Texture/TestParticle.png");

        TestRig rig;
        rig.AddSourceFile("Particles/Nested/ParentRelative.spark", particle);
        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Particles/Nested/ParentRelative.spk"));
    }

    SECTION("RejectsSparkTexturePathOutsideResourceSource")
    {
        string particle {ValidParticle};
        string relative_path = "TestParticle.png";
        size_t path_pos = particle.find(relative_path);
        REQUIRE(path_pos != string::npos);
        particle.replace(path_pos, relative_path.size(), "../../../Outside/TestParticle.png");

        TestRig rig;
        rig.AddSourceFile("Particles/Nested/Outside.spark", particle);
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_WITH(baker.BakeFiles(rig.GetAllSourceFiles(), ""), Catch::Matchers::ContainsSubstring("SPARK particle texture path escapes its resource source"));
        CHECK(rig.Outputs.empty());
    }

    SECTION("SkipsExplicitSparkSourceTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/UnitTest.spark", ValidParticle, 10);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/UnitTest.spark");

        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsAuthoredSparkRuntimeBinary")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Cooked.spk", "SPK");
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }
#endif

#if FO_EFFEKSEER_PARTICLES
    SECTION("CompilesTextEffekseerProjectToDerivedRuntimeBinary")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string source_path = fs_combine_path(source_dir, "Particles/Simple.efkproj");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(source_path, ParticleTests::SimpleGeneratingPositionProject));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(source_files.GetAllFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        REQUIRE(rig.Outputs.contains("Particles/Simple.efk"));
        const vector<byte>& binary = rig.Outputs.at("Particles/Simple.efk");
        REQUIRE(binary.size() > 4);
        CHECK(string_from_byte_span(const_span<byte> {binary.data(), 4}) == "SKFE");
        CHECK_FALSE(rig.Outputs.contains("Particles/Simple.efkproj"));
    }

    SECTION("IncludesModelGeometryInEffekseerBounds")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string project_path = fs_combine_path(source_dir, "Particles/Mesh.efkproj");
        u8string model_path = fs_combine_path(source_dir, "Particles/Model/Fixture.efkmodel");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(project_path, ParticleTests::MakeModelProject(2)));
        REQUIRE(fs_write_file_bytes(model_path, ParticleTests::MakeFixtureModelPayload()));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(source_files.GetAllFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        REQUIRE(rig.Outputs.contains("Particles/Mesh.efk"));
        EffekseerBoundsTrailer trailer = ReadEffekseerBoundsTrailer(rig.Outputs.at("Particles/Mesh.efk"));

        CHECK(trailer.PositionMin.x == Catch::Approx(0.0f));
        CHECK(trailer.PositionMin.y == Catch::Approx(0.0f));
        CHECK(trailer.PositionMin.z == Catch::Approx(0.0f));
        CHECK(trailer.PositionMax.x == Catch::Approx(0.0f));
        CHECK(trailer.PositionMax.y == Catch::Approx(0.0f));
        CHECK(trailer.PositionMax.z == Catch::Approx(0.0f));
        CHECK(trailer.BillboardRadius == Catch::Approx(std::sqrt(0.5f)));
    }

    SECTION("RejectsModelSymlinkOutsideTheResourceSource")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string project_path = fs_combine_path(source_dir, "Particles/Mesh.efkproj");
        u8string outside_dir = fs_combine_path(temp_dir, "outside");
        u8string outside_model_path = fs_combine_path(outside_dir, "Fixture.efkmodel");
        u8string linked_model_dir = fs_combine_path(source_dir, "Particles/Model");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(project_path, ParticleTests::MakeModelProject(2)));
        REQUIRE(fs_write_file_bytes(outside_model_path, ParticleTests::MakeFixtureModelPayload()));

        std::error_code link_error;
        std::filesystem::create_directory_symlink(std::filesystem::path {fs_make_path(outside_dir)}, std::filesystem::path {fs_make_path(linked_model_dir)}, link_error);

        if (link_error) {
            SKIP(strex("Directory symlinks are unavailable: {}", link_error.message()).str());
        }

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_WITH(baker.BakeFiles(source_files.GetAllFiles(), ""), Catch::Matchers::ContainsSubstring("resolves outside its directory resource source"));
    }

    SECTION("BatchesTextEffekseerProjects")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string first_source_path = fs_combine_path(source_dir, "Particles/First/Effect.efkproj");
        u8string second_source_path = fs_combine_path(source_dir, "Particles/Second/Effect.efkproj");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(first_source_path, ParticleTests::SimpleGeneratingPositionProject));
        REQUIRE(fs_write_file_text(second_source_path, ParticleTests::SimpleGeneratingPositionProject));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(source_files.GetAllFiles(), "");

        REQUIRE(rig.Outputs.size() == 2);
        CHECK(rig.Outputs.contains("Particles/First/Effect.efk"));
        CHECK(rig.Outputs.contains("Particles/Second/Effect.efk"));

        u8string dependency_path = fs_combine_path(source_dir, "Particles/First/Texture/Splash01.png");
        REQUIRE(fs_write_file_text(dependency_path, "first texture"));
        std::filesystem::last_write_time(std::filesystem::path {fs_make_path(dependency_path)}, std::filesystem::file_time_type::clock::now() + std::chrono::minutes {5});
        uint64_t dependency_write_time = fs_last_write_time(dependency_path);
        FileSystem changed_source_files;
        changed_source_files.AddDirSource(source_dir, true, true);
        ParticleBaker changed_baker(rig.MakeContext("TestPack", [dependency_write_time](string_view, uint64_t write_time) { return write_time == dependency_write_time; }));
        changed_baker.BakeFiles(changed_source_files.GetAllFiles(), "");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Particles/First/Effect.efk"));
        CHECK_FALSE(rig.Outputs.contains("Particles/Second/Effect.efk"));
    }

    SECTION("BakesOnlyExplicitDerivedEffekseerTarget")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string first_source_path = fs_combine_path(source_dir, "Particles/First.efkproj");
        u8string second_source_path = fs_combine_path(source_dir, "Particles/Second.efkproj");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(first_source_path, ParticleTests::SimpleGeneratingPositionProject));
        REQUIRE(fs_write_file_text(second_source_path, ParticleTests::SimpleGeneratingPositionProject));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(source_files.GetAllFiles(), "Particles/Second.efk");

        REQUIRE(rig.Outputs.size() == 1);
        CHECK(rig.Outputs.contains("Particles/Second.efk"));
        CHECK_FALSE(rig.Outputs.contains("Particles/First.efk"));
    }

    SECTION("SkipsExplicitEffekseerSourceTarget")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Simple.efkproj", ParticleTests::SimpleGeneratingPositionProject);

        ParticleBaker baker(rig.MakeContext());
        baker.BakeFiles(rig.GetAllSourceFiles(), "Particles/Simple.efkproj");

        CHECK(rig.Outputs.empty());
    }

    SECTION("TracksEffekseerDependencyAddChangeAndRename")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string output_dir = fs_combine_path(temp_dir, "output");
        u8string source_path = fs_combine_path(source_dir, "Particles/Nested/Simple.efkproj");
        u8string dependency_path = fs_combine_path(source_dir, "Particles/Texture/Splash01.png");
        u8string renamed_dependency_path = fs_combine_path(source_dir, "Particles/Texture/SplashRenamed.png");
        u8string baked_output_path = fs_combine_path(output_dir, "TestPack/Particles/Nested/Simple.efk");
        string project {ParticleTests::SimpleGeneratingPositionProject};
        string original_dependency = "Texture/Splash01.png";
        size_t dependency_pos = project.find(original_dependency);
        REQUIRE(dependency_pos != string::npos);
        project.replace(dependency_pos, original_dependency.size(), "../Texture/Splash01.png");

        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(source_path, project));

        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, output_dir);
        vector<pair<string, uint64_t>> checks;

        auto run_bake = [&](bool record_checks = false) {
            FileSystem source_files;
            source_files.AddDirSource(source_dir, true, true);
            ParticleBaker baker(rig.MakeContext("TestPack", [&](string_view path, uint64_t write_time) {
                if (record_checks) {
                    checks.emplace_back(string(path), write_time);
                }

                return write_time > fs_last_write_time(baked_output_path);
            }));
            baker.BakeFiles(source_files.GetAllFiles(), "");

            if (rig.Outputs.contains("Particles/Nested/Simple.efk")) {
                REQUIRE(fs_write_file_bytes(baked_output_path, rig.Outputs.at("Particles/Nested/Simple.efk")));
            }
        };

        run_bake();
        REQUIRE(rig.Outputs.contains("Particles/Nested/Simple.efk"));

        u8string cache_dir = fs_combine_path(output_dir, BAKER_CACHE_DIR);
        u8string cache_path = fs_combine_path(cache_dir, "Effekseer/TestPack/Particles/Nested/Simple.efk.deps");
        optional<u8string> missing_snapshot = fs_read_file_text(cache_path);
        REQUIRE(missing_snapshot);
        u8string resolved_dependency_path = fs_resolve_path(dependency_path);
        u8string missing_dependency_record = u8strex(u8"{}\t-\t-", resolved_dependency_path).str();
        CHECK(missing_snapshot->view().native_view().find(missing_dependency_record.view().native_view()) != std::u8string_view::npos);

        run_bake();
        CHECK(rig.Outputs.empty());

        REQUIRE(fs_write_file_text(dependency_path, "texture"));
        uint64_t dependency_write_time = fs_last_write_time(dependency_path);
        REQUIRE(dependency_write_time != 0);
        checks.clear();

        {
            FileSystem source_files;
            source_files.AddDirSource(source_dir, true, true);
            ParticleBaker discovery_baker(rig.MakeContext("TestPack", [&checks](string_view path, uint64_t write_time) {
                checks.emplace_back(string(path), write_time);
                return false;
            }));
            discovery_baker.BakeFiles(source_files.GetAllFiles(), "");
        }

        CHECK(rig.Outputs.empty());
        CHECK_FALSE(fs_exists(baked_output_path));
        REQUIRE(checks.size() == 1);
        CHECK(checks.front().first == "Particles/Nested/Simple.efk");
        CHECK(checks.front().second == std::max(fs_last_write_time(source_path), dependency_write_time));

        run_bake();
        REQUIRE(rig.Outputs.contains("Particles/Nested/Simple.efk"));

        checks.clear();
        run_bake(true);
        CHECK(rig.Outputs.empty());
        REQUIRE(checks.size() == 1);
        CHECK(checks.front().first == "Particles/Nested/Simple.efk");
        CHECK(checks.front().second == std::max(fs_last_write_time(source_path), dependency_write_time));

        REQUIRE(fs_write_file_text(dependency_path, "changed texture payload"));
        run_bake();
        CHECK(rig.Outputs.contains("Particles/Nested/Simple.efk"));

        std::filesystem::rename(std::filesystem::path {fs_make_path(dependency_path)}, std::filesystem::path {fs_make_path(renamed_dependency_path)});
        run_bake();
        CHECK(rig.Outputs.contains("Particles/Nested/Simple.efk"));

        run_bake();
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsMalformedEffekseerProjectXml")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string source_path = fs_combine_path(source_dir, "Particles/Broken.efkproj");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(source_path, "<EffekseerProject><Root>"));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(source_files.GetAllFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsEffekseerProjectFromAnotherEditorVersion")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string source_path = fs_combine_path(source_dir, "Particles/Unsupported.efkproj");
        string project {ParticleTests::SimpleGeneratingPositionProject};
        size_t version_pos = project.find("<ToolVersion>1.80.5</ToolVersion>");
        REQUIRE(version_pos != string::npos);
        project.replace(version_pos, string_view {"<ToolVersion>1.80.5</ToolVersion>"}.size(), "<ToolVersion>1.80.4</ToolVersion>");

        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        REQUIRE(fs_write_file_text(source_path, project));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(source_files.GetAllFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsEffekseerContainerRenamedAsTextProject")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string source_dir = fs_combine_path(temp_dir, "source");
        u8string source_path = fs_combine_path(source_dir, "Particles/Disguised.efkproj");
        (void)fs_remove_dir_tree(temp_dir);
        auto cleanup = scope_exit([&temp_dir]() noexcept { (void)fs_remove_dir_tree(temp_dir); });
        vector<byte> container {byte {'E'}, byte {'F'}, byte {'K'}, byte {'E'}, byte {0}, byte {0}, byte {0}, byte {0}};
        REQUIRE(fs_write_file_bytes(source_path, container));

        FileSystem source_files;
        source_files.AddDirSource(source_dir, true, true);
        TestRig rig;
        BakerTests::OverrideSetting(rig.Settings.BakeOutput, fs_combine_path(temp_dir, "output"));
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(source_files.GetAllFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsAuthoredEffekseerRuntimeBinary")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Cooked.efk", "SKFE");
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsNonDirectoryEffekseerProjectSource")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Simple.efkproj", ParticleTests::SimpleGeneratingPositionProject);
        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }
#endif

#if FO_SPARK_PARTICLES
    SECTION("RejectsUnknownParticleObject")
    {
        TestRig rig;
        rig.AddSourceFile("Particles/Unknown.spark", UnknownObjectParticle);

        ParticleBaker baker(rig.MakeContext());

        CHECK_THROWS_AS(baker.BakeFiles(rig.GetAllSourceFiles(), ""), ParticleBakerException);
        CHECK(rig.Outputs.empty());
    }

    SECTION("RejectsTruncatedAndOversizedBinary")
    {
        vector<byte> binary = BakeValidParticleBinary();
        REQUIRE(binary.size() > 12);

        for (size_t truncated_size : {size_t {0}, size_t {1}, size_t {11}, binary.size() - 1}) {
            SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(truncated_size));
            CHECK_FALSE(system);
        }

        vector<byte> oversized(12, byte {0});
        oversized[0] = byte {'S'};
        oversized[1] = byte {'P'};
        oversized[2] = byte {'K'};
        oversized[4] = byte {0xFF};
        oversized[5] = byte {0xFF};
        oversized[6] = byte {0xFF};
        oversized[7] = byte {0x7F};
        oversized[8] = byte {1};

        SPK::Ref<SPK::System> oversized_system = spark_io.loadFromBuffer("spk", make_ptr(oversized.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(oversized.size()));
        CHECK_FALSE(oversized_system);
    }

    SECTION("RejectsDescriptorSignatureMismatch")
    {
        vector<byte> binary = BakeValidParticleBinary();
        size_t signature_offset = 12;

        while (signature_offset < binary.size() && binary[signature_offset] != byte {0}) {
            ++signature_offset;
        }

        REQUIRE(signature_offset + 1 + sizeof(uint32_t) + sizeof(uint32_t) <= binary.size());
        signature_offset += 1 + sizeof(uint32_t);
        binary[signature_offset] ^= byte {0xFF};

        SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsZeroSystemGroupReference")
    {
        vector<byte> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::Zero);

        SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsOutOfRangeSystemGroupReference")
    {
        vector<byte> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::OutOfRange);

        SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("RejectsWrongTypeSystemGroupReference")
    {
        vector<byte> binary = BakeValidParticleBinary();
        MutateSystemGroupsReference(binary, ParticleReferenceMutation::WrongType);

        SPK::Ref<SPK::System> system = spark_io.loadFromBuffer("spk", make_ptr(binary.data()).reinterpret_as<const char>().get(), numeric_cast<unsigned>(binary.size()));
        CHECK_FALSE(system);
    }

    SECTION("CustomObjectRegistrationIsIdempotentAcrossThreads")
    {
        vector<std::future<void>> registrations;

        for (size_t i = 0; i < 8; ++i) {
            registrations.emplace_back(std::async(std::launch::async, [&spark_context] { SPK::FO::EnsureSparkParticleObjectsRegistered(spark_context); }));
        }
        for (auto& registration : registrations) {
            registration.get();
        }

        CHECK(SPK::FO::IsSparkParticleObjectRegistered(spark_context));
    }

    SECTION("BakerDataSourceBakesBinaryOnDemandWithoutRawCopy")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string input_dir = fs_combine_path(temp_dir, "input");
        u8string output_dir = fs_combine_path(temp_dir, "output");
        u8string source_path = fs_combine_path(input_dir, "Particles/Runtime.spark");
        u8string output_path = fs_combine_path(output_dir, "Visual/Particles/Runtime.spk");
        u8string late_source_path = fs_combine_path(input_dir, "Particles/LateRuntime.spark");
        u8string late_output_path = fs_combine_path(output_dir, "Visual/Particles/LateRuntime.spk");

        ignore_unused(fs_remove_dir_tree(temp_dir));
        REQUIRE(fs_write_file_text(source_path, ValidParticle));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        ConfigFile config = ConfigFile(u8strex(u8R"(Baking.BakeOutput = {}
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

        auto first_data = data_source.OpenFile("Particles/Runtime.spk", size, write_time);
        REQUIRE(first_data);
        REQUIRE(size > 3);
        CHECK(first_data.as_ptr()[0] == byte {'S'});
        CHECK(first_data.as_ptr()[1] == byte {'P'});
        CHECK(first_data.as_ptr()[2] == byte {'K'});
        CHECK(fs_exists(output_path));

        SPK::Ref<SPK::System> first_system = spark_io.loadFromBuffer("spk", first_data.as_ptr().reinterpret_as<const char>().get(), numeric_cast<unsigned>(size));
        REQUIRE(first_system);
        CHECK(first_system->getNbGroups() == 1);

        auto second_data = data_source.OpenFile("Particles/Runtime.spk", size, write_time);
        REQUIRE(second_data);
        CHECK(second_data.as_ptr()[0] == byte {'S'});

        CHECK_FALSE(data_source.Reindex());
        REQUIRE(fs_write_file_text(late_source_path, ValidParticle));
        CHECK_FALSE(data_source.IsFileExists("Particles/LateRuntime.spk"));
        CHECK(data_source.Reindex());
        CHECK(data_source.IsFileExists("Particles/LateRuntime.spk"));
        CHECK_FALSE(data_source.Reindex());

        auto late_data = data_source.OpenFile("Particles/LateRuntime.spk", size, write_time);
        REQUIRE(late_data);
        CHECK(late_data.as_ptr()[0] == byte {'S'});
        CHECK(fs_exists(late_output_path));

        CHECK(fs_remove_dir_tree(temp_dir));
    }
#endif

#if FO_EFFEKSEER_PARTICLES
    SECTION("BakerDataSourceCompilesEffekseerRuntimeOnDemand")
    {
        u8string temp_dir = MakeTempParticleBakerDir();
        u8string input_dir = fs_combine_path(temp_dir, "input");
        u8string output_dir = fs_combine_path(temp_dir, "output");
        u8string source_path = fs_combine_path(input_dir, "Particles/Runtime.efkproj");
        u8string dependency_path = fs_combine_path(input_dir, "Particles/Texture/Splash01.png");
        u8string output_path = fs_combine_path(output_dir, "Visual/Particles/Runtime.efk");

        (void)fs_remove_dir_tree(temp_dir);
        REQUIRE(fs_write_file_text(source_path, ParticleTests::SimpleGeneratingPositionProject));

        GlobalSettings settings {true};
        settings.ApplyDefaultSettings();

        ConfigFile config = ConfigFile(u8strex(u8R"(Baking.BakeOutput = {}
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

        auto runtime_data = data_source.OpenFile("Particles/Runtime.efk", size, write_time);
        REQUIRE(runtime_data);
        REQUIRE(size > 4);
        auto runtime_data_ptr = runtime_data.as_ptr();
        CHECK(std::ranges::equal(const_span<byte> {runtime_data_ptr.get(), 4}, string_to_byte_span("SKFE")));
        CHECK(fs_exists(output_path));
        CHECK_FALSE(data_source.Reindex());

        REQUIRE(fs_write_file_text(dependency_path, "texture"));
        CHECK(data_source.Reindex());
        auto runtime_data_after_add = data_source.OpenFile("Particles/Runtime.efk", size, write_time);
        REQUIRE(runtime_data_after_add);
        auto runtime_data_after_add_ptr = runtime_data_after_add.as_ptr();
        CHECK(std::ranges::equal(const_span<byte> {runtime_data_after_add_ptr.get(), 4}, string_to_byte_span("SKFE")));
        CHECK_FALSE(data_source.Reindex());

        REQUIRE(fs_remove_file(dependency_path));
        CHECK(data_source.Reindex());
        auto runtime_data_after_remove = data_source.OpenFile("Particles/Runtime.efk", size, write_time);
        REQUIRE(runtime_data_after_remove);
        auto runtime_data_after_remove_ptr = runtime_data_after_remove.as_ptr();
        CHECK(std::ranges::equal(const_span<byte> {runtime_data_after_remove_ptr.get(), 4}, string_to_byte_span("SKFE")));
        CHECK_FALSE(data_source.Reindex());

        CHECK(fs_remove_dir_tree(temp_dir));
    }
#endif
}

FO_END_NAMESPACE
