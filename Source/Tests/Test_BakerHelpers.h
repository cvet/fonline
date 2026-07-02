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
    template<typename T>
    [[nodiscard]] inline auto FixedSettingForOverride(const T& setting) noexcept -> ptr<T>
    {
        FO_NO_STACK_TRACE_ENTRY();

        return const_cast<T*>(&setting);
    }

    template<typename T, typename U>
    inline void OverrideSetting(T& setting, U&& value)
    {
        using setting_type = std::remove_cvref_t<T>;
        auto mutable_setting = FixedSettingForOverride(setting);
        *mutable_setting = setting_type {std::forward<U>(value)};
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

    // Test rigs embed tiny scripts that intentionally use mutable module-level globals as
    // observation hooks. List every namespace those embedded scripts declare; production scripts
    // compile through their own settings instance and stay gated. The gate-test namespace
    // (MutableGlobal) is intentionally excluded so the gate-test still fires.
    inline auto GetTestMutableGlobalsAllowedNamespaces() -> vector<string>
    {
        return {
            "ServerEngineTest",
            "CommonMethods",
            "EntityOps",
            "EntityLifecycle",
            "ServerItemsTest",
            "AdvOps",
            "ScriptMethodsTest",
            "MapOpsTest",
            "ScriptBuiltins",
            "LocEntity",
            "ClientEngineTest",
            "ClientServerIntegrationServer",
            "ClientServerIntegrationClient",
            "AttrTest",
            "Footsteps",
            "CritterState",
            "Deterioration",
            "RemoteCallTest",
            "ItemTriggerTest",
            "ItemStaticTest",
            "TestSettings",
            "TestMigration",
            "TestRefTypeProps",
            "TestRefTypeEntityProps",
            "TestNestedRefTypeProps",
            "TestLegacyRefType",
            "TestRefTypeFlags",
            "TestValueTypePropertyOwner",
        };
    }

    inline void ApplySelfContainedServerSettings(GlobalSettings& settings)
    {
        OverrideSetting(settings.DisableNetworking, true);
        OverrideSetting(settings.OpLogEnabled, false);
        OverrideSetting(settings.MutableGlobalsAllowedNamespaces, GetTestMutableGlobalsAllowedNamespaces());
    }

    inline auto MakeScriptCompilerSettings() -> GlobalSettings
    {
        auto settings = GlobalSettings(false);
        settings.ApplyDefaultSettings();
        OverrideSetting(settings.MutableGlobalsAllowedNamespaces, GetTestMutableGlobalsAllowedNamespaces());
        return settings;
    }

    inline auto MakeEmptyMetadataBlob() -> vector<uint8_t>
    {
        vector<uint8_t> metadata;
        auto writer = DataWriter(metadata);
        writer.Write<uint16_t>(uint16_t {0});
        return metadata;
    }

    inline void CleanupMemoryDataSourceFileBuffer(ptr<const uint8_t> p) FO_DEFERRED
    {
        FO_STACK_TRACE_ENTRY();

        unique_arr_ptr<const uint8_t> owned_buf {p.get()};
        ignore_unused(owned_buf);
    }

    inline auto MakeMemoryDataSourceFileBufferHolder(unique_arr_ptr<uint8_t>&& buf) -> unique_del_ptr<const uint8_t>
    {
        FO_STACK_TRACE_ENTRY();

        ptr<const uint8_t> released_buf = std::move(buf).release();
        return make_unique_del_ptr(released_buf, CleanupMemoryDataSourceFileBuffer);
    }

    template<typename ProtoType>
    inline auto MakeSingleProtoResourceBlob(EngineMetadata& meta, hstring type_name, string_view proto_name) -> vector<uint8_t>
    {
        vector<uint8_t> props_data;
        set<hstring> str_hashes;

        auto registrator = meta.GetPropertyRegistrator(type_name);
        REQUIRE(static_cast<bool>(registrator));

        ProtoType proto {meta.Hashes.ToHashedString(proto_name), registrator.as_ptr()};
        proto.GetProperties().StoreAllData(props_data, str_hashes);

        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        ignore_unused(str_hashes);
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WriteStringBytes(type_name.as_str());
        writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
        writer.WriteStringBytes(proto_name);
        writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
        if (!props_data.empty()) {
            writer.WriteBytes({props_data.data(), props_data.size()});
        }

        return protos_data;
    }

    // Same single-type proto resource blob as MakeSingleProtoResourceBlob, but packs several protos of
    // the same entity type into one pack and lets the caller mutate each proto's default properties via
    // a configure callback (invoked before properties are serialized). Matches the proto pack format read
    // by ProtoManager::LoadFromResources: u32 hashes_count, u32 types_count, then per type
    // { u32 protos_count, u16+type_name, per proto { u16+proto_name, u32+props_data } }.
    template<typename ProtoType>
    inline auto MakeMultiProtoResourceBlob(EngineMetadata& meta, hstring type_name, const vector<pair<string, function<void(ProtoType&)>>>& protos) -> vector<uint8_t>
    {
        vector<uint8_t> protos_data;
        auto writer = DataWriter(protos_data);

        writer.Write<uint32_t>(uint32_t {0});
        writer.Write<uint32_t>(uint32_t {1});
        writer.Write<uint32_t>(numeric_cast<uint32_t>(protos.size()));
        writer.Write<uint16_t>(numeric_cast<uint16_t>(type_name.as_str().length()));
        writer.WritePtr(type_name.as_str().data(), type_name.as_str().length());

        for (const auto& [proto_name, configure] : protos) {
            ProtoType proto {meta.Hashes.ToHashedString(proto_name), meta.GetPropertyRegistrator(type_name).as_ptr()};

            if (configure) {
                configure(proto);
            }

            vector<uint8_t> props_data;
            set<hstring> str_hashes;
            proto.GetProperties().StoreAllData(props_data, str_hashes);
            ignore_unused(str_hashes);

            writer.Write<uint16_t>(numeric_cast<uint16_t>(proto_name.length()));
            writer.WritePtr(proto_name.data(), proto_name.length());
            writer.Write<uint32_t>(numeric_cast<uint32_t>(props_data.size()));
            writer.WritePtr(props_data.data(), props_data.size());
        }

        return protos_data;
    }

    // Minimal valid baked sprite blob (the magic-42 single-frame format read by
    // DefaultSpriteFactory::LoadSprite). Produces a width x height fully-opaque white image so that
    // headless font/sprite binding succeeds under NullRenderer without shipping real baked art.
    inline auto MakeMinimalBakedSprite(uint16_t width = 1, uint16_t height = 1) -> vector<uint8_t>
    {
        vector<uint8_t> sprite_data;
        auto writer = DataWriter(sprite_data);

        writer.Write<uint8_t>(uint8_t {42}); // Header magic
        writer.Write<uint16_t>(uint16_t {1}); // Frames count
        writer.Write<uint16_t>(uint16_t {0}); // Ticks
        writer.Write<uint8_t>(uint8_t {1}); // Directions

        writer.Write<int16_t>(int16_t {0}); // Offset x
        writer.Write<int16_t>(int16_t {0}); // Offset y

        writer.Write<uint8_t>(uint8_t {0}); // Not a sprite reference
        writer.Write<uint16_t>(width);
        writer.Write<uint16_t>(height);
        writer.Write<int16_t>(int16_t {0}); // Frame x
        writer.Write<int16_t>(int16_t {0}); // Frame y

        const auto pixel_count = numeric_cast<size_t>(width) * height;

        for (size_t i = 0; i < pixel_count; i++) {
            writer.Write<uint8_t>(uint8_t {255}); // R
            writer.Write<uint8_t>(uint8_t {255}); // G
            writer.Write<uint8_t>(uint8_t {255}); // B
            writer.Write<uint8_t>(uint8_t {255}); // A
        }

        writer.Write<uint8_t>(uint8_t {42}); // Frame magic

        return sprite_data;
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
            vector<uint8_t> Data {};
            uint64_t WriteTime {};
        };

        [[nodiscard]] auto IsDiskDir() const -> bool override { return false; }
        [[nodiscard]] auto GetPackName() const -> string_view override { return _packName; }

        void AddFile(string_view path, string_view content, uint64_t write_time = 1) { AddFile(path, vector<uint8_t> {content.begin(), content.end()}, write_time); }

        void AddFile(string_view path, vector<uint8_t> data, uint64_t write_time = 1) { _entries[string(path)] = Entry {.Path = string(path), .Data = std::move(data), .WriteTime = write_time}; }

        [[nodiscard]] auto IsFileExists(string_view path) const -> bool override { return _entries.contains(string(path)); }

        [[nodiscard]] auto GetFileInfo(string_view path, size_t& size, uint64_t& write_time) const -> bool override
        {
            const auto it = _entries.find(string(path));

            if (it == _entries.end()) {
                return false;
            }

            size = it->second.Data.size();
            write_time = it->second.WriteTime;
            return true;
        }

        [[nodiscard]] auto OpenFile(string_view path, size_t& size, uint64_t& write_time) const -> unique_del_nptr<const uint8_t> override
        {
            const auto it = _entries.find(string(path));

            if (it == _entries.end()) {
                size = 0;
                write_time = 0;
                return nullptr;
            }

            size = it->second.Data.size();
            write_time = it->second.WriteTime;

            auto buf = SafeAlloc::MakeUniqueArr<uint8_t>(size);

            if (size != 0u) {
                ptr<uint8_t> buf_ptr = buf.get();
                MemCopy(buf_ptr.get(), it->second.Data.data(), size);
            }

            return MakeMemoryDataSourceFileBufferHolder(std::move(buf));
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

        void AddTextFile(string_view path, string_view content, uint64_t write_time = 1) { _dataSource->AddFile(path, content, write_time); }

        void AddBinaryFile(string_view path, vector<uint8_t> content, uint64_t write_time = 1) { _dataSource->AddFile(path, std::move(content), write_time); }

        [[nodiscard]] auto ReadFile(string_view path) const -> File { return _fileSystem.ReadFile(path); }

        [[nodiscard]] auto GetFileSystem() const -> const FileSystem& { return _fileSystem; }

    private:
        nptr<MemoryDataSource> _dataSource {};
        FileSystem _fileSystem {};
    };

    inline auto GetTestSettings() -> ptr<GlobalSettings>
    {
        // GlobalSettings holds member references back to *this (Common, Network, ...), so it can't
        // be safely moved out of a lambda. Construct in place and initialize once via call_once.
        static GlobalSettings instance(true);
        static std::once_flag once;
        std::call_once(once, [] { instance.ApplyDefaultSettings(); });
        return &instance;
    }

    inline auto CompileInlineScripts(ptr<EngineMetadata> meta, const ScriptSettings& script_settings, string_view pack_name, const vector<pair<string, string>>& script_files, function<void(string_view)> message_callback) -> vector<uint8_t>
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

        return CompileAngelScript(meta, script_settings, files, std::move(message_callback));
    }

    inline auto CompileInlineScripts(ptr<EngineMetadata> meta, string_view pack_name, const vector<pair<string, string>>& script_files, function<void(string_view)> message_callback) -> vector<uint8_t>
    {
        auto script_settings = MakeScriptCompilerSettings();
        return CompileInlineScripts(meta, script_settings, pack_name, script_files, std::move(message_callback));
    }

    class TestRig final
    {
    public:
        TestRig() :
            Settings(true)
        {
            Settings.ApplyDefaultSettings();
            // Match MakeScriptCompilerSettings — the gate also fires at runtime when ServerEngine
            // loads bytecode, so the runtime settings need the same allowlist as the compile-time
            // ones. The gate-test (Test_AngelScriptBaker) intentionally bypasses this default by
            // re-overriding the field on its TestRig instance before compiling.
            OverrideSetting(Settings.MutableGlobalsAllowedNamespaces, GetTestMutableGlobalsAllowedNamespaces());

            auto source_ds = SafeAlloc::MakeUnique<MemoryDataSource>("Tests");
            _sourceData = source_ds.get();
            SourceFiles.AddCustomSource(std::move(source_ds));

            auto baked_ds = SafeAlloc::MakeUnique<MemoryDataSource>("Baked");
            _bakedData = baked_ds.get();
            BakedFiles.AddCustomSource(std::move(baked_ds));
        }

        void AddSourceFile(string_view path, string_view content, uint64_t write_time = 1) { _sourceData->AddFile(path, content, write_time); }

        void AddBakedFile(string_view path, string_view content, uint64_t write_time = 1) { _bakedData->AddFile(path, content, write_time); }

        void AddBakedFile(string_view path, vector<uint8_t> content, uint64_t write_time = 1) { _bakedData->AddFile(path, std::move(content), write_time); }

        [[nodiscard]] auto GetAllSourceFiles() const -> FileCollection { return SourceFiles.GetAllFiles(); }

        [[nodiscard]] static auto MakeEmptyFiles() -> FileCollection { return FileCollection(vector<FileHeader> {}); }

        auto MakeContext(string_view pack_name = "TestPack", BakeCheckerCallback bake_checker = BakeCheckerCallback {}) -> shared_ptr<BakingContext>
        {
            Outputs.clear();

            if (!bake_checker) {
                bake_checker = [](string_view, uint64_t) { return true; };
            }

            nptr<const BakingSettings> settings_ptr = &Settings;
            nptr<const FileSystem> baked_files_ptr = &BakedFiles;

            return SafeAlloc::MakeShared<BakingContext>(BakingContext {
                .Settings = settings_ptr,
                .PackName = string(pack_name),
                .BakeChecker = std::move(bake_checker),
                .WriteData = [this](string_view path, const_span<uint8_t> data) { Outputs[string(path)] = vector<uint8_t> {data.begin(), data.end()}; },
                .BakedFiles = baked_files_ptr,
                .ForceSyncMode = true,
            });
        }

        [[nodiscard]] auto GetOutputText(string_view path) const -> string
        {
            const auto it = Outputs.find(string(path));
            FO_VERIFY_AND_THROW(it != Outputs.end(), "Lookup failed in outputs");
            return string {it->second.begin(), it->second.end()};
        }

        GlobalSettings Settings;
        FileSystem SourceFiles {};
        FileSystem BakedFiles {};
        unordered_map<string, vector<uint8_t>> Outputs {};

    private:
        nptr<MemoryDataSource> _sourceData {};
        nptr<MemoryDataSource> _bakedData {};
    };

    inline auto MakeRequestedBakers(const vector<string>& request_bakers, TestRig& rig, string_view pack_name = "TestPack") -> vector<unique_ptr<BaseBaker>>
    {
        return BaseBaker::SetupBakers(request_bakers, string(pack_name), rig.Settings, [](string_view, uint64_t) { return true; }, [](string_view, const_span<uint8_t>) {}, &rig.BakedFiles);
    }
}

FO_END_NAMESPACE
