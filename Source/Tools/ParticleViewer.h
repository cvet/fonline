// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "EngineBase.h"
#include "ResourceManager.h"
#include "SettingsStorage.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

// Particle effect preview, view only — authoring stays in the mapper's particle editor. Loads through the
// same factory path the game uses and owns no engine services; a host passes its own and calls Draw()
class ParticleViewer final
{
public:
    ParticleViewer() = delete;
    ParticleViewer(ptr<BaseEngine> engine, ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr);
    ParticleViewer(const ParticleViewer&) = delete;
    ParticleViewer(ParticleViewer&&) noexcept = delete;
    auto operator=(const ParticleViewer&) = delete;
    auto operator=(ParticleViewer&&) noexcept = delete;
    ~ParticleViewer();

    [[nodiscard]] auto IsVisible() const noexcept -> bool { return _visible; }

    void SetVisible(bool visible) noexcept { _visible = visible; }

    // Standalone hosts have nothing else on screen, so the window takes the
    // whole viewport and drops its title bar and move/resize handles
    void SetFillViewport(bool fill) noexcept { _fillViewport = fill; }
    void Draw();

    // Persists the ImGui layout and the view options (zoom, seed, loop/prewarm, overlays, last effect) to the
    // per-user settings store. Loaded in the constructor (the ImGui layout applies lazily on the first Draw); a host calls SaveSettings() before teardown
    void SaveSettings();

private:
    void LoadSettings();
    void RefreshResourceList();
    void DrawResourceList();
    void DrawPreview();
    void DrawControls();
    void DrawDebugToggles();
    void SelectParticle(string_view path);
    void PlayCurrent();
    void ApplyDirection();
    void PanBy(fpos32 screen_delta);
    void RenderPreview();
    void DrawRootCrosshair(ipos32 anchor);
    void DrawOverlays(ipos32 sprite_pos, isize32 sprite_size, float32_t draw_scale);

    ptr<BaseEngine> _engine;
    ptr<SpriteManager> _sprMngr;
    ptr<ResourceManager> _resMngr;
    SettingsStorage _settings {"ParticleViewer"};
    string _pendingImguiLayout {};

    bool _visible {};
    bool _fillViewport {};
    array<char, 128> _filterBuf {};
    vector<string> _resourcePaths {};
    bool _resourcesIndexed {};
    string _selectedPath {};
    string _selectionError {};
    shared_ptr<Sprite> _previewSprite {};
    unique_nptr<RenderTexture> _renderTarget {};
    isize32 _renderTargetSize {};
    float32_t _zoom {1.0f};
    float32_t _appliedScale {}; // effect scale currently pushed into the particle system (viewer zoom)
    fpos32 _pan {}; // camera pan offset (screen px), held-RMB drag
    float32_t _dirAngle {};
    int32_t _seed {};
    bool _looped {true}; // restart the effect when a finite burst ends
    bool _prewarm {true}; // warm the system on play so it opens mid-effect

    // Diagnostic overlays, all opt-in (off by default)
    bool _drawRoot {};
    bool _drawDrawRect {};
    bool _showWireframe {};
};

FO_END_NAMESPACE
