// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_MANAGED_SCRIPTING

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(ManagedAssemblyReferencesException);

inline constexpr string_view MANAGED_CORELIB_ASSEMBLY_NAME {"System.Private.CoreLib"};

struct ManagedAssemblyIdentity
{
    string Name {};
    vector<string> References {};
};

// Answers the runtime class library of that name, or nothing when the runtime does not publish it
using FindManagedRuntimeAssemblyCallback = function<optional<ManagedAssemblyIdentity>(string_view name)>;

// Reads the assembly's own name and the names in its AssemblyRef table from a PE image
auto ReadManagedAssemblyIdentity(const_span<uint8_t> image) -> ManagedAssemblyIdentity;

// Names every runtime class-library assembly the pack assemblies reach through assembly references; CoreLib must
// resolve and is always included, and a pack reference that neither the pack nor the runtime satisfies throws
auto CollectReferencedRuntimeAssemblies(const vector<ManagedAssemblyIdentity>& pack_assemblies, const FindManagedRuntimeAssemblyCallback& find_runtime_assembly) -> set<string>;

FO_END_NAMESPACE

#endif
