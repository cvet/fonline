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

#include "ManagedPInvokeTable.h"

#if FO_MANAGED_SCRIPTING

FO_DISABLE_WARNINGS_PUSH()
#include <mono/utils/mono-dl-fallback.h>
FO_DISABLE_WARNINGS_POP()

FO_BEGIN_NAMESPACE

static auto ManagedInteropShimLoad(const char* name, int flags, char** err, void* user_data) -> void*;
static auto ManagedInteropShimSymbol(void* handle, const char* name, char** err, void* user_data) -> void*;
static auto ManagedInteropShimClose(void* handle, void* user_data) -> void*;
static auto FindInteropShimLibrary(string_view name) -> optional<size_t>;
static auto NormalizeInteropShimName(string_view name) -> string;

void RegisterManagedInteropShims()
{
    FO_STACK_TRACE_ENTRY();

    // Mono consults a fallback only after its own dlopen failed, so a real shared library on the host
    // still wins and this path serves exactly the shims that are linked in statically
    (void)mono_dl_fallback_register(ManagedInteropShimLoad, ManagedInteropShimSymbol, ManagedInteropShimClose, nullptr);
}

static auto ManagedInteropShimLoad(const char* name, int flags, char** err, void* user_data) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(flags);
    ignore_unused(err);
    ignore_unused(user_data);

    if (name == nullptr) {
        return nullptr;
    }

    auto first_entry = FindInteropShimLibrary(name);

    if (!first_entry.has_value()) {
        return nullptr;
    }

    // Handles are opaque to Mono and must be non-null, so the entry index is biased by one
    return reinterpret_cast<void*>(static_cast<uintptr_t>(*first_entry) + 1);
}

static auto ManagedInteropShimSymbol(void* handle, const char* name, char** err, void* user_data) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(err);
    ignore_unused(user_data);

    if (handle == nullptr || name == nullptr) {
        return nullptr;
    }

    size_t first_entry = static_cast<size_t>(reinterpret_cast<uintptr_t>(handle)) - 1;

    if (first_entry >= ManagedPInvokeEntryCount) {
        return nullptr;
    }

    string_view library = ManagedPInvokeEntries[first_entry].Library;

    for (size_t i = first_entry; i < ManagedPInvokeEntryCount; i++) {
        if (library != ManagedPInvokeEntries[i].Library) {
            break;
        }

        if (string_view {ManagedPInvokeEntries[i].EntryPoint} == name) {
            return const_cast<void*>(ManagedPInvokeEntries[i].Address);
        }
    }

    return nullptr;
}

static auto ManagedInteropShimClose(void* handle, void* user_data) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(handle);
    ignore_unused(user_data);

    return nullptr;
}

static auto FindInteropShimLibrary(string_view name) -> optional<size_t>
{
    FO_STACK_TRACE_ENTRY();

    string normalized = NormalizeInteropShimName(name);

    for (size_t i = 0; i < ManagedPInvokeEntryCount; i++) {
        if (NormalizeInteropShimName(ManagedPInvokeEntries[i].Library) == normalized) {
            return i;
        }
    }

    return std::nullopt;
}

// Mono probes a shim under several spellings ("libSystem.Native", "libSystem.Native.so", a full path),
// so both sides are reduced to the bare module name before they are compared
static auto NormalizeInteropShimName(string_view name) -> string
{
    FO_STACK_TRACE_ENTRY();

    size_t separator_pos = name.find_last_of("/\\");

    if (separator_pos != string_view::npos) {
        name.remove_prefix(separator_pos + 1);
    }

    if (name.starts_with("lib")) {
        name.remove_prefix(3);
    }

    for (string_view extension : {".so", ".dylib", ".dll", ".a", ".lib"}) {
        if (const size_t pos = name.find(extension); pos != string_view::npos) {
            name = name.substr(0, pos);
            break;
        }
    }

    return string {name};
}

FO_END_NAMESPACE

#endif
