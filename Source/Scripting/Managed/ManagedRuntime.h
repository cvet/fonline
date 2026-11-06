// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_MANAGED_SCRIPTING

#include "FileSystem.h"

FO_BEGIN_NAMESPACE

inline constexpr string_view MANAGED_RUNTIME_RESOURCE_DIR {"ManagedRuntime"};
inline constexpr string_view MANAGED_RUNTIME_MANIFEST_FILE {"runtime.manifest"};

// Directory the baker writes compiled assemblies into, relative to a resource pack root; the mounted
// resources and the bake-output fallback both ask here, since a fallback of its own would match nothing
auto MakeManagedAssemblyResourceDir(string_view target_name) -> string;

auto FindManagedRuntimeDirectory() -> optional<std::filesystem::path>;
auto RestoreManagedRuntimeResources(const FileSystem& resources, string_view cache_dir) -> optional<std::filesystem::path>;

FO_END_NAMESPACE

#endif
