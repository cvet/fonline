// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

class MapperEngine;
class MapView;

class ParticleSubEditor
{
public:
    ParticleSubEditor() = default;
    ParticleSubEditor(const ParticleSubEditor&) = delete;
    ParticleSubEditor(ParticleSubEditor&&) noexcept = delete;
    auto operator=(const ParticleSubEditor&) -> ParticleSubEditor& = delete;
    auto operator=(ParticleSubEditor&&) noexcept -> ParticleSubEditor& = delete;
    virtual ~ParticleSubEditor() = default;

    virtual void Initialize() { }
    virtual void Shutdown() { }
    virtual void ResetLayout() { }
    virtual void OnFocusGained() { }
    virtual void OnCurrentMapChanging(nptr<MapView> next_map) { ignore_unused(next_map); }
    virtual void OnMapUnloading(ptr<MapView> map) { ignore_unused(map); }
    virtual void DrawMenuItem() = 0;
    virtual void DrawWindows() = 0;
};

class ParticleEditorManager final
{
public:
    explicit ParticleEditorManager(ptr<MapperEngine> mapper);
    ParticleEditorManager(const ParticleEditorManager&) = delete;
    ParticleEditorManager(ParticleEditorManager&&) noexcept = delete;
    auto operator=(const ParticleEditorManager&) -> ParticleEditorManager& = delete;
    auto operator=(ParticleEditorManager&&) noexcept -> ParticleEditorManager& = delete;
    ~ParticleEditorManager();

    void Initialize();
    void Shutdown();
    void ResetLayout();
    void OnFocusGained();
    void OnCurrentMapChanging(nptr<MapView> next_map);
    void OnMapUnloading(ptr<MapView> map);
    void DrawMenuItems();
    void DrawWindows();

private:
    vector<unique_ptr<ParticleSubEditor>> _subEditors {};
};

FO_END_NAMESPACE
