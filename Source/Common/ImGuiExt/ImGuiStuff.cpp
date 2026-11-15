// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImGuiStuff.h"

#include "EngineBase.h"

FO_USING_NAMESPACE();

ScriptImGui::ScriptImGui(ptr<BaseEngine> engine) :
    Entity(engine->GetPropertyRegistrarForEdit("ImGui"), nullptr, nullptr),
    _engine {engine}
{
}

static auto ImGuiAlloc(size_t sz, void* user_data) -> void*
{
    ignore_unused(user_data);

    constexpr safe_allocator<uint8_t> allocator {};
    ptr<uint8_t> bytes {allocator.allocate(sz)};
    return bytes.get();
}

static void ImGuiFree(void* raw_mem, void* user_data)
{
    ignore_unused(user_data);

    auto bytes {cast_from_void<uint8_t*>(raw_mem)};

    if (!bytes) {
        return;
    }

    constexpr safe_allocator<uint8_t> allocator {};
    allocator.deallocate(bytes.get(), 0);
}

static void ImGuiLogError(ImGuiContext* ctx, void* user_data, const char* msg) noexcept
{
    ignore_unused(user_data);

    const ImGuiWindow* window {ctx != nullptr ? ctx->CurrentWindow : nullptr};
    logging::write(logging::type::error, "ImGui error in window '{}': {}", window != nullptr ? window->Name : "(none)", msg != nullptr ? msg : "(no message)");
}

void ImGuiExt::Init()
{
    FO_TRACE_ZONE(Gui);

    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(&ImGuiAlloc, &ImGuiFree, nullptr);
    ImGui::CreateContext();
    ImGui::GetCurrentContext()->ErrorCallback = &ImGuiLogError;
}

auto ImGuiExt::LoadIniSettingsIfContext(std::string_view ini_data) -> bool
{
    FO_TRACE_ZONE(Gui);

    if (ini_data.empty() || ImGui::GetCurrentContext() == nullptr) {
        return false;
    }

    ImGui::LoadIniSettingsFromMemory(ini_data.data(), ini_data.size());
    ImGui::GetIO().WantSaveIniSettings = false;
    return true;
}
