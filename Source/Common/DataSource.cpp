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

#include "DataSource.h"
#include "DataSource-Dat.h"
#include "DataSource-Directory.h"
#include "DataSource-Embedded.h"
#include "DataSource-Zip.h"
#include "ResourcePack.h"

FO_BEGIN_NAMESPACE

// Templated over the element so a source that owns its names and one that only views them share the filter
template<typename T>
static auto FilterFileNames(const T& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>;

auto GetFileNamesGeneric(const vector<string>& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>
{
    FO_TRACE_ZONE(FileSystem);

    return FilterFileNames(fnames, dir, recursive, ext);
}

auto GetFileNamesGeneric(const vector<string_view>& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>
{
    FO_TRACE_ZONE(FileSystem);

    return FilterFileNames(fnames, dir, recursive, ext);
}

template<typename T>
static auto FilterFileNames(const T& fnames, string_view dir, bool recursive, string_view ext) -> vector<string>
{
    string dir_fixed = strex(dir).normalize_path_slashes();

    if (!dir_fixed.empty() && dir_fixed.back() != '/') {
        dir_fixed += "/";
    }

    vector<string> result;
    unordered_set<string_view> added_names;
    auto len = dir_fixed.length();

    for (const auto& fname : fnames) {
        bool add = false;

        if (fname.compare(0, len, dir_fixed) == 0 && (recursive || (len > 0 && fname.find_last_of('/') < len) || (len == 0 && fname.find_last_of('/') == string_view::npos))) {
            if (ext.empty() || strex(fname).get_file_extension() == ext) {
                add = true;
            }
        }

        if (add && added_names.emplace(fname).second) {
            result.emplace_back(fname);
        }
    }

    return result;
}

auto MakeFileBufferHolder(unique_arr_ptr<uint8_t>&& buf) -> unique_del_ptr<const uint8_t>
{
    auto released_buf = make_ptr<const uint8_t*>(buf.release());
    return make_unique_del_ptr(released_buf, [](const uint8_t* raw_buf) noexcept {
        unique_arr_ptr<const uint8_t> owned_buf {raw_buf};
        ignore_unused(owned_buf);
    });
}

auto MakeFileBufferHolder(vector<uint8_t>&& buf) -> unique_del_ptr<const uint8_t>
{
    if (buf.empty()) {
        return MakeFileBufferHolder(safe_alloc::make_unique_arr<uint8_t>(0));
    }

    unique_nptr<vector<uint8_t>> owner = safe_alloc::make_unique<vector<uint8_t>>(std::move(buf));
    auto data = make_ptr<const uint8_t*>(owner->data());
    return make_unique_del_ptr(data, [owner = std::move(owner)](const uint8_t*) mutable noexcept { owner.reset(); });
}

auto DataSource::MountDir(string_view dir, bool recursive, bool non_cached, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    return DataSourceDirectory::Mount(dir, recursive, non_cached, maybe_not_available);
}

auto DataSource::MountPack(string_view dir, string_view name, bool maybe_not_available) -> unique_ptr<DataSource>
{
    FO_TRACE_ZONE(FileSystem);

    FO_VERIFY_AND_THROW(!name.empty(), "Pack data source mount requested an empty pack name", dir, maybe_not_available);

    auto is_file_present = [](string_view path) -> bool { return static_cast<bool>(fs::open_ifstream(path)); };

    string path = strex(dir).combine_path(name);

    if (name == EMBEDDED_PACK_NAME) {
        return DataSourceEmbedded::Mount();
    }
    else if (OpenResourcePackFile(strex("{}.fores", path).str())) {
        string base_path = strex("{}.fores", path).str();
        return safe_alloc::make_unique<ResourcePackSource>(base_path, GetResourcePatchPath(base_path));
    }
    else if (is_file_present(strex("{}.zip", path))) {
#if FO_DATA_SOURCE_ZIP
        return DataSourceZip::Mount(strex("{}.zip", path));
#else
        throw DataSourceException("Archive provider is disabled; enable FO_DATA_SOURCE_ZIP", path);
#endif
    }
    else if (is_file_present(strex("{}.bos", path))) {
#if FO_DATA_SOURCE_ZIP
        return DataSourceZip::Mount(strex("{}.bos", path));
#else
        throw DataSourceException("Archive provider is disabled; enable FO_DATA_SOURCE_ZIP", path);
#endif
    }
    else if (is_file_present(strex("{}.dat", path))) {
#if FO_DATA_SOURCE_DAT
        return DataSourceDat::Mount(strex("{}.dat", path));
#else
        throw DataSourceException("Archive provider is disabled; enable FO_DATA_SOURCE_DAT", path);
#endif
    }

    if (maybe_not_available) {
        return DataSourceDirectory::CreateEmpty();
    }

    throw DataSourceException("Data pack not found", path);
}

FO_END_NAMESPACE
