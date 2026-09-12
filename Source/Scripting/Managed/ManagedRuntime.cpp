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

#include "ManagedRuntime.h"

#if FO_MANAGED_SCRIPTING

#include "Platform.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

struct ManagedRuntimeResource
{
    string ResourcePath {};
    std::filesystem::path RelativePath {};
    vector<uint8_t> Data {};
};

static mutex ManagedRuntimeRestoreLocker;

static auto IsRuntimeLayoutPath(const std::filesystem::path& dir) -> bool;
static auto CollectManagedRuntimeResources(const FileSystem& resources) -> vector<ManagedRuntimeResource>;
static auto MakeManagedRuntimeCacheKey(const vector<ManagedRuntimeResource>& runtime_resources) noexcept -> string;
static auto IsSameManagedRuntimeCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> data) -> bool;
static auto IsSameManagedRuntimeCache(const std::filesystem::path& cache_root, const vector<ManagedRuntimeResource>& runtime_resources) -> bool;

auto FindManagedRuntimeDirectory() -> optional<std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    vector<std::filesystem::path> candidates;

    if (const char* explicit_dir = std::getenv("FO_MANAGED_RUNTIME"); explicit_dir != nullptr && explicit_dir[0] != '\0') {
        candidates.emplace_back(explicit_dir);
    }

    candidates.emplace_back(std::filesystem::current_path() / fs_make_path(MANAGED_RUNTIME_RESOURCE_DIR));

    if (auto exe_path = Platform::GetExePath()) {
        auto exe_dir = std::filesystem::path(fs_make_path(*exe_path)).parent_path();
        candidates.emplace_back(exe_dir / fs_make_path(MANAGED_RUNTIME_RESOURCE_DIR));
    }

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(candidate, ec);
        const std::filesystem::path& runtime_dir = !ec ? normalized : candidate;

        if (IsRuntimeLayoutPath(runtime_dir)) {
            return runtime_dir;
        }
    }

    return std::nullopt;
}

auto RestoreManagedRuntimeResources(const FileSystem& resources, string_view cache_dir) -> optional<std::filesystem::path>
{
    FO_STACK_TRACE_ENTRY();

    vector<ManagedRuntimeResource> runtime_resources = CollectManagedRuntimeResources(resources);

    if (runtime_resources.empty()) {
        return std::nullopt;
    }

    if (cache_dir.empty()) {
        throw ScriptSystemException("Managed runtime resources require a cache directory");
    }

    scoped_lock restore_locker {ManagedRuntimeRestoreLocker};
    auto runtime_cache_root = std::filesystem::path {fs_make_path(cache_dir)} / fs_make_path(MANAGED_RUNTIME_RESOURCE_DIR);
    string cache_key = MakeManagedRuntimeCacheKey(runtime_resources);
    auto cache_root = runtime_cache_root / fs_make_path(cache_key);

    if (IsSameManagedRuntimeCache(cache_root, runtime_resources)) {
        return cache_root;
    }

    string runtime_cache_root_str = fs_path_to_string(runtime_cache_root);

    if (!fs_create_directories(runtime_cache_root_str)) {
        throw ScriptSystemException("Can't create Managed runtime cache root", runtime_cache_root_str);
    }

    auto staged_root = runtime_cache_root / fs_make_path(strex(".{}.tmp-{}", cache_key, Platform::GetCurrentProcessIdStr()).str());
    string staged_root_str = fs_path_to_string(staged_root);
    (void)fs_remove_dir_tree(staged_root_str);
    auto cleanup_staged = scope_exit([&staged_root_str]() noexcept { (void)fs_remove_dir_tree(staged_root_str); });

    for (const ManagedRuntimeResource& resource : runtime_resources) {
        auto disk_path = staged_root / resource.RelativePath;
        string disk_dir = fs_path_to_string(disk_path.parent_path());

        if (!fs_create_directories(disk_dir)) {
            throw ScriptSystemException("Can't create Managed runtime cache directory", disk_dir);
        }
        if (!fs_write_file(disk_path.string(), resource.Data)) {
            throw ScriptSystemException("Can't restore Managed runtime resource", resource.ResourcePath);
        }
    }

    if (!IsSameManagedRuntimeCache(staged_root, runtime_resources)) {
        throw ScriptSystemException("Staged Managed runtime cache validation failed", staged_root.string());
    }

    // Prefer another process's byte-identical completed cache without touching files Mono may hold open
    if (IsSameManagedRuntimeCache(cache_root, runtime_resources)) {
        return cache_root;
    }

    string cache_root_str = fs_path_to_string(cache_root);

    if (fs_exists(cache_root_str) && !fs_remove_dir_tree(cache_root_str)) {
        throw ScriptSystemException("Can't replace invalid Managed runtime cache", cache_root_str);
    }
    if (!fs_rename(staged_root_str, cache_root_str)) {
        if (IsSameManagedRuntimeCache(cache_root, runtime_resources)) {
            return cache_root;
        }

        throw ScriptSystemException("Can't publish Managed runtime cache", staged_root_str, cache_root_str);
    }

    return cache_root;
}

static auto IsRuntimeLayoutPath(const std::filesystem::path& dir) -> bool
{
    FO_STACK_TRACE_ENTRY();

    std::error_code ec;
    return std::filesystem::is_regular_file(dir / fs_make_path(MANAGED_RUNTIME_MANIFEST_FILE), ec) && std::filesystem::is_regular_file(dir / "lib" / "netcoreapp" / "System.Private.CoreLib.dll", ec);
}

static auto CollectManagedRuntimeResources(const FileSystem& resources) -> vector<ManagedRuntimeResource>
{
    FO_STACK_TRACE_ENTRY();

    vector<ManagedRuntimeResource> result;
    string resource_prefix = strex("{}/", MANAGED_RUNTIME_RESOURCE_DIR).str();

    for (const FileHeader& file : resources.FilterFiles("", MANAGED_RUNTIME_RESOURCE_DIR, true)) {
        string resource_path {file.GetPath()};

        if (!resource_path.starts_with(resource_prefix)) {
            throw ScriptSystemException("Invalid Managed runtime resource path", resource_path);
        }

        string relative_path_str = resource_path.substr(resource_prefix.length());
        std::filesystem::path relative_path {fs_make_path(relative_path_str)};

        if (relative_path.empty() || relative_path.is_absolute() || std::ranges::any_of(relative_path, [](const std::filesystem::path& component) { return component == ".."; })) {
            throw ScriptSystemException("Unsafe Managed runtime resource path", resource_path);
        }

        auto runtime_file = resources.ReadFile(resource_path);

        if (!runtime_file) {
            throw ScriptSystemException("Can't read Managed runtime resource", resource_path);
        }

        result.emplace_back(ManagedRuntimeResource {
            .ResourcePath = resource_path,
            .RelativePath = std::move(relative_path),
            .Data = runtime_file.GetData(),
        });
    }

    std::ranges::sort(result, {}, &ManagedRuntimeResource::ResourcePath);
    return result;
}

static auto MakeManagedRuntimeCacheKey(const vector<ManagedRuntimeResource>& runtime_resources) noexcept -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    uint64_t hash = 1469598103934665603ull;
    auto add_byte = [&](uint8_t byte) noexcept {
        hash ^= byte;
        hash *= 1099511628211ull;
    };

    for (const ManagedRuntimeResource& resource : runtime_resources) {
        for (char c : resource.ResourcePath) {
            add_byte(static_cast<uint8_t>(c));
        }

        add_byte(0);

        for (uint8_t byte : resource.Data) {
            add_byte(byte);
        }

        add_byte(0);
    }

    return strex("{}", hash).str();
}

static auto IsSameManagedRuntimeCacheFile(const std::filesystem::path& disk_path, const_span<uint8_t> data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    auto existing_data = fs_read_file(disk_path.string());

    if (!existing_data.has_value() || existing_data->size() != data.size()) {
        return false;
    }

    for (size_t i = 0; i != data.size(); i++) {
        if (static_cast<uint8_t>((*existing_data)[i]) != data[i]) {
            return false;
        }
    }

    return true;
}

static auto IsSameManagedRuntimeCache(const std::filesystem::path& cache_root, const vector<ManagedRuntimeResource>& runtime_resources) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (!IsRuntimeLayoutPath(cache_root)) {
        return false;
    }

    return std::ranges::all_of(runtime_resources, [&](const ManagedRuntimeResource& resource) { return IsSameManagedRuntimeCacheFile(cache_root / resource.RelativePath, resource.Data); });
}

FO_END_NAMESPACE

#endif
