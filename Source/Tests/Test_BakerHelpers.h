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

#pragma once

#include "Common.h"

#include "AngelScriptScripting.h"
#include "Baker.h"
#include "DataSerialization.h"
#include "DataSource.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace BakerTests
{
    template<typename T, typename U>
    inline void OverrideSetting(T& setting, U&& value)
    {
        using setting_type = std::remove_cvref_t<T>;
        const_cast<setting_type&>(setting) = setting_type {std::forward<U>(value)};
    }

    inline void ApplySelfContainedClientSettings(GlobalSettings& settings)
    {
        settings.ScreenWidth = 320;
        settings.ScreenHeight = 200;
        settings.DisableAudio = true;
        OverrideSetting(settings.NullRenderer, true);
        OverrideSetting(settings.CritterStubSpriteName, string {});
        OverrideSetting(settings.ItemStubSpriteName, string {});
    }

    inline void ApplySelfContainedServerSettings(GlobalSettings& settings)
    {
        OverrideSetting(settings.DisableNetworking, true);
        OverrideSetting(settings.OpLogEnabled, false);
    }

    inline auto MakeEmptyMetadataBlob() -> vector<uint8>
    {
        vector<uint8> metadata;
        auto writer = DataWriter(metadata);
        writer.Write<uint16>(uint16 {0});
        return metadata;
    }

    template<typename ProtoType>
    inline auto MakeSingleProtoResourceBlob(EngineMetadata& meta, hstring type_name, string_view proto_name) -> vector<uint8>
    {
        vector<uint8> props_data;
        set<hstring> str_hashes;

        ProtoType proto {meta.Hashes.ToHashedString(proto_name), meta.GetPropertyRegistrator(type_name)};
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32>(uint32 {0});
        ignore_unused(str_hashes);
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint32>(uint32 {1});
        writer.Write<uint16>(numeric_cast<uint16>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());
        writer.Write<uint16>(numeric_cast<uint16>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());
        writer.Write<uint32>(numeric_cast<uint32>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());

        return protos_data;
    }

    class MemoryDataSource final : public DataSource
    {
    public:
        explicit MemoryDataSource(string pack_name) :
            _packName {std::move(pack_name)}
        {
        }

        struct Entry
        {
            string Path {};
            vector<uint8> Data {};
            uint64 WriteTime {};
        };

        [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
        [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }

        void AddFile(string_view path, string_view content, uint64 write_time = 1) { AddFile(path, vector<uint8> {content.begin(), content.end()}, write_time); }

        void AddFile(string_view path, vector<uint8> data, uint64 write_time = 1) { _entries[string(path)] = Entry {.Path = string(path), .Data = std::move(data), .WriteTime = write_time}; }

        [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _entries.contains(string(path)); }

        [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64& write_time) const -> bool override
        {
            const auto it = _entries.find(string(path));

            if (it == _entries.end()) {
                return false;
            }

            size = it->second.Data.size();
            write_time = it->second.WriteTime;
            return true;
        }

        [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64& write_time) const -> unique_del_ptr<const uint8> override
        {
            const auto it = _entries.find(string(path));

            if (it == _entries.end()) {
                size = 0;
                write_time = 0;
                return nullptr;
            }

            size = it->second.Data.size();
            write_time = it->second.WriteTime;

            auto buf = SafeAlloc::MakeUniqueArr<uint8>(size);

            if (size != 0u) {
                MemCopy(buf.get(), it->second.Data.data(), size);
            }

            return unique_del_ptr<const uint8> {buf.release(), [](const uint8* p) FO_DEFERRED { delete[] p; }};
        }

        [[nodiscard]] auto GetFileNames(string_view dir, bool recursive, string_view ext) const -> vector<string> override
        {
            vector<string> result;

            for (const auto& [path, entry] : _entries) {
                ignore_unused(entry);

                if (!ext.empty() && strex(path).get_file_extension() != ext) {
                    continue;
                }

                if (!dir.empty()) {
                    const string path_dir = strex(path).extract_dir().str();

                    if (recursive) {
                        if (!path.starts_with(string(dir))) {
                            continue;
                        }
                    }
                    else if (path_dir != dir) {
                        continue;
                    }
                }

                result.emplace_back(path);
            }

            std::ranges::sort(result);
            return result;
        }

    private:
        string _packName {};
        unordered_map<string, Entry> _entries {};
    };

    class MemoryFileSet final
    {
    public:
        explicit MemoryFileSet(string pack_name)
        {
            auto ds = SafeAlloc::MakeUnique<MemoryDataSource>(std::move(pack_name));
            _dataSource = ds.get();
            _fileSystem.AddCustomSource(std::move(ds));
        }

        void AddTextFile(string_view path, string_view content, uint64 write_time = 1) { _dataSource->AddFile(path, content, write_time); }

        void AddBinaryFile(string_view path, vector<uint8> content, uint64 write_time = 1) { _dataSource->AddFile(path, std::move(content), write_time); }

        [[nodiscard]] auto ReadFile(string_view path) const -> File { return _fileSystem.ReadFile(path); }

        [[nodiscard]] auto GetFileSystem() const -> const FileSystem& { return _fileSystem; }

    private:
        raw_ptr<MemoryDataSource> _dataSource {};
        FileSystem _fileSystem {};
    };

    inline auto CompileInlineScripts(EngineMetadata* meta, string_view pack_name, const vector<pair<string, string>>& script_files, function<void(string_view)> message_callback) -> vector<uint8>
    {
        MemoryFileSet source_files {string(pack_name)};
        vector<File> files;
        files.reserve(script_files.size());

        for (const auto& [path, content] : script_files) {
            source_files.AddTextFile(path, content);
        }

        for (const auto& [path, content] : script_files) {
            ignore_unused(content);
            files.emplace_back(source_files.ReadFile(path));
        }

        return CompileAngelScript(meta, files, std::move(message_callback));
    }

    class TestRig final
    {
    public:
        TestRig() :
            Settings(true)
        {
            Settings.ApplyDefaultSettings();

            auto source_ds = SafeAlloc::MakeUnique<MemoryDataSource>("Tests");
            _sourceData = source_ds.get();
            SourceFiles.AddCustomSource(std::move(source_ds));

            auto baked_ds = SafeAlloc::MakeUnique<MemoryDataSource>("Baked");
            _bakedData = baked_ds.get();
            BakedFiles.AddCustomSource(std::move(baked_ds));
        }

        void AddSourceFile(string_view path, string_view content, uint64 write_time = 1) { _sourceData->AddFile(path, content, write_time); }

        void AddBakedFile(string_view path, string_view content, uint64 write_time = 1) { _bakedData->AddFile(path, content, write_time); }

        [[nodiscard]] auto GetAllSourceFiles() const -> FileCollection { return SourceFiles.GetAllFiles(); }

        [[nodiscard]] static auto MakeEmptyFiles() -> FileCollection { return FileCollection(vector<FileHeader> {}); }

        auto MakeContext(string_view pack_name = "TestPack", BakeCheckerCallback bake_checker = BakeCheckerCallback {}) -> shared_ptr<BakingContext>
        {
            Outputs.clear();

            if (!bake_checker) {
                bake_checker = [](string_view, uint64) { return true; };
            }

            return SafeAlloc::MakeShared<BakingContext>(BakingContext {
                .Settings = &Settings,
                .PackName = string(pack_name),
                .BakeChecker = std::move(bake_checker),
                .WriteData = [this](string_view path, const_span<uint8> data) { Outputs[string(path)] = vector<uint8> {data.begin(), data.end()}; },
                .BakedFiles = &BakedFiles,
                .ForceSyncMode = true,
            });
        }

        [[nodiscard]] auto GetOutputText(string_view path) const -> string
        {
            const auto it = Outputs.find(string(path));
            FO_RUNTIME_ASSERT(it != Outputs.end());
            return string {it->second.begin(), it->second.end()};
        }

        GlobalSettings Settings;
        FileSystem SourceFiles {};
        FileSystem BakedFiles {};
        unordered_map<string, vector<uint8>> Outputs {};

    private:
        raw_ptr<MemoryDataSource> _sourceData {};
        raw_ptr<MemoryDataSource> _bakedData {};
    };

    inline auto MakeRequestedBakers(const vector<string>& request_bakers, TestRig& rig, string_view pack_name = "TestPack") -> vector<unique_ptr<BaseBaker>>
    {
        return BaseBaker::SetupBakers(request_bakers, string(pack_name), rig.Settings, [](string_view, uint64) { return true; }, [](string_view, const_span<uint8>) {}, &rig.BakedFiles);
    }
}

FO_END_NAMESPACE
