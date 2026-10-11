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

#include "UpdateDescriptor.h"

FO_BEGIN_NAMESPACE

void WriteUpdateDescriptor(vector<uint8_t>& desc, const_span<UpdateDescriptorEntry> entries)
{
    // Every entry is checked before the first byte lands in the caller's buffer, so a refusal leaves it as it was
    for (const UpdateDescriptorEntry& entry : entries) {
        FO_VERIFY_AND_THROW(!entry.Name.empty() && entry.Name.length() <= numeric_cast<size_t>(std::numeric_limits<int16_t>::max()), "Update file name length is out of range", entry.Name);
        FO_VERIFY_AND_THROW((entry.Target == UpdateFileTarget::ClientResources) == entry.PackHeader.has_value(), "Update target/header mismatch", entry.Name);
    }

    auto writer = data_writer(desc);

    for (const UpdateDescriptorEntry& entry : entries) {
        writer.write<int16_t>(numeric_cast<int16_t>(entry.Name.length()));
        writer.write_string_bytes(entry.Name);
        writer.write<uint64_t>(entry.Size);
        writer.write<uint64_t>(entry.Hash);
        writer.write<UpdateFileTarget>(entry.Target);
        writer.write<uint32_t>(entry.FileIndex);

        if (entry.PackHeader.has_value()) {
            vector<uint8_t> header = SerializeResourcePackHeader(entry.PackHeader.value());
            writer.write<uint32_t>(numeric_cast<uint32_t>(header.size()));
            writer.write_bytes(header);
        }
        else {
            writer.write<uint32_t>(uint32_t {0});
        }
    }

    writer.write<int16_t>(const_numeric_cast<int16_t>(-1));
}

auto ReadUpdateDescriptor(const_span<uint8_t> desc) -> vector<UpdateDescriptorEntry>
{
    vector<UpdateDescriptorEntry> entries;
    auto reader = data_reader(desc);

    while (true) {
        int16_t name_len = reader.read<int16_t>();

        if (name_len == -1) {
            break;
        }

        FO_VERIFY_AND_THROW(name_len > 0, "Update file name length must be positive", name_len);
        UpdateDescriptorEntry entry;
        entry.Name.resize(numeric_cast<size_t>(name_len));
        reader.read_string_bytes(entry.Name);
        FO_VERIFY_AND_THROW(fs::is_contained_relative_path(entry.Name), "Invalid update file path", entry.Name);
        entry.Size = reader.read<uint64_t>();
        entry.Hash = reader.read<uint64_t>();
        entry.Target = reader.read<UpdateFileTarget>();
        FO_VERIFY_AND_THROW(entry.Target == UpdateFileTarget::ClientResources || entry.Target == UpdateFileTarget::ClientBinaries, "Invalid update target", entry.Target);
        entry.FileIndex = reader.read<uint32_t>();
        uint32_t header_size = reader.read<uint32_t>();
        FO_VERIFY_AND_THROW(header_size == 0 || header_size == RESOURCE_PACK_HEADER_SIZE, "Invalid update resource header size", header_size);

        if (header_size != 0) {
            ResourcePackHeader pack_header;
            const_span<uint8_t> header_bytes = reader.read_bytes(header_size);
            bool header_parsed = ParseResourcePackHeader(header_bytes, pack_header);
            FO_VERIFY_AND_THROW(header_parsed, "Invalid advertised resource header", entry.Name);
            entry.PackHeader = pack_header;
        }

        // A resource entry names a full base; its patch is the client's own and never travels as a file
        bool is_resource = entry.Target == UpdateFileTarget::ClientResources;
        FO_VERIFY_AND_THROW(is_resource == entry.PackHeader.has_value(), "Update target/header mismatch", entry.Name);
        FO_VERIFY_AND_THROW(!is_resource || (entry.Name.ends_with(".fores") && !entry.Name.ends_with(".patch.fores")), "Update resource entry is not a base pack", entry.Name);
        entries.emplace_back(std::move(entry));
    }

    reader.verify_end();
    return entries;
}

FO_END_NAMESPACE
