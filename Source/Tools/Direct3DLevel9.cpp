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

#include "Direct3DLevel9.h"

FO_DISABLE_WARNINGS_PUSH()
#include "vkd3d_shader.h"
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

// Shader Model 2 tokens (d3d9types.h): an instruction keeps its opcode in bits 0-15 and its length in 24-27; a register keeps
// its number in 0-10, its type in 28-30 and 11-12, relative addressing in 13, the mask or the swizzle from 16, modifiers above
static constexpr uint32_t SM2_OPCODE_MASK = 0xFFFF;
static constexpr uint32_t SM2_LENGTH_SHIFT = 24;
static constexpr uint32_t SM2_LENGTH_MASK = 0xF;
static constexpr uint32_t SM2_COMMENT_OPCODE = 0xFFFE;
static constexpr uint32_t SM2_COMMENT_LENGTH_SHIFT = 16;
static constexpr uint32_t SM2_COMMENT_LENGTH_MASK = 0x7FFF;
static constexpr uint32_t SM2_END_TOKEN = 0x0000FFFF;
static constexpr uint32_t SM2_REGISTER_NUMBER_MASK = 0x7FF;
static constexpr uint32_t SM2_REGISTER_TYPE_MASK = (0x7u << 28) | (0x3u << 11);
static constexpr uint32_t SM2_RELATIVE_ADDRESSING = 1u << 13;
static constexpr uint32_t SM2_SELECT_SHIFT = 16;
static constexpr uint32_t SM2_SWIZZLE_MASK = 0xFFu << SM2_SELECT_SHIFT;
static constexpr uint32_t SM2_WRITE_MASK_MASK = 0xFu << SM2_SELECT_SHIFT;
static constexpr uint32_t SM2_RESULT_MODIFIERS_MASK = 0xFFu << 20;
static constexpr uint32_t SM2_SOURCE_MODIFIER_MASK = 0xFu << 24;
static constexpr uint32_t SM2_PARAMETER_TOKEN = 0x80000000u;
static constexpr uint32_t SM2_IDENTITY_SWIZZLE = 0xE4;
static constexpr uint32_t SM2_FULL_WRITE_MASK = 0xF;
static constexpr uint32_t SM2_USAGE_MASK = 0x1F;
static constexpr uint32_t SM2_USAGE_INDEX_SHIFT = 16;
static constexpr uint32_t SM2_USAGE_INDEX_MASK = 0xF;
static constexpr uint32_t SM2_SAMPLER_TYPE_SHIFT = 27;
static constexpr uint32_t SM2_SAMPLER_TYPE_MASK = 0xF;
static constexpr uint32_t SM2_TEX_CONTROL_SHIFT = 16;
static constexpr uint32_t SM2_TEX_CONTROL_MASK = 0xFF;

static constexpr uint32_t SM2_OP_NOP = 0x00;
static constexpr uint32_t SM2_OP_MOV = 0x01;
static constexpr uint32_t SM2_OP_MAD = 0x04;
static constexpr uint32_t SM2_OP_LRP = 0x12;
static constexpr uint32_t SM2_OP_M4X4 = 0x14;
static constexpr uint32_t SM2_OP_M4X3 = 0x15;
static constexpr uint32_t SM2_OP_M3X4 = 0x16;
static constexpr uint32_t SM2_OP_M3X3 = 0x17;
static constexpr uint32_t SM2_OP_M3X2 = 0x18;
static constexpr uint32_t SM2_OP_CALL = 0x19;
static constexpr uint32_t SM2_OP_CALLNZ = 0x1A;
static constexpr uint32_t SM2_OP_LOOP = 0x1B;
static constexpr uint32_t SM2_OP_RET = 0x1C;
static constexpr uint32_t SM2_OP_ENDLOOP = 0x1D;
static constexpr uint32_t SM2_OP_LABEL = 0x1E;
static constexpr uint32_t SM2_OP_DCL = 0x1F;
static constexpr uint32_t SM2_OP_POW = 0x20;
static constexpr uint32_t SM2_OP_CRS = 0x21;
static constexpr uint32_t SM2_OP_SGN = 0x22;
static constexpr uint32_t SM2_OP_NRM = 0x24;
static constexpr uint32_t SM2_OP_SINCOS = 0x25;
static constexpr uint32_t SM2_OP_REP = 0x26;
static constexpr uint32_t SM2_OP_ENDREP = 0x27;
static constexpr uint32_t SM2_OP_IF = 0x28;
static constexpr uint32_t SM2_OP_IFC = 0x29;
static constexpr uint32_t SM2_OP_ELSE = 0x2A;
static constexpr uint32_t SM2_OP_ENDIF = 0x2B;
static constexpr uint32_t SM2_OP_BREAK = 0x2C;
static constexpr uint32_t SM2_OP_BREAKC = 0x2D;
static constexpr uint32_t SM2_OP_DEFB = 0x2F;
static constexpr uint32_t SM2_OP_DEFI = 0x30;
static constexpr uint32_t SM2_OP_TEXKILL = 0x41;
static constexpr uint32_t SM2_OP_TEX = 0x42;
static constexpr uint32_t SM2_OP_DEF = 0x51;
static constexpr uint32_t SM2_OP_DP2ADD = 0x5A;
static constexpr uint32_t SM2_OP_TEXLDD = 0x5D;
static constexpr uint32_t SM2_OP_SETP = 0x5E;
static constexpr uint32_t SM2_OP_TEXLDL = 0x5F;
static constexpr uint32_t SM2_OP_BREAKP = 0x60;

static constexpr uint32_t SM2_USAGE_NORMAL = 3;
static constexpr uint32_t SM2_USAGE_TEXCOORD = 5;
static constexpr uint32_t SM2_SAMPLER_2D = 2;
static constexpr uint32_t SM2_SAMPLER_CUBE = 3;
static constexpr uint32_t SM2_SAMPLER_VOLUME = 4;

// Level 9.3 runs vs_2_x and ps_2_x: 32 temporaries each, 256 vertex and 512 pixel instruction slots, 256 vertex and
// 32 pixel float constants, 16 vertex inputs and 8 interpolated texture coordinates
static constexpr uint32_t LEVEL9_VERTEX_VERSION = 0xFFFE0201;
static constexpr uint32_t LEVEL9_PIXEL_VERSION = 0xFFFF0201;
static constexpr uint32_t VKD3D_VERTEX_VERSION = 0xFFFE0201;
static constexpr uint32_t VKD3D_PIXEL_VERSION = 0xFFFF0200;
static constexpr uint32_t LEVEL9_MAX_TEMPS = 32;
static constexpr int32_t LEVEL9_MAX_VERTEX_SLOTS = 256;
static constexpr int32_t LEVEL9_MAX_PIXEL_SLOTS = 512;
static constexpr uint32_t LEVEL9_MAX_VERTEX_CONSTANTS = 256;
static constexpr uint32_t LEVEL9_MAX_PIXEL_CONSTANTS = 32;
static constexpr uint32_t LEVEL9_MAX_VERTEX_INPUTS = 16;
static constexpr uint32_t LEVEL9_MAX_TEXCOORDS = 8;

// Reflection data of the Shader Model 4.0 container (d3d11shader.h)
static constexpr uint32_t D3D_SIT_CBUFFER = 0;
static constexpr uint32_t D3D_SIT_TEXTURE = 2;
static constexpr uint32_t D3D_SIT_SAMPLER = 3;
static constexpr uint32_t D3D_SVT_BOOL = 1;
static constexpr uint32_t D3D_SVT_INT = 2;
static constexpr uint32_t D3D_SVT_FLOAT = 3;
static constexpr uint32_t D3D_SVT_UINT = 19;
static constexpr uint32_t D3DXRS_FLOAT4 = 2;
static constexpr uint32_t D3DXRS_SAMPLER = 3;

// Aon9 constant conversions: how the runtime turns a constant buffer value into a Direct3D 9 float
static constexpr uint8_t AON9_CONVERSION_FLOAT = 0;
static constexpr uint8_t AON9_CONVERSION_BOOL = 1;
static constexpr uint8_t AON9_CONVERSION_INT = 2;
static constexpr uint8_t AON9_CONVERSION_UINT = 3;
static constexpr uint16_t AON9_RUNTIME_POSITION_OFFSET = 0;
static constexpr uint32_t AON9_HEADER_SIZE = 36;

enum class Sm2Register : uint8_t
{
    Temp = 0,
    Input = 1,
    Const = 2,
    Texture = 3, // The address register in a vertex shader
    RasterizerOutput = 4,
    AttributeOutput = 5,
    TexCoordOutput = 6,
    ConstInt = 7,
    ColorOutput = 8,
    DepthOutput = 9,
    Sampler = 10,
    Const2 = 11,
    Const3 = 12,
    Const4 = 13,
    ConstBool = 14,
    Misc = 17,
};

enum class Sm2OperandKind : uint8_t
{
    Destination,
    Source,
    Address,
    Data,
};

struct Sm2Operand
{
    size_t Index {};
    Sm2OperandKind Kind {};
};

using Sm2Instruction = vector<uint32_t>;

struct Sm2Program
{
    uint32_t Version {};
    vector<Sm2Instruction> Instructions {};
    vector<uint8_t> ConstantTable {};
};

struct ConstantTableEntry
{
    string Name {};
    uint32_t RegisterSet {};
    uint32_t RegisterIndex {};
    uint32_t RegisterCount {};
};

struct ResourceBinding
{
    string Name {};
    uint32_t Type {};
    uint32_t BindPoint {};
};

struct BufferVariable
{
    uint32_t Buffer {};
    uint32_t Offset {};
    uint32_t Size {};
    uint32_t Type {};
};

struct ResourceDefinitions
{
    vector<ResourceBinding> Bindings {};
    unordered_map<string, BufferVariable> Variables {};
};

struct SignatureElement
{
    string Name {};
    uint32_t SemanticIndex {};
    uint32_t Register {};
    uint32_t FirstComponent {};
    uint32_t ComponentCount {};
};

struct Level9ConstantMapping
{
    uint16_t Buffer {};
    uint16_t StartRegister {};
    uint16_t RegisterCount {};
    uint16_t TargetRegister {};
    uint8_t Conversion {};
};

struct Level9SamplerMapping
{
    uint8_t Texture {};
    uint8_t Sampler {};
    uint8_t TargetSampler {};
};

struct Level9Tables
{
    vector<Level9ConstantMapping> Constants {};
    vector<Level9SamplerMapping> Samplers {};
    uint32_t PositionOffsetRegister {};
    bool HasPositionOffset {};
};

// Temporaries the rewrite adds above the shader's own: two for moved sources, one for a redirected result and one
// holding the vertex position until the level 9 fixup
struct ScratchRegisters
{
    uint32_t Source0 {};
    uint32_t Source1 {};
    uint32_t Result {};
    uint32_t Position {};
};

// Little-endian four-character codes of the container sections and of the constant table comment
static constexpr uint32_t TAG_AON9 = 'A' | ('o' << 8) | ('n' << 16) | ('9' << 24);
static constexpr uint32_t TAG_RDEF = 'R' | ('D' << 8) | ('E' << 16) | ('F' << 24);
static constexpr uint32_t TAG_ISGN = 'I' | ('S' << 8) | ('G' << 16) | ('N' << 24);
static constexpr uint32_t TAG_OSGN = 'O' | ('S' << 8) | ('G' << 16) | ('N' << 24);
static constexpr uint32_t TAG_CTAB = 'C' | ('T' << 8) | ('A' << 16) | ('B' << 24);

// Direct3D 9 declaration usages, in the order of D3DDECLUSAGE
static constexpr string_view SM2_USAGE_NAMES[] = {"POSITION", "BLENDWEIGHT", "BLENDINDICES", "NORMAL", "PSIZE", "TEXCOORD", "TANGENT", "BINORMAL", "TESSFACTOR", "POSITIONT", "COLOR"};

static auto ParseSm2Program(const_span<uint8_t> bytecode, string_view name) -> Sm2Program;
static auto ParseConstantTable(const_span<uint8_t> data) -> vector<ConstantTableEntry>;
static auto ParseResourceDefinitions(const_span<uint8_t> data, string_view name) -> ResourceDefinitions;
static auto ParseSignature(const_span<uint8_t> data, string_view name) -> vector<SignatureElement>;
static auto MapResources(const vector<ConstantTableEntry>& constant_table, const ResourceDefinitions& resources, string_view name) -> Level9Tables;
static auto RemapSignatures(const vector<Sm2Instruction>& instructions, bool is_vertex, const vector<SignatureElement>& inputs, const vector<SignatureElement>& outputs, const ScratchRegisters& scratch, string_view name) -> vector<Sm2Instruction>;
static void LegalizeInstruction(Sm2Instruction instruction, const ScratchRegisters& scratch, const unordered_map<uint32_t, uint32_t>& sampler_types, vector<Sm2Instruction>& output, string_view name);
static auto BuildAon9Chunk(uint32_t version, const vector<Sm2Instruction>& instructions, const Level9Tables& tables) -> vector<uint8_t>;
static auto SerializeContainer(const vkd3d_shader_dxbc_desc& container, const_span<uint8_t> aon9, string_view name) -> vector<uint8_t>;
static auto GetOperands(const Sm2Instruction& instruction) -> small_vector<Sm2Operand, 8>;
static auto GetInstructionSlots(uint32_t opcode) -> int32_t;
static auto IsFlowControl(uint32_t opcode) -> bool;
static auto FindSignatureElement(const vector<SignatureElement>& signature, string_view semantic, uint32_t index, string_view name) -> const SignatureElement&;
static auto GetOpcode(uint32_t token) -> uint32_t;
static auto GetRegisterType(uint32_t token) -> Sm2Register;
static auto GetRegisterNumber(uint32_t token) -> uint32_t;
static auto SetRegister(uint32_t token, Sm2Register type, uint32_t number) -> uint32_t;
static auto SetSwizzle(uint32_t token, uint32_t swizzle) -> uint32_t;
static auto GetSwizzle(uint32_t token) -> uint32_t;
static auto ShiftSwizzle(uint32_t swizzle, uint32_t first_component, uint32_t component_count) -> uint32_t;
static auto MakeTempDestination(uint32_t number, uint32_t write_mask) -> uint32_t;
static auto MakeTempSource(uint32_t number, uint32_t swizzle) -> uint32_t;
static auto MakeMov(uint32_t destination, const_span<uint32_t> source) -> Sm2Instruction;
static auto ReadStringAt(const_span<uint8_t> data, size_t pos) -> string;

auto AddDirect3DLevel9Code(const_span<uint8_t> sm4_container, const_span<uint8_t> sm2_bytecode, bool is_vertex, string_view name) -> vector<uint8_t>
{
    FO_TRACE_ZONE(Baking);

    vkd3d_shader_code container_code {};
    container_code.code = sm4_container.data();
    container_code.size = sm4_container.size();

    vkd3d_shader_dxbc_desc container {};
    nptr<char> messages {};
    int32_t parse_result = vkd3d_shader_parse_dxbc(&container_code, 0, &container, messages.get_pp());
    auto messages_holder = make_unique_del_ptr(messages, vkd3d_shader_free_messages);

    if (parse_result < 0) {
        throw Direct3DLevel9Exception("Shader Model 4.0 container does not parse", name, parse_result, messages ? string(messages.get()) : string());
    }

    auto container_holder = scope_exit([&container]() noexcept { vkd3d_shader_free_dxbc(&container); });

    auto find_section = [&container, name](uint32_t tag, string_view tag_name) -> const_span<uint8_t> {
        for (size_t i = 0; i < container.section_count; i++) {
            const vkd3d_shader_dxbc_section_desc& section = container.sections[i];
            nptr<const void> section_code = section.data.code;

            if (section.tag == tag && section_code && section.data.size != 0) {
                return {section_code.reinterpret_as<const uint8_t>().get(), section.data.size};
            }
        }

        throw Direct3DLevel9Exception("Shader Model 4.0 container has no section", name, tag_name);
    };

    ResourceDefinitions resources = ParseResourceDefinitions(find_section(TAG_RDEF, "RDEF"), name);
    vector<SignatureElement> inputs = ParseSignature(find_section(TAG_ISGN, "ISGN"), name);
    vector<SignatureElement> outputs = ParseSignature(find_section(TAG_OSGN, "OSGN"), name);
    Sm2Program program = ParseSm2Program(sm2_bytecode, name);

    if (program.Version != (is_vertex ? VKD3D_VERTEX_VERSION : VKD3D_PIXEL_VERSION)) {
        throw Direct3DLevel9Exception("Shader Model 2 bytecode has an unexpected version", name, program.Version);
    }

    Level9Tables tables = MapResources(ParseConstantTable(program.ConstantTable), resources, name);

    // Survey the program: registers it takes, sampler dimensions, and anything level 9 cannot run
    int32_t max_temp = -1;
    int32_t max_const = -1;
    unordered_map<uint32_t, uint32_t> sampler_types;

    for (const Level9ConstantMapping& mapping : tables.Constants) {
        max_const = std::max(max_const, numeric_cast<int32_t>(mapping.TargetRegister + mapping.RegisterCount) - 1);
    }

    for (const Sm2Instruction& instruction : program.Instructions) {
        uint32_t opcode = GetOpcode(instruction[0]);

        if (opcode == SM2_OP_SETP || opcode == SM2_OP_BREAKP) {
            throw Direct3DLevel9Exception("Level 9 has no predication", name);
        }

        if (!is_vertex && IsFlowControl(opcode)) {
            throw Direct3DLevel9Exception("Level 9 pixel shaders have no flow control", name, opcode);
        }

        if (opcode == SM2_OP_DCL && GetRegisterType(instruction[2]) == Sm2Register::Sampler) {
            sampler_types.emplace(GetRegisterNumber(instruction[2]), (instruction[1] >> SM2_SAMPLER_TYPE_SHIFT) & SM2_SAMPLER_TYPE_MASK);
        }

        for (const Sm2Operand& operand : GetOperands(instruction)) {
            if (operand.Kind != Sm2OperandKind::Destination && operand.Kind != Sm2OperandKind::Source) {
                continue;
            }

            uint32_t token = instruction[operand.Index];
            Sm2Register type = GetRegisterType(token);

            if (type == Sm2Register::Temp) {
                max_temp = std::max(max_temp, numeric_cast<int32_t>(GetRegisterNumber(token)));
            }
            else if (type == Sm2Register::Const && (token & SM2_RELATIVE_ADDRESSING) == 0) {
                max_const = std::max(max_const, numeric_cast<int32_t>(GetRegisterNumber(token)));
            }
            else if ((type == Sm2Register::ConstInt || type == Sm2Register::ConstBool) && opcode != SM2_OP_DEFI && opcode != SM2_OP_DEFB) {
                throw Direct3DLevel9Exception("Level 9 code reads an integer or boolean constant set by the application", name);
            }
            else if (type == Sm2Register::Const2 || type == Sm2Register::Const3 || type == Sm2Register::Const4) {
                throw Direct3DLevel9Exception("Level 9 code addresses more than 2048 constants", name);
            }
            else if (type == Sm2Register::DepthOutput || type == Sm2Register::Misc) {
                throw Direct3DLevel9Exception("Level 9 has no depth output, pixel position or face register", name, static_cast<uint32_t>(type));
            }
        }
    }

    ScratchRegisters scratch;
    scratch.Source0 = numeric_cast<uint32_t>(max_temp + 1);
    scratch.Source1 = scratch.Source0 + 1;
    scratch.Result = scratch.Source0 + 2;
    scratch.Position = scratch.Source0 + 3;

    if ((is_vertex ? scratch.Position : scratch.Result) >= LEVEL9_MAX_TEMPS) {
        throw Direct3DLevel9Exception("Level 9 code needs more than 32 temporary registers", name, max_temp + 1);
    }

    // The runtime numbers level 9 inputs and outputs after the Shader Model 4.0 signature registers
    vector<Sm2Instruction> remapped = RemapSignatures(program.Instructions, is_vertex, inputs, outputs, scratch, name);

    vector<Sm2Instruction> legalized;
    legalized.reserve(remapped.size() * 2);
    bool position_written = false;

    for (Sm2Instruction& instruction : remapped) {
        if (is_vertex) {
            for (const Sm2Operand& operand : GetOperands(instruction)) {
                if (operand.Kind == Sm2OperandKind::Destination && GetRegisterType(instruction[operand.Index]) == Sm2Register::RasterizerOutput) {
                    if (GetRegisterNumber(instruction[operand.Index]) != 0) {
                        throw Direct3DLevel9Exception("Level 9 vertex shader writes fog or point size", name);
                    }

                    instruction[operand.Index] = SetRegister(instruction[operand.Index], Sm2Register::Temp, scratch.Position);
                    position_written = true;
                }
            }
        }

        LegalizeInstruction(std::move(instruction), scratch, sampler_types, legalized, name);
    }

    if (is_vertex) {
        if (!position_written) {
            throw Direct3DLevel9Exception("Vertex shader does not write the position", name);
        }

        tables.PositionOffsetRegister = numeric_cast<uint32_t>(max_const + 1);
        tables.HasPositionOffset = true;

        if (tables.PositionOffsetRegister >= LEVEL9_MAX_VERTEX_CONSTANTS) {
            throw Direct3DLevel9Exception("Level 9 vertex shader has no constant register left for the position offset", name);
        }

        // The runtime keeps the Direct3D 9 half-pixel correction in a constant, which the position goes through last:
        // mad oPos.xy, r.w, c, r / mov oPos.zw, r
        legalized.emplace_back(Sm2Instruction {SM2_OP_MAD | (4u << SM2_LENGTH_SHIFT), 0xC0030000u, 0x80FF0000u | scratch.Position, 0xA0E40000u | tables.PositionOffsetRegister, 0x80E40000u | scratch.Position});
        legalized.emplace_back(Sm2Instruction {SM2_OP_MOV | (2u << SM2_LENGTH_SHIFT), 0xC00C0000u, 0x80E40000u | scratch.Position});
    }
    else if (max_const >= numeric_cast<int32_t>(LEVEL9_MAX_PIXEL_CONSTANTS)) {
        throw Direct3DLevel9Exception("Level 9 pixel shader uses more than 32 constant registers", name, max_const + 1);
    }

    int32_t slots = 0;

    for (const Sm2Instruction& instruction : legalized) {
        slots += GetInstructionSlots(GetOpcode(instruction[0]));
    }

    if (slots > (is_vertex ? LEVEL9_MAX_VERTEX_SLOTS : LEVEL9_MAX_PIXEL_SLOTS)) {
        throw Direct3DLevel9Exception("Level 9 code exceeds the instruction slot limit", name, slots, is_vertex ? LEVEL9_MAX_VERTEX_SLOTS : LEVEL9_MAX_PIXEL_SLOTS);
    }

    vector<uint8_t> aon9 = BuildAon9Chunk(is_vertex ? LEVEL9_VERTEX_VERSION : LEVEL9_PIXEL_VERSION, legalized, tables);
    return SerializeContainer(container, aon9, name);
}

static auto ParseSm2Program(const_span<uint8_t> bytecode, string_view name) -> Sm2Program
{
    if (bytecode.size() % sizeof(uint32_t) != 0 || bytecode.size() < sizeof(uint32_t) * 2) {
        throw Direct3DLevel9Exception("Shader Model 2 bytecode is not a token stream", name, bytecode.size());
    }

    vector<uint32_t> tokens(bytecode.size() / sizeof(uint32_t));
    memory::copy(tokens.data(), bytecode.data(), bytecode.size());

    Sm2Program program;
    program.Version = tokens[0];
    size_t pos = 1;

    while (true) {
        if (pos >= tokens.size()) {
            throw Direct3DLevel9Exception("Shader Model 2 bytecode has no end token", name);
        }

        uint32_t token = tokens[pos];

        if (token == SM2_END_TOKEN) {
            break;
        }

        if (GetOpcode(token) == SM2_COMMENT_OPCODE) {
            size_t length = (token >> SM2_COMMENT_LENGTH_SHIFT) & SM2_COMMENT_LENGTH_MASK;

            if (length > tokens.size() - pos - 1) {
                throw Direct3DLevel9Exception("Shader Model 2 comment runs past the bytecode", name);
            }

            if (length != 0 && tokens[pos + 1] == TAG_CTAB) {
                program.ConstantTable.resize((length - 1) * sizeof(uint32_t));
                memory::copy(program.ConstantTable.data(), &tokens[pos + 2], program.ConstantTable.size());
            }

            pos += 1 + length;
            continue;
        }

        size_t length = (token >> SM2_LENGTH_SHIFT) & SM2_LENGTH_MASK;

        if (length > tokens.size() - pos - 1) {
            throw Direct3DLevel9Exception("Shader Model 2 instruction runs past the bytecode", name);
        }

        program.Instructions.emplace_back(tokens.begin() + numeric_cast<ptrdiff_t>(pos), tokens.begin() + numeric_cast<ptrdiff_t>(pos + 1 + length));
        pos += 1 + length;
    }

    if (program.ConstantTable.empty()) {
        throw Direct3DLevel9Exception("Shader Model 2 bytecode has no constant table", name);
    }

    return program;
}

static auto ParseConstantTable(const_span<uint8_t> data) -> vector<ConstantTableEntry>
{
    // D3DXSHADER_CONSTANTTABLE, offsets from its start: Size, Creator, Version, Constants, ConstantInfo, Flags, Target
    size_t pos = sizeof(uint32_t) * 3;
    uint32_t count = span_read_object<uint32_t>(data, pos);
    uint32_t info_offset = span_read_object<uint32_t>(data, pos);
    vector<ConstantTableEntry> entries;
    entries.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        // D3DXSHADER_CONSTANTINFO: Name, RegisterSet, RegisterIndex, RegisterCount, Reserved, TypeInfo, DefaultValue
        size_t info_pos = numeric_cast<size_t>(info_offset) + numeric_cast<size_t>(i) * 20;
        uint32_t name_offset = span_read_object<uint32_t>(data, info_pos);

        ConstantTableEntry entry;
        entry.Name = ReadStringAt(data, name_offset);
        entry.RegisterSet = span_read_object<uint16_t>(data, info_pos);
        entry.RegisterIndex = span_read_object<uint16_t>(data, info_pos);
        entry.RegisterCount = span_read_object<uint16_t>(data, info_pos);
        entries.emplace_back(std::move(entry));
    }

    return entries;
}

static auto ParseResourceDefinitions(const_span<uint8_t> data, string_view name) -> ResourceDefinitions
{
    // RDEF of Shader Model 4.0: constant buffer and binding tables, then the creator string
    size_t pos = 0;
    uint32_t buffer_count = span_read_object<uint32_t>(data, pos);
    uint32_t buffer_offset = span_read_object<uint32_t>(data, pos);
    uint32_t binding_count = span_read_object<uint32_t>(data, pos);
    uint32_t binding_offset = span_read_object<uint32_t>(data, pos);
    uint8_t minor_version = span_read_object<uint8_t>(data, pos);
    uint8_t major_version = span_read_object<uint8_t>(data, pos);

    if (major_version != 4) {
        throw Direct3DLevel9Exception("Resource definitions are not Shader Model 4", name, major_version, minor_version);
    }

    ResourceDefinitions resources;

    for (uint32_t i = 0; i < binding_count; i++) {
        size_t binding_pos = numeric_cast<size_t>(binding_offset) + numeric_cast<size_t>(i) * 32;
        uint32_t name_offset = span_read_object<uint32_t>(data, binding_pos);

        ResourceBinding binding;
        binding.Name = ReadStringAt(data, name_offset);
        binding.Type = span_read_object<uint32_t>(data, binding_pos);
        binding_pos += sizeof(uint32_t) * 3;
        binding.BindPoint = span_read_object<uint32_t>(data, binding_pos);
        resources.Bindings.emplace_back(std::move(binding));
    }

    for (uint32_t i = 0; i < buffer_count; i++) {
        size_t buffer_pos = numeric_cast<size_t>(buffer_offset) + numeric_cast<size_t>(i) * 24;
        string buffer_name = ReadStringAt(data, span_read_object<uint32_t>(data, buffer_pos));
        uint32_t variable_count = span_read_object<uint32_t>(data, buffer_pos);
        uint32_t variable_offset = span_read_object<uint32_t>(data, buffer_pos);

        auto binding = std::ranges::find_if(resources.Bindings, [&buffer_name](const ResourceBinding& b) { return b.Type == D3D_SIT_CBUFFER && b.Name == buffer_name; });

        if (binding == resources.Bindings.end()) {
            throw Direct3DLevel9Exception("Constant buffer has no binding", name, buffer_name);
        }

        for (uint32_t j = 0; j < variable_count; j++) {
            size_t variable_pos = numeric_cast<size_t>(variable_offset) + numeric_cast<size_t>(j) * 24;
            string variable_name = ReadStringAt(data, span_read_object<uint32_t>(data, variable_pos));

            BufferVariable variable;
            variable.Buffer = binding->BindPoint;
            variable.Offset = span_read_object<uint32_t>(data, variable_pos);
            variable.Size = span_read_object<uint32_t>(data, variable_pos);
            variable_pos += sizeof(uint32_t);
            size_t type_pos = span_read_object<uint32_t>(data, variable_pos) + sizeof(uint16_t);
            variable.Type = span_read_object<uint16_t>(data, type_pos);
            resources.Variables.emplace(variable_name, variable);
        }
    }

    return resources;
}

static auto ParseSignature(const_span<uint8_t> data, string_view name) -> vector<SignatureElement>
{
    size_t pos = 0;
    uint32_t count = span_read_object<uint32_t>(data, pos);
    uint32_t offset = span_read_object<uint32_t>(data, pos);
    vector<SignatureElement> elements;
    elements.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        // Name, SemanticIndex, SystemValueType, ComponentType, Register, Mask, ReadWriteMask
        size_t element_pos = numeric_cast<size_t>(offset) + numeric_cast<size_t>(i) * 24;

        SignatureElement element;
        element.Name = strex(ReadStringAt(data, span_read_object<uint32_t>(data, element_pos))).upper();
        element.SemanticIndex = span_read_object<uint32_t>(data, element_pos);
        element_pos += sizeof(uint32_t) * 2;
        element.Register = span_read_object<uint32_t>(data, element_pos);
        uint32_t mask = span_read_object<uint8_t>(data, element_pos) & SM2_FULL_WRITE_MASK;

        if (mask == 0) {
            throw Direct3DLevel9Exception("Signature element has no components", name, element.Name, element.SemanticIndex);
        }

        element.FirstComponent = numeric_cast<uint32_t>(std::countr_zero(mask));
        element.ComponentCount = numeric_cast<uint32_t>(std::popcount(mask));
        elements.emplace_back(std::move(element));
    }

    return elements;
}

static auto MapResources(const vector<ConstantTableEntry>& constant_table, const ResourceDefinitions& resources, string_view name) -> Level9Tables
{
    Level9Tables tables;

    for (const ConstantTableEntry& entry : constant_table) {
        if (entry.RegisterSet == D3DXRS_FLOAT4) {
            auto variable_it = resources.Variables.find(entry.Name);

            if (variable_it == resources.Variables.end()) {
                throw Direct3DLevel9Exception("Level 9 constant is not a constant buffer member", name, entry.Name);
            }

            // A member laid out mid-register would reach the level 9 code in the wrong components
            const BufferVariable& variable = variable_it->second;

            if (variable.Offset % 16 != 0) {
                throw Direct3DLevel9Exception("Level 9 constant does not start a constant buffer register", name, entry.Name, variable.Offset);
            }

            if (entry.RegisterCount * 16 > align_up(variable.Size, 16u)) {
                throw Direct3DLevel9Exception("Level 9 constant maps past its constant buffer member", name, entry.Name, entry.RegisterCount, variable.Size);
            }

            Level9ConstantMapping mapping;
            mapping.Buffer = numeric_cast<uint16_t>(variable.Buffer);
            mapping.StartRegister = numeric_cast<uint16_t>(variable.Offset / 16);
            mapping.RegisterCount = numeric_cast<uint16_t>(entry.RegisterCount);
            mapping.TargetRegister = numeric_cast<uint16_t>(entry.RegisterIndex);

            switch (variable.Type) {
            case D3D_SVT_FLOAT:
                mapping.Conversion = AON9_CONVERSION_FLOAT;
                break;
            case D3D_SVT_BOOL:
                mapping.Conversion = AON9_CONVERSION_BOOL;
                break;
            case D3D_SVT_INT:
                mapping.Conversion = AON9_CONVERSION_INT;
                break;
            case D3D_SVT_UINT:
                mapping.Conversion = AON9_CONVERSION_UINT;
                break;
            default:
                throw Direct3DLevel9Exception("Level 9 constant has a type the runtime cannot convert", name, entry.Name, variable.Type);
            }

            tables.Constants.emplace_back(mapping);
        }
        else if (entry.RegisterSet == D3DXRS_SAMPLER) {
            if (entry.RegisterCount != 1) {
                throw Direct3DLevel9Exception("Level 9 sampler is an array", name, entry.Name);
            }

            // vkd3d-shader names a Direct3D 9 sampler after the sampler state and the texture it combines: "sampler+texture"
            vector<string_view> parts = strvex(entry.Name).split('+');

            if (parts.size() != 2) {
                throw Direct3DLevel9Exception("Level 9 sampler does not name a sampler and a texture", name, entry.Name);
            }

            auto sampler = std::ranges::find_if(resources.Bindings, [&parts](const ResourceBinding& b) { return b.Type == D3D_SIT_SAMPLER && b.Name == parts[0]; });
            auto texture = std::ranges::find_if(resources.Bindings, [&parts](const ResourceBinding& b) { return b.Type == D3D_SIT_TEXTURE && b.Name == parts[1]; });

            if (sampler == resources.Bindings.end() || texture == resources.Bindings.end()) {
                throw Direct3DLevel9Exception("Level 9 sampler does not name a bound sampler and texture", name, entry.Name);
            }

            Level9SamplerMapping mapping;
            mapping.Texture = numeric_cast<uint8_t>(texture->BindPoint);
            mapping.Sampler = numeric_cast<uint8_t>(sampler->BindPoint);
            mapping.TargetSampler = numeric_cast<uint8_t>(entry.RegisterIndex);
            tables.Samplers.emplace_back(mapping);
        }
        else {
            throw Direct3DLevel9Exception("Level 9 constant uses an integer or boolean register set", name, entry.Name, entry.RegisterSet);
        }
    }

    return tables;
}

static auto RemapSignatures(const vector<Sm2Instruction>& instructions, bool is_vertex, const vector<SignatureElement>& inputs, const vector<SignatureElement>& outputs, const ScratchRegisters& scratch, string_view name) -> vector<Sm2Instruction>
{
    // vkd3d-shader numbers the Shader Model 2 registers after the semantic index, the level 9 runtime after the
    // Shader Model 4.0 signature register, which may pack two semantics into one register
    unordered_map<uint32_t, SignatureElement> texcoord_inputs;
    unordered_map<uint32_t, uint32_t> texcoord_masks;
    vector<Sm2Instruction> result;
    result.reserve(instructions.size());

    for (const Sm2Instruction& instruction : instructions) {
        if (GetOpcode(instruction[0]) != SM2_OP_DCL) {
            continue;
        }

        Sm2Register type = GetRegisterType(instruction[2]);

        if (!is_vertex && type == Sm2Register::Texture) {
            const SignatureElement& element = FindSignatureElement(inputs, "TEXCOORD", GetRegisterNumber(instruction[2]), name);

            if (element.Register >= LEVEL9_MAX_TEXCOORDS) {
                throw Direct3DLevel9Exception("Level 9 pixel shader input register is beyond 8 texture coordinates", name, element.SemanticIndex, element.Register);
            }

            texcoord_inputs.emplace(GetRegisterNumber(instruction[2]), element);
            uint32_t mask = ((instruction[2] & SM2_WRITE_MASK_MASK) >> SM2_SELECT_SHIFT) << element.FirstComponent;
            texcoord_masks[element.Register] |= mask & SM2_FULL_WRITE_MASK;
        }
        else if (!is_vertex && type == Sm2Register::Input) {
            throw Direct3DLevel9Exception("Level 9 pixel shader reads a color input", name);
        }
    }

    unordered_set<uint32_t> declared_texcoords;

    for (Sm2Instruction instruction : instructions) {
        uint32_t opcode = GetOpcode(instruction[0]);

        if (opcode == SM2_OP_DCL) {
            Sm2Register type = GetRegisterType(instruction[2]);

            if (is_vertex && type == Sm2Register::Input) {
                // The runtime declares signature register r as TEXCOORDr below 8 and as NORMAL(r - 8) above
                uint32_t usage = instruction[1] & SM2_USAGE_MASK;
                uint32_t usage_index = (instruction[1] >> SM2_USAGE_INDEX_SHIFT) & SM2_USAGE_INDEX_MASK;

                if (usage >= std::size(SM2_USAGE_NAMES)) {
                    throw Direct3DLevel9Exception("Level 9 vertex input has an unknown usage", name, usage);
                }

                const SignatureElement& element = FindSignatureElement(inputs, SM2_USAGE_NAMES[usage], usage_index, name);

                if (element.FirstComponent != 0 || element.Register >= LEVEL9_MAX_VERTEX_INPUTS) {
                    throw Direct3DLevel9Exception("Level 9 vertex input is packed or beyond 16 registers", name, element.Name, element.SemanticIndex, element.Register);
                }

                uint32_t new_usage = element.Register < 8 ? SM2_USAGE_TEXCOORD : SM2_USAGE_NORMAL;
                uint32_t new_index = element.Register % 8;
                instruction[1] = (instruction[1] & ~SM2_USAGE_MASK & ~(SM2_USAGE_INDEX_MASK << SM2_USAGE_INDEX_SHIFT)) | new_usage | (new_index << SM2_USAGE_INDEX_SHIFT);
            }
            else if (!is_vertex && type == Sm2Register::Texture) {
                // Semantics packed into one register are declared once, with the joint mask
                uint32_t target = texcoord_inputs.at(GetRegisterNumber(instruction[2])).Register;

                if (!declared_texcoords.emplace(target).second) {
                    continue;
                }

                instruction[2] = SetRegister(instruction[2] & ~SM2_WRITE_MASK_MASK, Sm2Register::Texture, target) | (texcoord_masks.at(target) << SM2_SELECT_SHIFT);
            }

            result.emplace_back(std::move(instruction));
            continue;
        }

        optional<Sm2Instruction> shifted_output;

        for (const Sm2Operand& operand : GetOperands(instruction)) {
            if (operand.Kind != Sm2OperandKind::Destination && operand.Kind != Sm2OperandKind::Source) {
                continue;
            }

            uint32_t& token = instruction[operand.Index];
            Sm2Register type = GetRegisterType(token);

            if (is_vertex && operand.Kind == Sm2OperandKind::Destination && type == Sm2Register::TexCoordOutput) {
                const SignatureElement& element = FindSignatureElement(outputs, "TEXCOORD", GetRegisterNumber(token), name);

                if (element.Register >= LEVEL9_MAX_TEXCOORDS) {
                    throw Direct3DLevel9Exception("Level 9 vertex output register is beyond 8 texture coordinates", name, element.SemanticIndex, element.Register);
                }

                if (element.FirstComponent == 0) {
                    token = SetRegister(token, Sm2Register::TexCoordOutput, element.Register);
                }
                else {
                    // A semantic packed after other components: compute it in a temporary, then move it into place
                    uint32_t mask = (token & SM2_WRITE_MASK_MASK) >> SM2_SELECT_SHIFT;
                    uint32_t swizzle = 0;

                    for (uint32_t component = 0; component < 4; component++) {
                        swizzle |= (component > element.FirstComponent ? component - element.FirstComponent : 0) << (component * 2);
                    }

                    uint32_t destination = SetRegister(token & ~SM2_RESULT_MODIFIERS_MASK & ~SM2_WRITE_MASK_MASK, Sm2Register::TexCoordOutput, element.Register) | (((mask << element.FirstComponent) & SM2_FULL_WRITE_MASK) << SM2_SELECT_SHIFT);
                    uint32_t source = MakeTempSource(scratch.Result, swizzle);
                    shifted_output = MakeMov(destination, const_span<uint32_t> {&source, 1});
                    token = MakeTempDestination(scratch.Result, mask) | (token & SM2_RESULT_MODIFIERS_MASK);
                }
            }
            else if (is_vertex && operand.Kind == Sm2OperandKind::Destination && type == Sm2Register::AttributeOutput) {
                throw Direct3DLevel9Exception("Level 9 vertex shader writes a color output", name);
            }
            else if (!is_vertex && type == Sm2Register::Texture) {
                const SignatureElement& element = texcoord_inputs.at(GetRegisterNumber(token));
                token = SetRegister(token, Sm2Register::Texture, element.Register);

                if (operand.Kind == Sm2OperandKind::Source) {
                    token = SetSwizzle(token, ShiftSwizzle(GetSwizzle(token), element.FirstComponent, element.ComponentCount));
                }
            }
        }

        result.emplace_back(std::move(instruction));

        if (shifted_output.has_value()) {
            result.emplace_back(std::move(shifted_output.value()));
        }
    }

    return result;
}

static void LegalizeInstruction(Sm2Instruction instruction, const ScratchRegisters& scratch, const unordered_map<uint32_t, uint32_t>& sampler_types, vector<Sm2Instruction>& output, string_view name)
{
    // vkd3d-shader writes code the Direct3D 9 validator rejects, which a level 9 driver runs: each rule below is one
    // the validator states and the fix a compiler for Direct3D 9 applies
    uint32_t opcode = GetOpcode(instruction[0]);

    if (opcode == SM2_OP_DCL || opcode == SM2_OP_DEF || opcode == SM2_OP_DEFI || opcode == SM2_OP_DEFB) {
        output.emplace_back(std::move(instruction));
        return;
    }

    small_vector<Sm2Operand, 8> operands = GetOperands(instruction);

    // texkill tests every component: replicate the written ones over the whole register
    if (opcode == SM2_OP_TEXKILL) {
        uint32_t token = instruction[operands[0].Index];
        uint32_t mask = (token & SM2_WRITE_MASK_MASK) >> SM2_SELECT_SHIFT;
        uint32_t swizzle = 0;
        uint32_t last_component = 0;

        for (uint32_t component = 0, written = 0; component < 4; component++) {
            if ((mask & (1u << component)) != 0) {
                swizzle |= component << (written * 2);
                last_component = component;
                written++;
            }
        }

        for (uint32_t written = numeric_cast<uint32_t>(std::popcount(mask)); written < 4; written++) {
            swizzle |= last_component << (written * 2);
        }

        uint32_t source = (token & ~SM2_SWIZZLE_MASK & ~SM2_SOURCE_MODIFIER_MASK) | (swizzle << SM2_SELECT_SHIFT);
        output.emplace_back(MakeMov(MakeTempDestination(scratch.Source0, SM2_FULL_WRITE_MASK), const_span<uint32_t> {&source, 1}));
        output.emplace_back(Sm2Instruction {instruction[0], MakeTempDestination(scratch.Source0, SM2_FULL_WRITE_MASK)});
        return;
    }

    small_vector<Sm2Instruction, 4> before;
    optional<Sm2Instruction> after;

    // A texture coordinate is read without a swizzle: drop one that changes no component the lookup reads, or move it
    if (opcode == SM2_OP_TEX || opcode == SM2_OP_TEXLDD || opcode == SM2_OP_TEXLDL) {
        uint32_t& coordinate = instruction[operands[1].Index];
        uint32_t sampler = GetRegisterNumber(instruction[operands[2].Index]);
        auto sampler_type = sampler_types.find(sampler);
        bool projected = opcode == SM2_OP_TEX && ((instruction[0] >> SM2_TEX_CONTROL_SHIFT) & SM2_TEX_CONTROL_MASK) != 0;
        uint32_t used_components = 4;

        if (!projected && opcode != SM2_OP_TEXLDL && sampler_type != sampler_types.end()) {
            used_components = sampler_type->second == SM2_SAMPLER_2D ? 2 : (sampler_type->second == SM2_SAMPLER_CUBE || sampler_type->second == SM2_SAMPLER_VOLUME ? 3 : 4);
        }

        uint32_t swizzle = GetSwizzle(coordinate);
        bool identity = (coordinate & SM2_SOURCE_MODIFIER_MASK) == 0;

        for (uint32_t component = 0; component < used_components; component++) {
            identity = identity && ((swizzle >> (component * 2)) & 0x3) == component;
        }

        if (identity) {
            coordinate = SetSwizzle(coordinate, SM2_IDENTITY_SWIZZLE);
        }
        else {
            before.emplace_back(MakeMov(MakeTempDestination(scratch.Source0, SM2_FULL_WRITE_MASK), const_span<uint32_t> {&coordinate, 1}));
            coordinate = MakeTempSource(scratch.Source0, SM2_IDENTITY_SWIZZLE);
        }
    }

    // Read ports: one constant register (read at most twice) and one input register per instruction
    if (opcode != SM2_OP_SINCOS) {
        optional<uint32_t> first_constant;
        optional<uint32_t> first_input;
        int32_t constant_reads = 0;
        small_vector<uint32_t, 2> free_temps = {scratch.Source0, scratch.Source1};
        small_vector<size_t, 2> dropped_address_tokens;

        for (const Sm2Operand& operand : operands) {
            if (operand.Kind != Sm2OperandKind::Source) {
                continue;
            }

            uint32_t& token = instruction[operand.Index];
            Sm2Register type = GetRegisterType(token);
            bool relative = (token & SM2_RELATIVE_ADDRESSING) != 0;
            uint32_t key = (token & (SM2_REGISTER_NUMBER_MASK | SM2_RELATIVE_ADDRESSING));
            bool move = false;

            if (type == Sm2Register::Const) {
                if (!first_constant.has_value()) {
                    first_constant = key;
                    constant_reads = 1;
                }
                else if (first_constant.value() == key && constant_reads < 2 && !relative) {
                    constant_reads++;
                }
                else {
                    move = true;
                }
            }
            else if (type == Sm2Register::Input) {
                if (!first_input.has_value()) {
                    first_input = key;
                }
                else if (first_input.value() != key) {
                    move = true;
                }
            }

            if (move) {
                if (free_temps.empty()) {
                    throw Direct3DLevel9Exception("Level 9 instruction reads too many constant or input registers", name, opcode);
                }

                uint32_t temp = free_temps.front();
                free_temps.erase(free_temps.begin());
                small_vector<uint32_t, 2> source = {SetSwizzle(token, SM2_IDENTITY_SWIZZLE) & ~SM2_SOURCE_MODIFIER_MASK};

                if (relative) {
                    source.emplace_back(instruction[operand.Index + 1]);
                    dropped_address_tokens.emplace_back(operand.Index + 1);
                }

                before.emplace_back(MakeMov(MakeTempDestination(temp, SM2_FULL_WRITE_MASK), const_span<uint32_t> {source.data(), source.size()}));
                token = SetRegister(token & ~SM2_RELATIVE_ADDRESSING, Sm2Register::Temp, temp);
            }
        }

        for (auto it = dropped_address_tokens.rbegin(); it != dropped_address_tokens.rend(); ++it) {
            instruction.erase(instruction.begin() + numeric_cast<ptrdiff_t>(*it));
        }

        if (!dropped_address_tokens.empty()) {
            instruction[0] = (instruction[0] & ~(SM2_LENGTH_MASK << SM2_LENGTH_SHIFT)) | (numeric_cast<uint32_t>(instruction.size() - 1) << SM2_LENGTH_SHIFT);
            operands = GetOperands(instruction);
        }
    }

    // A color output is written only by mov, and sincos never writes the register it reads
    if (!operands.empty() && operands[0].Kind == Sm2OperandKind::Destination) {
        uint32_t& destination = instruction[operands[0].Index];
        bool redirect = GetRegisterType(destination) == Sm2Register::ColorOutput && opcode != SM2_OP_MOV;

        if (opcode == SM2_OP_SINCOS) {
            uint32_t source = instruction[operands[1].Index];
            redirect = redirect || (GetRegisterType(destination) == GetRegisterType(source) && GetRegisterNumber(destination) == GetRegisterNumber(source));
        }

        if (redirect) {
            uint32_t mask = (destination & SM2_WRITE_MASK_MASK) >> SM2_SELECT_SHIFT;
            uint32_t source = MakeTempSource(scratch.Result, SM2_IDENTITY_SWIZZLE);
            after = MakeMov(destination & ~SM2_RESULT_MODIFIERS_MASK, const_span<uint32_t> {&source, 1});
            destination = MakeTempDestination(scratch.Result, mask) | (destination & SM2_RESULT_MODIFIERS_MASK);
        }
    }

    for (Sm2Instruction& moved : before) {
        output.emplace_back(std::move(moved));
    }

    output.emplace_back(std::move(instruction));

    if (after.has_value()) {
        output.emplace_back(std::move(after.value()));
    }
}

static auto BuildAon9Chunk(uint32_t version, const vector<Sm2Instruction>& instructions, const Level9Tables& tables) -> vector<uint8_t>
{
    vector<uint8_t> bytecode;

    {
        data_writer writer {bytecode};
        writer.write<uint32_t>(version);

        for (const Sm2Instruction& instruction : instructions) {
            writer.write_object_vector(instruction);
        }

        writer.write<uint32_t>(SM2_END_TOKEN);
    }

    // Tables after the header: samplers, constant buffers, loop counters (none), runtime constants, then the code
    size_t sampler_offset = AON9_HEADER_SIZE;
    size_t constant_offset = sampler_offset + tables.Samplers.size() * 4;
    size_t loop_offset = constant_offset + tables.Constants.size() * 12;
    size_t runtime_count = tables.HasPositionOffset ? 1 : 0;
    size_t bytecode_offset = loop_offset + runtime_count * 4;

    vector<uint8_t> chunk;
    data_writer writer {chunk};
    writer.write<uint32_t>(numeric_cast<uint32_t>(bytecode_offset + bytecode.size()));
    writer.write<uint32_t>((version & 0xFFFF0000u) | 0x0200u);
    writer.write<uint32_t>(numeric_cast<uint32_t>(bytecode.size()));
    writer.write<uint32_t>(numeric_cast<uint32_t>(bytecode_offset));

    // Each table as a count and an offset: constant buffers, loop counters, an unused one, samplers, runtime constants
    writer.write<uint16_t>(numeric_cast<uint16_t>(tables.Constants.size()));
    writer.write<uint16_t>(numeric_cast<uint16_t>(constant_offset));
    writer.write<uint16_t>(uint16_t {0});
    writer.write<uint16_t>(numeric_cast<uint16_t>(loop_offset));
    writer.write<uint16_t>(uint16_t {0});
    writer.write<uint16_t>(numeric_cast<uint16_t>(loop_offset));
    writer.write<uint16_t>(numeric_cast<uint16_t>(tables.Samplers.size()));
    writer.write<uint16_t>(numeric_cast<uint16_t>(sampler_offset));
    writer.write<uint16_t>(numeric_cast<uint16_t>(runtime_count));
    writer.write<uint16_t>(numeric_cast<uint16_t>(loop_offset));

    for (const Level9SamplerMapping& mapping : tables.Samplers) {
        writer.write<uint8_t>(mapping.Texture);
        writer.write<uint8_t>(mapping.Sampler);
        writer.write<uint8_t>(mapping.TargetSampler);
        writer.write<uint8_t>(uint8_t {0});
    }

    for (const Level9ConstantMapping& mapping : tables.Constants) {
        writer.write<uint16_t>(mapping.Buffer);
        writer.write<uint16_t>(mapping.StartRegister);
        writer.write<uint16_t>(mapping.RegisterCount);
        writer.write<uint16_t>(mapping.TargetRegister);

        for (int32_t component = 0; component < 4; component++) {
            writer.write<uint8_t>(mapping.Conversion);
        }
    }

    if (tables.HasPositionOffset) {
        writer.write<uint16_t>(AON9_RUNTIME_POSITION_OFFSET);
        writer.write<uint16_t>(numeric_cast<uint16_t>(tables.PositionOffsetRegister));
    }

    writer.write_byte_vector(bytecode);
    FO_STRONG_ASSERT(chunk.size() == bytecode_offset + bytecode.size(), "Level 9 chunk size disagrees with its layout", chunk.size());
    return chunk;
}

static auto SerializeContainer(const vkd3d_shader_dxbc_desc& container, const_span<uint8_t> aon9, string_view name) -> vector<uint8_t>
{
    // The runtime looks for the level 9 chunk first, as the Microsoft compiler writes it
    vector<vkd3d_shader_dxbc_section_desc> sections;
    sections.reserve(container.section_count + 1);

    vkd3d_shader_dxbc_section_desc& aon9_section = sections.emplace_back();
    aon9_section.tag = TAG_AON9;
    aon9_section.data.code = aon9.data();
    aon9_section.data.size = aon9.size();

    for (size_t i = 0; i < container.section_count; i++) {
        sections.emplace_back(container.sections[i]);
    }

    vkd3d_shader_code dxbc {};
    nptr<char> messages {};
    int32_t result = vkd3d_shader_serialize_dxbc(sections.size(), sections.data(), &dxbc, messages.get_pp());
    auto messages_holder = make_unique_del_ptr(messages, vkd3d_shader_free_messages);
    auto dxbc_holder = scope_exit([&dxbc]() noexcept { vkd3d_shader_free_shader_code(&dxbc); });

    if (result < 0) {
        throw Direct3DLevel9Exception("Level 9 container does not serialize", name, result, messages ? string(messages.get()) : string());
    }

    nptr<const void> code = dxbc.code;
    FO_VERIFY_AND_THROW(code && dxbc.size != 0, "Level 9 container serialized without bytes", name);
    vector<uint8_t> bytes(dxbc.size);
    memory::copy(bytes.data(), code, dxbc.size);
    return bytes;
}

static auto GetOperands(const Sm2Instruction& instruction) -> small_vector<Sm2Operand, 8>
{
    small_vector<Sm2Operand, 8> operands;
    uint32_t opcode = GetOpcode(instruction[0]);

    if (opcode == SM2_OP_DCL) {
        operands.emplace_back(Sm2Operand {1, Sm2OperandKind::Data});
        operands.emplace_back(Sm2Operand {2, Sm2OperandKind::Destination});
        return operands;
    }

    if (opcode == SM2_OP_DEF || opcode == SM2_OP_DEFI || opcode == SM2_OP_DEFB) {
        operands.emplace_back(Sm2Operand {1, Sm2OperandKind::Destination});

        for (size_t i = 2; i < instruction.size(); i++) {
            operands.emplace_back(Sm2Operand {i, Sm2OperandKind::Data});
        }

        return operands;
    }

    bool has_destination = opcode != SM2_OP_NOP && opcode != SM2_OP_LABEL && opcode != SM2_OP_BREAKP && !IsFlowControl(opcode);
    size_t index = 1;

    if (has_destination && index < instruction.size()) {
        operands.emplace_back(Sm2Operand {index, Sm2OperandKind::Destination});
        index++;
    }

    while (index < instruction.size()) {
        operands.emplace_back(Sm2Operand {index, Sm2OperandKind::Source});

        if ((instruction[index] & SM2_RELATIVE_ADDRESSING) != 0 && index + 1 < instruction.size()) {
            operands.emplace_back(Sm2Operand {index + 1, Sm2OperandKind::Address});
            index++;
        }

        index++;
    }

    return operands;
}

static auto GetInstructionSlots(uint32_t opcode) -> int32_t
{
    // Instruction slot costs of vs_2_x and ps_2_x, as the Direct3D 9 validator counts them
    switch (opcode) {
    case SM2_OP_NOP:
    case SM2_OP_DCL:
    case SM2_OP_DEF:
    case SM2_OP_DEFI:
    case SM2_OP_DEFB:
    case SM2_OP_LABEL:
        return 0;
    case SM2_OP_LRP:
    case SM2_OP_M3X2:
    case SM2_OP_CRS:
    case SM2_OP_CALL:
    case SM2_OP_ENDLOOP:
    case SM2_OP_ENDREP:
    case SM2_OP_DP2ADD:
        return 2;
    case SM2_OP_M4X3:
    case SM2_OP_M3X3:
    case SM2_OP_POW:
    case SM2_OP_SGN:
    case SM2_OP_NRM:
    case SM2_OP_CALLNZ:
    case SM2_OP_LOOP:
    case SM2_OP_REP:
    case SM2_OP_IF:
    case SM2_OP_IFC:
    case SM2_OP_BREAKC:
        return 3;
    case SM2_OP_M4X4:
    case SM2_OP_M3X4:
        return 4;
    case SM2_OP_SINCOS:
        return 8;
    default:
        return 1;
    }
}

static auto IsFlowControl(uint32_t opcode) -> bool
{
    switch (opcode) {
    case SM2_OP_CALL:
    case SM2_OP_CALLNZ:
    case SM2_OP_LOOP:
    case SM2_OP_RET:
    case SM2_OP_ENDLOOP:
    case SM2_OP_REP:
    case SM2_OP_ENDREP:
    case SM2_OP_IF:
    case SM2_OP_IFC:
    case SM2_OP_ELSE:
    case SM2_OP_ENDIF:
    case SM2_OP_BREAK:
    case SM2_OP_BREAKC:
        return true;
    default:
        return false;
    }
}

static auto FindSignatureElement(const vector<SignatureElement>& signature, string_view semantic, uint32_t index, string_view name) -> const SignatureElement&
{
    for (const SignatureElement& element : signature) {
        if (element.Name == semantic && element.SemanticIndex == index) {
            return element;
        }
    }

    throw Direct3DLevel9Exception("Shader Model 4.0 signature has no semantic the level 9 code uses", name, semantic, index);
}

static auto GetOpcode(uint32_t token) -> uint32_t
{
    return token & SM2_OPCODE_MASK;
}

static auto GetRegisterType(uint32_t token) -> Sm2Register
{
    return static_cast<Sm2Register>(((token >> 28) & 0x7) | ((token >> 8) & 0x18));
}

static auto GetRegisterNumber(uint32_t token) -> uint32_t
{
    return token & SM2_REGISTER_NUMBER_MASK;
}

static auto SetRegister(uint32_t token, Sm2Register type, uint32_t number) -> uint32_t
{
    uint32_t type_value = static_cast<uint32_t>(type);
    return (token & ~(SM2_REGISTER_NUMBER_MASK | SM2_REGISTER_TYPE_MASK)) | (number & SM2_REGISTER_NUMBER_MASK) | ((type_value & 0x7) << 28) | ((type_value & 0x18) << 8);
}

static auto SetSwizzle(uint32_t token, uint32_t swizzle) -> uint32_t
{
    return (token & ~SM2_SWIZZLE_MASK) | (swizzle << SM2_SELECT_SHIFT);
}

static auto GetSwizzle(uint32_t token) -> uint32_t
{
    return (token & SM2_SWIZZLE_MASK) >> SM2_SELECT_SHIFT;
}

static auto ShiftSwizzle(uint32_t swizzle, uint32_t first_component, uint32_t component_count) -> uint32_t
{
    // A component beyond the semantic reads its last one: its value was never meaningful
    uint32_t shifted = 0;

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t component = std::min((swizzle >> (i * 2)) & 0x3, component_count - 1);
        shifted |= (first_component + component) << (i * 2);
    }

    return shifted;
}

static auto MakeTempDestination(uint32_t number, uint32_t write_mask) -> uint32_t
{
    return SM2_PARAMETER_TOKEN | (write_mask << SM2_SELECT_SHIFT) | number;
}

static auto MakeTempSource(uint32_t number, uint32_t swizzle) -> uint32_t
{
    return SM2_PARAMETER_TOKEN | (swizzle << SM2_SELECT_SHIFT) | number;
}

static auto MakeMov(uint32_t destination, const_span<uint32_t> source) -> Sm2Instruction
{
    Sm2Instruction instruction;
    instruction.reserve(2 + source.size());
    instruction.emplace_back(SM2_OP_MOV | (numeric_cast<uint32_t>(1 + source.size()) << SM2_LENGTH_SHIFT));
    instruction.emplace_back(destination);
    instruction.insert(instruction.end(), source.begin(), source.end());
    return instruction;
}

static auto ReadStringAt(const_span<uint8_t> data, size_t pos) -> string
{
    if (pos >= data.size()) {
        throw DataReadingException("String offset is past the data");
    }

    auto end = std::find(data.begin() + numeric_cast<ptrdiff_t>(pos), data.end(), uint8_t {0});

    if (end == data.end()) {
        throw DataReadingException("String is not terminated");
    }

    return span_read_string(data, pos, numeric_cast<size_t>(end - data.begin()) - pos);
}

FO_END_NAMESPACE
