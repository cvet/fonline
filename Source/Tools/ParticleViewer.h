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

#include "EngineBase.h"
#include "SettingsStore.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

///
/// Particle effect preview window (view only).
///
/// Lists every particle resource the loaded content provides (the baked `.spk`
/// Spark and `.efk` Effekseer files the sprite factory advertises), loads the
/// selected one as a `Sprite` through the same `ParticleSpriteFactory` path the
/// game uses, and plays it live in an offscreen preview at in-game scale. Opt-in
/// overlays mark the review geometry: the **root** crosshair (the ground point
/// the effect is anchored to) and the **draw rect** (the sprite frame the
/// particle rasterizes into). Playback runs on a per-system seed so a one-shot
/// burst can be replayed deterministically or reseeded.
///
/// Authoring stays in the mapper's particle editor; this window never edits.
///
/// The window owns no engine services; a host (standalone app) passes the ones
/// it already has and calls `Draw()` from its ImGui pass.
///
class ParticleViewer final
{
public:
    ParticleViewer() = delete;
    ParticleViewer(ptr<BaseEngine> engine, ptr<SpriteManager> spr_mngr);
    ParticleViewer(const ParticleViewer&) = delete;
    ParticleViewer(ParticleViewer&&) noexcept = delete;
    auto operator=(const ParticleViewer&) = delete;
    auto operator=(ParticleViewer&&) noexcept = delete;
    ~ParticleViewer();

    [[nodiscard]] auto IsVisible() const noexcept -> bool { return _visible; }

    void SetVisible(bool visible) noexcept { _visible = visible; }

    // Standalone hosts have nothing else on screen, so the window takes the
    // whole viewport and drops its title bar and move/resize handles.
    void SetFillViewport(bool fill) noexcept { _fillViewport = fill; }
    void Draw();

    // Persists the ImGui layout and the view options (zoom, seed, loop/prewarm, overlays, last effect) to the
    // per-user settings store. Loaded in the constructor; a standalone host calls SaveSettings() before teardown.
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
    SettingsStore _settings {"ParticleViewer"};

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
    fpos32 _pan {};  // camera pan offset (screen px), held-RMB drag
    float32_t _dirAngle {};
    int32_t _seed {};
    bool _looped {true};    // restart the effect when a finite burst ends
    bool _prewarm {true};   // warm the system on play so it opens mid-effect

    // Diagnostic overlays, all opt-in (off by default).
    bool _drawRoot {};
    bool _drawDrawRect {};
};

FO_END_NAMESPACE
