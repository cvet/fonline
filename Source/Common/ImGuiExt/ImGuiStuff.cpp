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

#include "ImGuiStuff.h"
#include "EngineBase.h"

FO_USING_NAMESPACE();

ScriptImGui::ScriptImGui(ptr<BaseEngine> engine) :
    Entity(engine->GetPropertyRegistratorForEdit("ImGui"), nullptr, nullptr),
    _engine {engine}
{
    FO_STACK_TRACE_ENTRY();
}

static auto ImGuiAlloc(size_t sz, void* user_data) -> void*
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(user_data);

    constexpr SafeAllocator<uint8_t> allocator;
    ptr<uint8_t> bytes = allocator.allocate(sz);
    return bytes.get();
}

static void ImGuiFree(void* raw_mem, void* user_data)
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(user_data);

    auto bytes = cast_from_void<uint8_t*>(raw_mem);

    if (!bytes) {
        return;
    }

    constexpr SafeAllocator<uint8_t> allocator;
    allocator.deallocate(bytes.get(), 0);
}

void ImGuiExt::Init()
{
    FO_STACK_TRACE_ENTRY();

    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(&ImGuiAlloc, &ImGuiFree, nullptr);
    ImGui::CreateContext();
}

auto ImGuiExt::LoadIniSettingsIfContext(std::string_view ini_data) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (ini_data.empty() || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    ImGui::LoadIniSettingsFromMemory(ini_data.data(), ini_data.size());
    ImGui::GetIO().WantSaveIniSettings = false;
    return true;
}
