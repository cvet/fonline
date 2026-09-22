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

#include "ManagedAssemblyReferences.h"

#if FO_MANAGED_SCRIPTING

FO_BEGIN_NAMESPACE

// Table numbers and column layouts follow ECMA-335 partition II, chapter 22
constexpr size_t METADATA_TABLE_COUNT = 64;
constexpr size_t TABLE_MODULE = 0x00;
constexpr size_t TABLE_TYPE_REF = 0x01;
constexpr size_t TABLE_TYPE_DEF = 0x02;
constexpr size_t TABLE_FIELD_PTR = 0x03;
constexpr size_t TABLE_FIELD = 0x04;
constexpr size_t TABLE_METHOD_PTR = 0x05;
constexpr size_t TABLE_METHOD_DEF = 0x06;
constexpr size_t TABLE_PARAM_PTR = 0x07;
constexpr size_t TABLE_PARAM = 0x08;
constexpr size_t TABLE_INTERFACE_IMPL = 0x09;
constexpr size_t TABLE_MEMBER_REF = 0x0A;
constexpr size_t TABLE_CONSTANT = 0x0B;
constexpr size_t TABLE_CUSTOM_ATTRIBUTE = 0x0C;
constexpr size_t TABLE_FIELD_MARSHAL = 0x0D;
constexpr size_t TABLE_DECL_SECURITY = 0x0E;
constexpr size_t TABLE_CLASS_LAYOUT = 0x0F;
constexpr size_t TABLE_FIELD_LAYOUT = 0x10;
constexpr size_t TABLE_STAND_ALONE_SIG = 0x11;
constexpr size_t TABLE_EVENT_MAP = 0x12;
constexpr size_t TABLE_EVENT_PTR = 0x13;
constexpr size_t TABLE_EVENT = 0x14;
constexpr size_t TABLE_PROPERTY_MAP = 0x15;
constexpr size_t TABLE_PROPERTY_PTR = 0x16;
constexpr size_t TABLE_PROPERTY = 0x17;
constexpr size_t TABLE_METHOD_SEMANTICS = 0x18;
constexpr size_t TABLE_METHOD_IMPL = 0x19;
constexpr size_t TABLE_MODULE_REF = 0x1A;
constexpr size_t TABLE_TYPE_SPEC = 0x1B;
constexpr size_t TABLE_IMPL_MAP = 0x1C;
constexpr size_t TABLE_FIELD_RVA = 0x1D;
constexpr size_t TABLE_ENC_LOG = 0x1E;
constexpr size_t TABLE_ENC_MAP = 0x1F;
constexpr size_t TABLE_ASSEMBLY = 0x20;
constexpr size_t TABLE_ASSEMBLY_PROCESSOR = 0x21;
constexpr size_t TABLE_ASSEMBLY_OS = 0x22;
constexpr size_t TABLE_ASSEMBLY_REF = 0x23;
constexpr size_t TABLE_FILE = 0x26;
constexpr size_t TABLE_EXPORTED_TYPE = 0x27;
constexpr size_t TABLE_MANIFEST_RESOURCE = 0x28;
constexpr size_t TABLE_GENERIC_PARAM = 0x2A;
constexpr size_t TABLE_METHOD_SPEC = 0x2B;
constexpr size_t TABLE_GENERIC_PARAM_CONSTRAINT = 0x2C;

constexpr uint16_t DOS_SIGNATURE = 0x5A4D;
constexpr uint32_t PE_SIGNATURE = 0x00004550;
constexpr uint16_t PE32_MAGIC = 0x10B;
constexpr uint16_t PE32_PLUS_MAGIC = 0x20B;
constexpr size_t CLR_DIRECTORY_INDEX = 14;
constexpr size_t SECTION_HEADER_SIZE = 40;
constexpr uint32_t METADATA_SIGNATURE = 0x424A5342;
constexpr uint8_t HEAP_SIZE_LARGE_STRINGS = 0x01;
constexpr uint8_t HEAP_SIZE_LARGE_GUIDS = 0x02;
constexpr uint8_t HEAP_SIZE_LARGE_BLOBS = 0x04;
constexpr uint8_t HEAP_SIZE_EXTRA_DATA = 0x40;

struct ManagedMetadataStream
{
    size_t Offset {};
    size_t Size {};
};

struct ManagedMetadataStreams
{
    optional<ManagedMetadataStream> Tables {};
    optional<ManagedMetadataStream> Strings {};
};

struct ManagedMetadataTables
{
    array<uint32_t, METADATA_TABLE_COUNT> Rows {};
    size_t RowsOffset {};
    size_t StringIndexSize {};
    size_t GuidIndexSize {};
    size_t BlobIndexSize {};
};

static auto FindMetadataRoot(const_span<uint8_t> image) -> size_t;
static auto ReadMetadataStreams(const_span<uint8_t> image, size_t metadata_offset) -> ManagedMetadataStreams;
static auto ReadMetadataTables(const_span<uint8_t> image, const ManagedMetadataStream& tables_stream) -> ManagedMetadataTables;
static auto MakeTableRowSizes(const ManagedMetadataTables& tables) -> array<size_t, TABLE_ASSEMBLY_REF + 1>;
static auto GetTableOffset(const ManagedMetadataTables& tables, const array<size_t, TABLE_ASSEMBLY_REF + 1>& row_sizes, size_t table) -> size_t;
static auto RvaToOffset(const_span<uint8_t> image, size_t sections_offset, size_t section_count, uint32_t rva) -> size_t;
static auto ReadHeapString(const_span<uint8_t> image, const ManagedMetadataStream& strings_stream, size_t index) -> string;
static auto ReadIndex(const_span<uint8_t> image, size_t offset, size_t index_size) -> size_t;
static auto ReadU8(const_span<uint8_t> image, size_t offset) -> uint8_t;
static auto ReadU16(const_span<uint8_t> image, size_t offset) -> uint16_t;
static auto ReadU32(const_span<uint8_t> image, size_t offset) -> uint32_t;
static auto ReadU64(const_span<uint8_t> image, size_t offset) -> uint64_t;
static void RequireImageBytes(const_span<uint8_t> image, size_t offset, size_t size);

auto ReadManagedAssemblyIdentity(const_span<uint8_t> image) -> ManagedAssemblyIdentity
{
    FO_STACK_TRACE_ENTRY();

    size_t metadata_offset = FindMetadataRoot(image);
    ManagedMetadataStreams streams = ReadMetadataStreams(image, metadata_offset);

    if (!streams.Tables.has_value() || !streams.Strings.has_value()) {
        throw ManagedAssemblyReferencesException("Managed assembly metadata has no compressed tables or strings stream");
    }

    ManagedMetadataTables tables = ReadMetadataTables(image, *streams.Tables);
    array<size_t, TABLE_ASSEMBLY_REF + 1> row_sizes = MakeTableRowSizes(tables);
    size_t tables_end = GetTableOffset(tables, row_sizes, TABLE_ASSEMBLY_REF) + tables.Rows[TABLE_ASSEMBLY_REF] * row_sizes[TABLE_ASSEMBLY_REF];

    if (tables_end > streams.Tables->Offset + streams.Tables->Size) {
        throw ManagedAssemblyReferencesException("Managed assembly metadata tables overrun their stream", tables_end, streams.Tables->Offset + streams.Tables->Size);
    }
    if (tables.Rows[TABLE_ASSEMBLY] != 1) {
        throw ManagedAssemblyReferencesException("Managed assembly image must define exactly one assembly", tables.Rows[TABLE_ASSEMBLY]);
    }

    ManagedAssemblyIdentity identity;

    // Assembly row: HashAlgId, four version parts, Flags, PublicKey blob, then Name
    size_t assembly_name_offset = GetTableOffset(tables, row_sizes, TABLE_ASSEMBLY) + 16 + tables.BlobIndexSize;
    identity.Name = ReadHeapString(image, *streams.Strings, ReadIndex(image, assembly_name_offset, tables.StringIndexSize));

    // AssemblyRef row: four version parts, Flags, PublicKeyOrToken blob, then Name
    size_t reference_offset = GetTableOffset(tables, row_sizes, TABLE_ASSEMBLY_REF);

    for (uint32_t row = 0; row < tables.Rows[TABLE_ASSEMBLY_REF]; row++) {
        size_t reference_name_offset = reference_offset + 12 + tables.BlobIndexSize;
        identity.References.emplace_back(ReadHeapString(image, *streams.Strings, ReadIndex(image, reference_name_offset, tables.StringIndexSize)));
        reference_offset += row_sizes[TABLE_ASSEMBLY_REF];
    }

    return identity;
}

auto CollectReferencedRuntimeAssemblies(const vector<ManagedAssemblyIdentity>& pack_assemblies, const FindManagedRuntimeAssemblyCallback& find_runtime_assembly) -> set<string>
{
    FO_STACK_TRACE_ENTRY();

    set<string> pack_names;
    vector<pair<string, string>> pending_references;

    for (const ManagedAssemblyIdentity& assembly : pack_assemblies) {
        pack_names.emplace(assembly.Name);

        for (const string& reference : assembly.References) {
            pending_references.emplace_back(reference, assembly.Name);
        }
    }

    optional<ManagedAssemblyIdentity> corelib = find_runtime_assembly(MANAGED_CORELIB_ASSEMBLY_NAME);

    if (!corelib.has_value()) {
        throw ManagedAssemblyReferencesException("Managed runtime does not publish CoreLib", MANAGED_CORELIB_ASSEMBLY_NAME);
    }
    if (corelib->Name != MANAGED_CORELIB_ASSEMBLY_NAME) {
        throw ManagedAssemblyReferencesException("Managed runtime CoreLib file defines another assembly", MANAGED_CORELIB_ASSEMBLY_NAME, corelib->Name);
    }

    set<string> runtime_names {string(MANAGED_CORELIB_ASSEMBLY_NAME)};

    for (string& reference : corelib->References) {
        pending_references.emplace_back(std::move(reference), string(MANAGED_CORELIB_ASSEMBLY_NAME));
    }

    while (!pending_references.empty()) {
        pair<string, string> pending = std::move(pending_references.back());
        pending_references.pop_back();
        const string& reference = pending.first;

        if (pack_names.contains(reference) || runtime_names.contains(reference)) {
            continue;
        }

        optional<ManagedAssemblyIdentity> runtime_assembly = find_runtime_assembly(reference);

        // Compatibility facades such as mscorlib forward into out-of-band packages the runtime never publishes, so
        // only a reference the pack itself makes has to resolve
        if (!runtime_assembly.has_value() && pack_names.contains(pending.second)) {
            throw ManagedAssemblyReferencesException("Managed assembly reference is satisfied neither by the pack nor by the runtime", reference, pending.second);
        }
        if (!runtime_assembly.has_value()) {
            continue;
        }
        if (runtime_assembly->Name != reference) {
            throw ManagedAssemblyReferencesException("Managed runtime assembly file defines another assembly than its name", reference, runtime_assembly->Name);
        }

        runtime_names.emplace(reference);

        for (string& nested_reference : runtime_assembly->References) {
            pending_references.emplace_back(std::move(nested_reference), reference);
        }
    }

    return runtime_names;
}

static auto FindMetadataRoot(const_span<uint8_t> image) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    if (ReadU16(image, 0) != DOS_SIGNATURE) {
        throw ManagedAssemblyReferencesException("Managed assembly image has no DOS header");
    }

    size_t pe_offset = ReadU32(image, 0x3C);

    if (ReadU32(image, pe_offset) != PE_SIGNATURE) {
        throw ManagedAssemblyReferencesException("Managed assembly image has no PE signature", pe_offset);
    }

    size_t section_count = ReadU16(image, pe_offset + 6);
    size_t optional_header_size = ReadU16(image, pe_offset + 20);
    size_t optional_header_offset = pe_offset + 24;
    uint16_t magic = ReadU16(image, optional_header_offset);
    size_t directory_count_offset = 0;

    if (magic == PE32_MAGIC) {
        directory_count_offset = 92;
    }
    else if (magic == PE32_PLUS_MAGIC) {
        directory_count_offset = 108;
    }
    else {
        throw ManagedAssemblyReferencesException("Managed assembly image has an unknown optional header", magic);
    }

    // The directory count and the directories are optional-header fields, so they must fit its declared size
    size_t clr_directory_offset = directory_count_offset + 4 + CLR_DIRECTORY_INDEX * 8;

    if (ReadU32(image, optional_header_offset + directory_count_offset) <= CLR_DIRECTORY_INDEX || clr_directory_offset + 8 > optional_header_size) {
        throw ManagedAssemblyReferencesException("Managed assembly image has no CLR runtime header directory");
    }

    uint32_t clr_rva = ReadU32(image, optional_header_offset + clr_directory_offset);

    if (clr_rva == 0) {
        throw ManagedAssemblyReferencesException("Image is a native PE file, not a managed assembly");
    }

    size_t sections_offset = optional_header_offset + optional_header_size;
    size_t clr_offset = RvaToOffset(image, sections_offset, section_count, clr_rva);
    size_t metadata_offset = RvaToOffset(image, sections_offset, section_count, ReadU32(image, clr_offset + 8));

    if (ReadU32(image, metadata_offset) != METADATA_SIGNATURE) {
        throw ManagedAssemblyReferencesException("Managed assembly metadata root has no signature", metadata_offset);
    }

    return metadata_offset;
}

static auto ReadMetadataStreams(const_span<uint8_t> image, size_t metadata_offset) -> ManagedMetadataStreams
{
    FO_STACK_TRACE_ENTRY();

    size_t version_length = ReadU32(image, metadata_offset + 12);
    RequireImageBytes(image, metadata_offset + 16, version_length);
    size_t stream_count = ReadU16(image, metadata_offset + 18 + version_length);
    size_t header_offset = metadata_offset + 20 + version_length;
    ManagedMetadataStreams streams;

    for (size_t stream_index = 0; stream_index < stream_count; stream_index++) {
        size_t stream_offset = metadata_offset + ReadU32(image, header_offset);
        size_t stream_size = ReadU32(image, header_offset + 4);
        RequireImageBytes(image, stream_offset, stream_size);

        size_t name_offset = header_offset + 8;
        size_t name_end = name_offset;

        while (ReadU8(image, name_end) != 0) {
            name_end++;
        }

        const_span<uint8_t> name_bytes = image.subspan(name_offset, name_end - name_offset);
        string name {name_bytes.begin(), name_bytes.end()};

        // The name field holds the terminator and is padded to a four-byte boundary
        header_offset += 8 + (name.size() + 4) / 4 * 4;

        if (name == "#~") {
            streams.Tables = ManagedMetadataStream {.Offset = stream_offset, .Size = stream_size};
        }
        else if (name == "#Strings") {
            streams.Strings = ManagedMetadataStream {.Offset = stream_offset, .Size = stream_size};
        }
    }

    return streams;
}

static auto ReadMetadataTables(const_span<uint8_t> image, const ManagedMetadataStream& tables_stream) -> ManagedMetadataTables
{
    FO_STACK_TRACE_ENTRY();

    uint8_t heap_sizes = ReadU8(image, tables_stream.Offset + 6);
    uint64_t valid_tables = ReadU64(image, tables_stream.Offset + 8);
    size_t rows_offset = tables_stream.Offset + 24;
    ManagedMetadataTables tables;

    for (size_t table = 0; table < METADATA_TABLE_COUNT; table++) {
        if ((valid_tables & (uint64_t {1} << table)) != 0) {
            tables.Rows[table] = ReadU32(image, rows_offset);
            rows_offset += 4;
        }
    }

    if ((heap_sizes & HEAP_SIZE_EXTRA_DATA) != 0) {
        rows_offset += 4;
    }

    tables.RowsOffset = rows_offset;
    tables.StringIndexSize = (heap_sizes & HEAP_SIZE_LARGE_STRINGS) != 0 ? 4 : 2;
    tables.GuidIndexSize = (heap_sizes & HEAP_SIZE_LARGE_GUIDS) != 0 ? 4 : 2;
    tables.BlobIndexSize = (heap_sizes & HEAP_SIZE_LARGE_BLOBS) != 0 ? 4 : 2;
    return tables;
}

static auto MakeTableRowSizes(const ManagedMetadataTables& tables) -> array<size_t, TABLE_ASSEMBLY_REF + 1>
{
    FO_STACK_TRACE_ENTRY();

    const array<uint32_t, METADATA_TABLE_COUNT>& rows = tables.Rows;
    size_t str = tables.StringIndexSize;
    size_t guid = tables.GuidIndexSize;
    size_t blob = tables.BlobIndexSize;

    auto simple_index = [&rows](size_t table) -> size_t { return rows[table] < 0x10000 ? 2 : 4; };

    // A coded index widens once its largest table no longer fits beside the tag bits
    auto coded_index = [&rows](std::initializer_list<size_t> coded_tables, uint32_t tag_bits) -> size_t {
        uint32_t max_rows = 0;

        for (size_t table : coded_tables) {
            max_rows = std::max(max_rows, rows[table]);
        }

        return max_rows < (uint32_t {1} << (16 - tag_bits)) ? 2 : 4;
    };

    size_t type_def_or_ref = coded_index({TABLE_TYPE_DEF, TABLE_TYPE_REF, TABLE_TYPE_SPEC}, 2);
    size_t has_constant = coded_index({TABLE_FIELD, TABLE_PARAM, TABLE_PROPERTY}, 2);
    size_t has_custom_attribute = coded_index({TABLE_METHOD_DEF, TABLE_FIELD, TABLE_TYPE_REF, TABLE_TYPE_DEF, TABLE_PARAM, TABLE_INTERFACE_IMPL, TABLE_MEMBER_REF, TABLE_MODULE, TABLE_DECL_SECURITY, TABLE_PROPERTY, TABLE_EVENT, TABLE_STAND_ALONE_SIG, TABLE_MODULE_REF, TABLE_TYPE_SPEC, TABLE_ASSEMBLY, TABLE_ASSEMBLY_REF, TABLE_FILE, TABLE_EXPORTED_TYPE, TABLE_MANIFEST_RESOURCE, TABLE_GENERIC_PARAM, TABLE_GENERIC_PARAM_CONSTRAINT, TABLE_METHOD_SPEC}, 5);
    size_t has_field_marshal = coded_index({TABLE_FIELD, TABLE_PARAM}, 1);
    size_t has_decl_security = coded_index({TABLE_TYPE_DEF, TABLE_METHOD_DEF, TABLE_ASSEMBLY}, 2);
    size_t member_ref_parent = coded_index({TABLE_TYPE_DEF, TABLE_TYPE_REF, TABLE_MODULE_REF, TABLE_METHOD_DEF, TABLE_TYPE_SPEC}, 3);
    size_t has_semantics = coded_index({TABLE_EVENT, TABLE_PROPERTY}, 1);
    size_t method_def_or_ref = coded_index({TABLE_METHOD_DEF, TABLE_MEMBER_REF}, 1);
    size_t member_forwarded = coded_index({TABLE_FIELD, TABLE_METHOD_DEF}, 1);
    size_t custom_attribute_type = coded_index({TABLE_METHOD_DEF, TABLE_MEMBER_REF}, 3);
    size_t resolution_scope = coded_index({TABLE_MODULE, TABLE_MODULE_REF, TABLE_ASSEMBLY_REF, TABLE_TYPE_REF}, 2);

    array<size_t, TABLE_ASSEMBLY_REF + 1> sizes {};
    sizes[TABLE_MODULE] = 2 + str + guid * 3;
    sizes[TABLE_TYPE_REF] = resolution_scope + str * 2;
    sizes[TABLE_TYPE_DEF] = 4 + str * 2 + type_def_or_ref + simple_index(TABLE_FIELD) + simple_index(TABLE_METHOD_DEF);
    sizes[TABLE_FIELD_PTR] = simple_index(TABLE_FIELD);
    sizes[TABLE_FIELD] = 2 + str + blob;
    sizes[TABLE_METHOD_PTR] = simple_index(TABLE_METHOD_DEF);
    sizes[TABLE_METHOD_DEF] = 4 + 2 + 2 + str + blob + simple_index(TABLE_PARAM);
    sizes[TABLE_PARAM_PTR] = simple_index(TABLE_PARAM);
    sizes[TABLE_PARAM] = 2 + 2 + str;
    sizes[TABLE_INTERFACE_IMPL] = simple_index(TABLE_TYPE_DEF) + type_def_or_ref;
    sizes[TABLE_MEMBER_REF] = member_ref_parent + str + blob;
    sizes[TABLE_CONSTANT] = 2 + has_constant + blob;
    sizes[TABLE_CUSTOM_ATTRIBUTE] = has_custom_attribute + custom_attribute_type + blob;
    sizes[TABLE_FIELD_MARSHAL] = has_field_marshal + blob;
    sizes[TABLE_DECL_SECURITY] = 2 + has_decl_security + blob;
    sizes[TABLE_CLASS_LAYOUT] = 2 + 4 + simple_index(TABLE_TYPE_DEF);
    sizes[TABLE_FIELD_LAYOUT] = 4 + simple_index(TABLE_FIELD);
    sizes[TABLE_STAND_ALONE_SIG] = blob;
    sizes[TABLE_EVENT_MAP] = simple_index(TABLE_TYPE_DEF) + simple_index(TABLE_EVENT);
    sizes[TABLE_EVENT_PTR] = simple_index(TABLE_EVENT);
    sizes[TABLE_EVENT] = 2 + str + type_def_or_ref;
    sizes[TABLE_PROPERTY_MAP] = simple_index(TABLE_TYPE_DEF) + simple_index(TABLE_PROPERTY);
    sizes[TABLE_PROPERTY_PTR] = simple_index(TABLE_PROPERTY);
    sizes[TABLE_PROPERTY] = 2 + str + blob;
    sizes[TABLE_METHOD_SEMANTICS] = 2 + simple_index(TABLE_METHOD_DEF) + has_semantics;
    sizes[TABLE_METHOD_IMPL] = simple_index(TABLE_TYPE_DEF) + method_def_or_ref * 2;
    sizes[TABLE_MODULE_REF] = str;
    sizes[TABLE_TYPE_SPEC] = blob;
    sizes[TABLE_IMPL_MAP] = 2 + member_forwarded + str + simple_index(TABLE_MODULE_REF);
    sizes[TABLE_FIELD_RVA] = 4 + simple_index(TABLE_FIELD);
    sizes[TABLE_ENC_LOG] = 4 + 4;
    sizes[TABLE_ENC_MAP] = 4;
    sizes[TABLE_ASSEMBLY] = 4 + 2 * 4 + 4 + blob + str * 2;
    sizes[TABLE_ASSEMBLY_PROCESSOR] = 4;
    sizes[TABLE_ASSEMBLY_OS] = 4 * 3;
    sizes[TABLE_ASSEMBLY_REF] = 2 * 4 + 4 + blob + str * 2 + blob;
    return sizes;
}

static auto GetTableOffset(const ManagedMetadataTables& tables, const array<size_t, TABLE_ASSEMBLY_REF + 1>& row_sizes, size_t table) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    size_t offset = tables.RowsOffset;

    for (size_t preceding_table = 0; preceding_table < table; preceding_table++) {
        offset += tables.Rows[preceding_table] * row_sizes[preceding_table];
    }

    return offset;
}

static auto RvaToOffset(const_span<uint8_t> image, size_t sections_offset, size_t section_count, uint32_t rva) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    for (size_t section = 0; section < section_count; section++) {
        size_t header_offset = sections_offset + section * SECTION_HEADER_SIZE;
        uint32_t virtual_size = ReadU32(image, header_offset + 8);
        uint32_t virtual_address = ReadU32(image, header_offset + 12);
        uint32_t raw_data_size = ReadU32(image, header_offset + 16);
        uint32_t raw_data_offset = ReadU32(image, header_offset + 20);

        if (rva >= virtual_address && rva - virtual_address < std::max(virtual_size, raw_data_size)) {
            uint64_t offset = uint64_t {raw_data_offset} + (rva - virtual_address);

            if (offset >= image.size()) {
                throw ManagedAssemblyReferencesException("Managed assembly RVA points past the image", rva, offset);
            }

            return numeric_cast<size_t>(offset);
        }
    }

    throw ManagedAssemblyReferencesException("Managed assembly RVA belongs to no section", rva);
}

static auto ReadHeapString(const_span<uint8_t> image, const ManagedMetadataStream& strings_stream, size_t index) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (index >= strings_stream.Size) {
        throw ManagedAssemblyReferencesException("Managed assembly string index is out of its heap", index, strings_stream.Size);
    }

    size_t begin = strings_stream.Offset + index;
    size_t end = begin;
    size_t heap_end = strings_stream.Offset + strings_stream.Size;

    while (end < heap_end && image[end] != 0) {
        end++;
    }

    if (end == heap_end) {
        throw ManagedAssemblyReferencesException("Managed assembly string runs past its heap", index);
    }

    const_span<uint8_t> bytes = image.subspan(begin, end - begin);
    return string {bytes.begin(), bytes.end()};
}

static auto ReadIndex(const_span<uint8_t> image, size_t offset, size_t index_size) -> size_t
{
    FO_STACK_TRACE_ENTRY();

    return index_size == 4 ? ReadU32(image, offset) : ReadU16(image, offset);
}

static auto ReadU8(const_span<uint8_t> image, size_t offset) -> uint8_t
{
    FO_STACK_TRACE_ENTRY();

    RequireImageBytes(image, offset, 1);
    return image[offset];
}

static auto ReadU16(const_span<uint8_t> image, size_t offset) -> uint16_t
{
    FO_STACK_TRACE_ENTRY();

    RequireImageBytes(image, offset, 2);
    return numeric_cast<uint16_t>(image[offset] | (image[offset + 1] << 8));
}

static auto ReadU32(const_span<uint8_t> image, size_t offset) -> uint32_t
{
    FO_STACK_TRACE_ENTRY();

    RequireImageBytes(image, offset, 4);
    return uint32_t {image[offset]} | (uint32_t {image[offset + 1]} << 8) | (uint32_t {image[offset + 2]} << 16) | (uint32_t {image[offset + 3]} << 24);
}

static auto ReadU64(const_span<uint8_t> image, size_t offset) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    return uint64_t {ReadU32(image, offset)} | (uint64_t {ReadU32(image, offset + 4)} << 32);
}

static void RequireImageBytes(const_span<uint8_t> image, size_t offset, size_t size)
{
    FO_STACK_TRACE_ENTRY();

    if (offset > image.size() || image.size() - offset < size) {
        throw ManagedAssemblyReferencesException("Managed assembly image is truncated", offset, size, image.size());
    }
}

FO_END_NAMESPACE

#endif
