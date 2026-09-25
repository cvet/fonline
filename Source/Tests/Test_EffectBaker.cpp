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
#include "EffectBaker.h"
#include "Test_BakerHelpers.h"

FO_DISABLE_WARNINGS_PUSH()
#include "vkd3d_shader.h"
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static constexpr string_view VALID_EFFECT = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition.xy, 0.0, 1.0);
    TexCoord = InTexCoord;
}

[FragmentShader]
layout(binding = 0) uniform sampler2D MainTex;
layout(binding = 3, std140) uniform TimeBuf { vec4 FrameTime; vec4 GameTime; };

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = texture(MainTex, TexCoord + GameTime.xx * 0.0);
}
)";

static constexpr string_view EFFECT_WITH_AUX_BINDINGS = R"(
[Effect]

[VertexShader]
layout(location = 0) in vec3 InPosition;
layout(location = 2) in vec2 InTexCoord;
layout(location = 0) out vec2 TexCoord;

void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
    TexCoord = InTexCoord;
}

[FragmentShader]
layout(binding = 2) uniform sampler2D IndoorMaskTex;
layout(binding = 4, std140) uniform RandomValueBuf { vec4 RandomValue; };
layout(binding = 5, std140) uniform ScriptValueBuf { vec4 ScriptValue[MAX_SCRIPT_VALUES / 4]; };
layout(binding = 6, std140) uniform CameraBuf { vec4 MapAnchorScreenPos; vec4 ChunkScreenAnchor; };

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    vec2 uv = MapAnchorScreenPos.xy + TexCoord * MapAnchorScreenPos.zw;
    FragColor = texture(IndoorMaskTex, uv) + vec4(RandomValue.x + ScriptValue[0].x + ChunkScreenAnchor.x * 0.0);
}
)";

static constexpr string_view MULTI_PASS_EFFECT = R"(
[Effect]
Passes = 2

[VertexShader]
layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(0.25, 0.5, 0.75, 1.0);
}

[VertexShader Pass2]
layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy * 0.5, 0.0, 1.0);
}

[FragmentShader Pass2]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(0.75, 0.5, 0.25, 1.0);
}
)";

static constexpr string_view EFFECT_WITH_DYNAMIC_UNIFORM_INDEX = R"(
[Effect]

[VertexShader]
layout(binding = 5, std140) uniform ScriptValueBuf { vec4 ScriptValue[MAX_SCRIPT_VALUES / 4]; };

layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = ScriptValue[int(InPosition.z)] + vec4(InPosition.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(1.0);
}
)";

static constexpr string_view EFFECT_WITH_BAD_PROJ_BUFFER = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { vec4 OnlyOne; };

layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy + OnlyOne.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(1.0);
}
)";

static constexpr string_view EFFECT_FOR_LEVEL9 = R"(
[Effect]

[VertexShader]
layout(binding = 0, std140) uniform ProjBuf { mat4 ProjMatrix; };

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in vec2 InTexCoord;
layout(location = 3) in vec2 InEggData;

layout(location = 0) out vec4 Color;
layout(location = 1) out vec2 TexCoord;
layout(location = 2) out vec2 EggFlag;

void main(void)
{
    gl_Position = ProjMatrix * vec4(InPosition, 1.0);
    Color = InColor;
    TexCoord = InTexCoord;
    EggFlag = InEggData;
}

[FragmentShader]
layout(binding = 0) uniform sampler2D MainTex;
layout(binding = 1, std140) uniform EggBuf { vec4 EggData[3]; };

layout(location = 0) in vec4 Color;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec2 EggFlag;
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = texture(MainTex, TexCoord) * Color * EggData[0] + EggData[1] * EggData[2].x;

    if (FragColor.a * EggFlag.y <= EggData[2].y) {
        discard;
    }
}
)";

static constexpr string_view EFFECT_BEYOND_LEVEL9 = R"(
[Effect]

[VertexShader]
layout(location = 0) in vec3 InPosition;

void main(void)
{
    gl_Position = vec4(InPosition.xy, 0.0, 1.0);
}

[FragmentShader]
layout(location = 0) out vec4 FragColor;

void main(void)
{
    FragColor = vec4(gl_FragCoord.xy, 0.0, 1.0);
}
)";

TEST_CASE("EffectBaker")
{
    using namespace BakerTests;

    TestRig rig;
    auto bakers = MakeRequestedBakers({string(EffectBaker::NAME)}, rig);

    REQUIRE(bakers.size() == 1);
    CHECK(bakers.front()->GetName() == EffectBaker::NAME);
    CHECK(bakers.front()->GetOrder() == 4);
    CHECK_NOTHROW(bakers.front()->BakeFiles(TestRig::MakeEmptyFiles(), "skip.bin"));

    SECTION("SkipsNonEffectSourcesAndTargets")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Readme.txt", "not an effect");
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "skip.bin"));
        CHECK(local_rig.Outputs.empty());

        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
    }

    SECTION("BakesWithoutBakeChecker")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        auto context = local_rig.MakeContext();
        context->BakeChecker = {};

        EffectBaker baker {std::move(context)};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx-1-info"));
    }

    SECTION("BakeCheckerCanSkipEffect")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT, 77);

        vector<pair<string, uint64_t>> checks;
        EffectBaker baker {local_rig.MakeContext("Effects", [&checks](string_view path, uint64_t write_time) {
            checks.emplace_back(string(path), write_time);
            return path != "Effects/Test.fofx";
        })};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.empty());
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx", 77}; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx-1-info", 77}; }));
        CHECK(std::ranges::any_of(checks, [](const auto& check) { return check == pair<string, uint64_t> {"Effects/Test.fofx-1-frag-msl_ios", 77}; }));
    }

    SECTION("SkipsMissingExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Missing.fofx-1-info"));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakeCheckerCanSkipExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT, 88);

        EffectBaker baker {local_rig.MakeContext("Effects", [](string_view, uint64_t) { return false; })};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Test.fofx-1-info"));
        CHECK(local_rig.Outputs.empty());
    }

    SECTION("BakesExplicitTarget")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), "Effects/Test.fofx-1-info"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Test.fofx-1-info"));
    }

    SECTION("BakesPassSpecificShaders")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/Multi.fofx", MULTI_PASS_EFFECT);

        EffectBaker baker {local_rig.MakeContext()};
        REQUIRE_NOTHROW(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""));

        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-1-info"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-info"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-vert-spv"));
        CHECK(local_rig.Outputs.contains("Effects/Multi.fofx-2-frag-spv"));
    }

    SECTION("RejectsInvalidEffects")
    {
        vector<pair<string, string>> invalid_effects = {
            {"Effects/NoEffect.fofx", "[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n"},
            {"Effects/NoVertex.fofx", "[Effect]\n\n[FragmentShader]\nvoid main(void) { }\n"},
            {"Effects/NoFragment.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n"},
            {"Effects/BadVertex.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = ; }\n\n[FragmentShader]\nlayout(location = 0) out vec4 FragColor;\nvoid main(void) { FragColor = vec4(1.0); }\n"},
            {"Effects/BadFragment.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = vec4(0.0); }\n\n[FragmentShader]\nlayout(location = 0) out vec4 FragColor;\nvoid main(void) { FragColor = ; }\n"},
        };

        for (const auto& [path, content] : invalid_effects) {
            TestRig local_rig;
            local_rig.AddSourceFile(path, content);

            EffectBaker baker {local_rig.MakeContext()};
            CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), EffectBakerException);
        }
    }

    SECTION("ShaderCompilerDiagnosticsStayOnOneLogLine")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/BadVertex.fofx", "[Effect]\n\n[VertexShader]\nvoid main(void) { gl_Position = ; }\n\n[FragmentShader]\nlayout(location = 0) out vec4 FragColor;\nvoid main(void) { FragColor = vec4(1.0); }\n");

        vector<string> captured_messages;
        logging::set_callback("effect-baker-diagnostic-line-test", [&](logging::type, string_view message, nptr<const stack_trace::catched_data>) { captured_messages.emplace_back(message); });
        auto remove_callback = scope_exit([]() noexcept { logging::set_callback("effect-baker-diagnostic-line-test", {}); });

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), EffectBakerException);

        auto diagnostic_it = std::ranges::find_if(captured_messages, [](const string& message) { return message.find("Failed to parse vertex shader") != string::npos; });
        REQUIRE(diagnostic_it != captured_messages.end());
        CHECK(diagnostic_it->find("\n- error") == string::npos);
        CHECK(diagnostic_it->find("\n- ERROR") == string::npos);
        CHECK(diagnostic_it->find("error :") == string::npos);
        CHECK(diagnostic_it->find("ERROR:") == string::npos);
        CHECK(diagnostic_it->find("Shader compiler diagnostic:") != string::npos);
        CHECK(diagnostic_it->find("syntax error") != string::npos);
    }

    SECTION("RejectsMismatchedKnownUniformBuffer")
    {
        TestRig local_rig;
        local_rig.AddSourceFile("Effects/BadProjBuffer.fofx", EFFECT_WITH_BAD_PROJ_BUFFER);

        EffectBaker baker {local_rig.MakeContext()};
        CHECK_THROWS_AS(baker.BakeFiles(local_rig.GetAllSourceFiles(), ""), EffectBakerException);
    }
}

TEST_CASE("EffectBakerBakesAuxiliaryBindings")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Aux.fofx", EFFECT_WITH_AUX_BINDINGS);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    auto info = ConfigFile(rig.GetOutputText("Effects/Aux.fofx-1-info"));

    REQUIRE(info.HasSection("EffectInfo"));
    CHECK(info.GetAsInt("EffectInfo", "IndoorMaskTex", -1) == 2);
    CHECK(info.GetAsInt("EffectInfo", "RandomValueBuf", -1) == 4);
    CHECK(info.GetAsInt("EffectInfo", "ScriptValueBuf", -1) == 5);
    CHECK(info.GetAsInt("EffectInfo", "CameraBuf", -1) == 6);
}

TEST_CASE("EffectBakerBakesExplicitBindings")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    auto info = ConfigFile(rig.GetOutputText("Effects/Test.fofx-1-info"));

    REQUIRE(info.HasSection("EffectInfo"));
    CHECK(info.GetAsInt("EffectInfo", "MainTex", -1) == 0);
    CHECK(info.GetAsInt("EffectInfo", "ProjBuf", -1) == 0);
    CHECK(info.GetAsInt("EffectInfo", "TimeBuf", -1) == 3);
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-vert-spv"));
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-frag-spv"));
}

// vkd3d-shader validates the container checksum before it disassembles, so readable bytecode is well-formed bytecode
static auto DisassembleDxbc(const vector<uint8_t>& dxbc) -> string
{
    vkd3d_shader_compile_info compile_info {};
    compile_info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    compile_info.source.code = dxbc.data();
    compile_info.source.size = dxbc.size();
    compile_info.source_type = VKD3D_SHADER_SOURCE_DXBC_TPF;
    compile_info.target_type = VKD3D_SHADER_TARGET_D3D_ASM;
    compile_info.log_level = VKD3D_SHADER_LOG_ERROR;

    vkd3d_shader_code text {};
    nptr<char> messages {};
    int32_t result = vkd3d_shader_compile(&compile_info, &text, messages.get_pp());
    auto messages_holder = make_unique_del_ptr(messages, vkd3d_shader_free_messages);
    auto text_holder = scope_exit([&text]() noexcept { vkd3d_shader_free_shader_code(&text); });
    FO_VERIFY_AND_THROW(result >= 0, "Baked DXBC does not disassemble", result, messages ? string(messages.get()) : string());

    nptr<const void> code = text.code;
    FO_VERIFY_AND_THROW(code, "Disassembly produced no text");
    return string(code.reinterpret_as<char>().get(), text.size);
}

TEST_CASE("EffectBakerBakesDirect3DBytecode")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);
    rig.AddSourceFile("Effects/Dynamic.fofx", EFFECT_WITH_DYNAMIC_UNIFORM_INDEX);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    // Direct3D consumes compiled bytecode, so no HLSL source reaches a client
    CHECK_FALSE(rig.Outputs.contains("Effects/Test.fofx-1-vert-hlsl"));
    CHECK_FALSE(rig.Outputs.contains("Effects/Test.fofx-1-frag-hlsl"));
    REQUIRE(rig.Outputs.contains("Effects/Test.fofx-1-vert-dxbc"));
    REQUIRE(rig.Outputs.contains("Effects/Test.fofx-1-frag-dxbc"));
    REQUIRE(rig.Outputs.contains("Effects/Dynamic.fofx-1-vert-dxbc"));

    CHECK(DisassembleDxbc(rig.Outputs.at("Effects/Test.fofx-1-vert-dxbc")).find("vs_4_0") != string::npos);
    CHECK(DisassembleDxbc(rig.Outputs.at("Effects/Test.fofx-1-frag-dxbc")).find("ps_4_0") != string::npos);

    // A register-indexed constant buffer must be declared as such, or a driver may lay it out for constant offsets only
    string dynamic_vertex = DisassembleDxbc(rig.Outputs.at("Effects/Dynamic.fofx-1-vert-dxbc"));
    CHECK(dynamic_vertex.find("dynamicIndexed") != string::npos);
    CHECK(dynamic_vertex.find("immediateIndexed") == string::npos);
}

static constexpr uint32_t AON9_TAG = 'A' | ('o' << 8) | ('n' << 16) | ('9' << 24);

struct Level9Code
{
    vector<uint32_t> Tokens {};
    vector<array<uint16_t, 4>> Constants {};
    vector<array<uint8_t, 3>> Samplers {};
    vector<uint16_t> PositionOffsets {};
};

// Reads the Aon9 chunk the way the level 9 runtime does: a header of four words, five table descriptors, then tables
static auto ReadLevel9Code(const vector<uint8_t>& dxbc) -> Level9Code
{
    vkd3d_shader_code code {};
    code.code = dxbc.data();
    code.size = dxbc.size();

    vkd3d_shader_dxbc_desc desc {};
    nptr<char> messages {};
    int32_t result = vkd3d_shader_parse_dxbc(&code, 0, &desc, messages.get_pp());
    auto messages_holder = make_unique_del_ptr(messages, vkd3d_shader_free_messages);
    FO_VERIFY_AND_THROW(result >= 0, "Baked DXBC does not parse", result);
    auto desc_holder = scope_exit([&desc]() noexcept { vkd3d_shader_free_dxbc(&desc); });
    FO_VERIFY_AND_THROW(desc.section_count != 0 && desc.sections[0].tag == AON9_TAG, "Level 9 code is not the first section");

    nptr<const void> section_code = desc.sections[0].data.code;
    FO_VERIFY_AND_THROW(section_code, "Level 9 section is empty");
    const_span<uint8_t> aon9 {section_code.reinterpret_as<const uint8_t>().get(), desc.sections[0].data.size};

    size_t pos = 0;
    uint32_t chunk_size = span_read_object<uint32_t>(aon9, pos);
    FO_VERIFY_AND_THROW(chunk_size == aon9.size(), "Level 9 chunk size disagrees with the section", chunk_size, aon9.size());
    pos += sizeof(uint32_t);
    uint32_t bytecode_size = span_read_object<uint32_t>(aon9, pos);
    uint32_t bytecode_offset = span_read_object<uint32_t>(aon9, pos);
    array<uint16_t, 10> tables {};

    for (uint16_t& value : tables) {
        value = span_read_object<uint16_t>(aon9, pos);
    }

    Level9Code level9;
    level9.Tokens.resize(bytecode_size / sizeof(uint32_t));
    size_t bytecode_pos = bytecode_offset;
    const_span<uint8_t> bytecode = span_read_bytes(aon9, bytecode_pos, bytecode_size);
    memory::copy(level9.Tokens.data(), bytecode.data(), bytecode.size());

    for (size_t i = 0, entry_pos = tables[1]; i < tables[0]; i++) {
        array<uint16_t, 4> entry {};

        for (uint16_t& value : entry) {
            value = span_read_object<uint16_t>(aon9, entry_pos);
        }

        entry_pos += sizeof(uint32_t);
        level9.Constants.emplace_back(entry);
    }

    for (size_t i = 0, entry_pos = tables[7]; i < tables[6]; i++) {
        array<uint8_t, 3> entry {};

        for (uint8_t& value : entry) {
            value = span_read_object<uint8_t>(aon9, entry_pos);
        }

        entry_pos += sizeof(uint8_t);
        level9.Samplers.emplace_back(entry);
    }

    for (size_t i = 0, entry_pos = tables[9]; i < tables[8]; i++) {
        uint16_t kind = span_read_object<uint16_t>(aon9, entry_pos);
        FO_VERIFY_AND_THROW(kind == 0, "Level 9 runtime constant is not the position offset", kind);
        level9.PositionOffsets.emplace_back(span_read_object<uint16_t>(aon9, entry_pos));
    }

    return level9;
}

// The Direct3D 9 validator rules vkd3d-shader output breaks, which a level 9 driver enforces
static void CheckLevel9ValidatorRules(const vector<uint32_t>& tokens)
{
    auto register_type = [](uint32_t token) -> uint32_t { return ((token >> 28) & 0x7) | ((token >> 8) & 0x18); };

    for (size_t pos = 1; pos < tokens.size() && tokens[pos] != 0x0000FFFF;) {
        uint32_t opcode = tokens[pos] & 0xFFFF;
        size_t length = (tokens[pos] >> 24) & 0xF;
        bool declaration = opcode == 0x1F || opcode == 0x51 || opcode == 0x30 || opcode == 0x2F;

        if (!declaration && opcode != 0x25) {
            set<uint32_t> constants;
            set<uint32_t> inputs;
            int32_t constant_reads = 0;

            for (size_t i = pos + 2; i <= pos + length; i++) {
                if (register_type(tokens[i]) == 2) {
                    constants.emplace(tokens[i] & 0x27FF);
                    constant_reads++;
                }
                else if (register_type(tokens[i]) == 1) {
                    inputs.emplace(tokens[i] & 0x7FF);
                }

                if ((tokens[i] & (1u << 13)) != 0) {
                    i++;
                }
            }

            CHECK(constants.size() <= 1);
            CHECK(constant_reads <= 2);
            CHECK(inputs.size() <= 1);
        }

        if (opcode == 0x42) {
            CHECK(((tokens[pos + 2] >> 16) & 0xFF) == 0xE4);
        }

        if (opcode == 0x41) {
            CHECK(((tokens[pos + 1] >> 16) & 0xF) == 0xF);
        }

        if (!declaration && length != 0 && register_type(tokens[pos + 1]) == 8) {
            CHECK(opcode == 0x01);
        }

        pos += 1 + length;
    }
}

TEST_CASE("EffectBakerBakesDirect3DLevel9Code")
{
    using namespace BakerTests;

    TestRig rig;
    OverrideSetting(rig.Settings.Baking.Direct3DLevel9Shaders, true);
    rig.AddSourceFile("Effects/Level9.fofx", EFFECT_FOR_LEVEL9);
    rig.AddSourceFile("Effects/3D_Level9.fofx", EFFECT_FOR_LEVEL9);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    const vector<uint8_t>& vertex_dxbc = rig.Outputs.at("Effects/Level9.fofx-1-vert-dxbc");
    const vector<uint8_t>& pixel_dxbc = rig.Outputs.at("Effects/Level9.fofx-1-frag-dxbc");

    // Feature level 10.0 and above keep running the Shader Model 4.0 code of the same container
    CHECK(DisassembleDxbc(vertex_dxbc).find("vs_4_0") != string::npos);
    CHECK(DisassembleDxbc(pixel_dxbc).find("ps_4_0") != string::npos);

    Level9Code vertex = ReadLevel9Code(vertex_dxbc);
    REQUIRE(vertex.Tokens.size() > 8);
    CHECK(vertex.Tokens.front() == 0xFFFE0201);
    CHECK(vertex.Tokens.back() == 0x0000FFFF);
    CheckLevel9ValidatorRules(vertex.Tokens);

    // ProjBuf lands on a Direct3D 9 register range, and the position goes through the runtime half-pixel offset last
    REQUIRE(vertex.Constants.size() == 1);
    CHECK(vertex.Constants[0][0] == 0);
    CHECK(vertex.Constants[0][1] == 0);
    REQUIRE(vertex.PositionOffsets.size() == 1);
    CHECK(vertex.PositionOffsets[0] >= vertex.Constants[0][3] + vertex.Constants[0][2]);
    size_t fixup = vertex.Tokens.size() - 1 - 8;
    CHECK(vertex.Tokens[fixup] == 0x04000004);
    CHECK(vertex.Tokens[fixup + 1] == 0xC0030000);
    CHECK(vertex.Tokens[fixup + 3] == (0xA0E40000u | vertex.PositionOffsets[0]));
    CHECK(vertex.Tokens[fixup + 5] == 0x02000001);
    CHECK(vertex.Tokens[fixup + 6] == 0xC00C0000);

    Level9Code pixel = ReadLevel9Code(pixel_dxbc);
    REQUIRE(pixel.Tokens.size() > 8);
    CHECK(pixel.Tokens.front() == 0xFFFF0201);
    CheckLevel9ValidatorRules(pixel.Tokens);
    CHECK(pixel.PositionOffsets.empty());

    // EggBuf is buffer 1 and MainTex is texture 0 through sampler 0, wherever the level 9 code keeps them
    REQUIRE_FALSE(pixel.Constants.empty());

    for (const array<uint16_t, 4>& constant : pixel.Constants) {
        CHECK(constant[0] == 1);
        CHECK(constant[3] + constant[2] <= 32);
    }

    REQUIRE(pixel.Samplers.size() == 1);
    CHECK(pixel.Samplers[0][0] == 0);
    CHECK(pixel.Samplers[0][1] == 0);

    // Level 9 does not support 3D: a model effect keeps only its Shader Model 4.0 code
    if constexpr (FO_ENABLE_3D) {
        CHECK_THROWS(ReadLevel9Code(rig.Outputs.at("Effects/3D_Level9.fofx-1-vert-dxbc")));
        CHECK_THROWS(ReadLevel9Code(rig.Outputs.at("Effects/3D_Level9.fofx-1-frag-dxbc")));
    }
    else {
        CHECK_FALSE(rig.Outputs.contains("Effects/3D_Level9.fofx-1-vert-dxbc"));
    }
}

TEST_CASE("EffectBakerRejectsEffectsBeyondDirect3DLevel9")
{
    using namespace BakerTests;

    SECTION("FailsTheBakeWhenLevel9IsRequested")
    {
        TestRig rig;
        OverrideSetting(rig.Settings.Baking.Direct3DLevel9Shaders, true);
        rig.AddSourceFile("Effects/Beyond.fofx", EFFECT_BEYOND_LEVEL9);

        EffectBaker baker {rig.MakeContext()};
        CHECK_THROWS(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
    }

    SECTION("BakesWithoutLevel9Code")
    {
        TestRig rig;
        rig.AddSourceFile("Effects/Beyond.fofx", EFFECT_BEYOND_LEVEL9);

        EffectBaker baker {rig.MakeContext()};
        REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));
        CHECK_THROWS(ReadLevel9Code(rig.Outputs.at("Effects/Beyond.fofx-1-frag-dxbc")));
    }
}

TEST_CASE("EffectBakerBakesSdlGpuFlavors")
{
    using namespace BakerTests;

    TestRig rig;
    rig.AddSourceFile("Effects/Test.fofx", VALID_EFFECT);

    EffectBaker baker {rig.MakeContext()};

    REQUIRE_NOTHROW(baker.BakeFiles(rig.GetAllSourceFiles(), ""));

    // The native Vulkan SPIR-V and the SDL_GPU-convention SPIR-V are baked as separate flavors
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-vert-spv"));
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-vert-spv_sdl"));
    CHECK(rig.Outputs.contains("Effects/Test.fofx-1-frag-spv_sdl"));

    auto info = ConfigFile(rig.GetOutputText("Effects/Test.fofx-1-info"));

    REQUIRE(info.HasSection("EffectInfoSdl"));
    // VALID_EFFECT: vertex has ProjBuf (1 UBO, no samplers); fragment has MainTex (1 sampler) + TimeBuf (1 UBO)
    CHECK(info.GetAsInt("EffectInfoSdl", "VertexSamplers", -1) == 0);
    CHECK(info.GetAsInt("EffectInfoSdl", "VertexUniformBuffers", -1) == 1);
    CHECK(info.GetAsInt("EffectInfoSdl", "FragmentSamplers", -1) == 1);
    CHECK(info.GetAsInt("EffectInfoSdl", "FragmentUniformBuffers", -1) == 1);
    CHECK(info.GetAsInt("EffectInfoSdl", "VertProjBuf", -1) == 0);
    CHECK(info.GetAsInt("EffectInfoSdl", "FragMainTex", -1) == 0);
    // TimeBuf is authored at binding 3 but gets the dense SDL slot 0 within the fragment UBO set
    CHECK(info.GetAsInt("EffectInfoSdl", "FragTimeBuf", -1) == 0);

    // The native [EffectInfo] program-wide bindings are untouched by the SDL additions
    REQUIRE(info.HasSection("EffectInfo"));
    CHECK(info.GetAsInt("EffectInfo", "TimeBuf", -1) == 3);
}

FO_END_NAMESPACE
