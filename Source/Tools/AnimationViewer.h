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
#include "ResourceManager.h"
#include "SettingsStorage.h"
#include "SpriteManager.h"

FO_BEGIN_NAMESPACE

#if FO_ENABLE_3D
struct ModelBone;
#endif

///
/// Critter animation preview window.
///
/// Lists every critter prototype the loaded content provides, renders the
/// selected one at its real in-game size, and offers the animations that
/// critter actually has — one click plays a clip. Both critter families are
/// handled through the same `Sprite` surface: 3D (`.fo3d`) models expose their
/// authored `(state, action)` table, while 2D critters are probed against the
/// sprite-sheet resources. Playback returns to the idle animation when a
/// one-shot clip ends, so the window always shows a live, representative pose.
///
/// The window owns no engine services; a host (mapper, editor) passes the ones
/// it already has and calls `Draw()` from its ImGui pass.
///
class AnimationViewer final
{
public:
    AnimationViewer() = delete;
    AnimationViewer(ptr<BaseEngine> engine, ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr, ptr<GameTimer> game_time);
    AnimationViewer(const AnimationViewer&) = delete;
    AnimationViewer(AnimationViewer&&) noexcept = delete;
    auto operator=(const AnimationViewer&) = delete;
    auto operator=(AnimationViewer&&) noexcept = delete;
    ~AnimationViewer();

    [[nodiscard]] auto IsVisible() const noexcept -> bool { return _visible; }

    void SetVisible(bool visible) noexcept { _visible = visible; }

    // Standalone hosts have nothing else on screen, so the window takes the
    // whole viewport and drops its title bar and move/resize handles.
    void SetFillViewport(bool fill) noexcept { _fillViewport = fill; }
    void Draw();

    // Persists the ImGui layout and the view options (zoom, facing, overlays, last critter) to the per-user
    // settings store. Loaded in the constructor; a standalone host calls SaveSettings() before teardown.
    void SaveSettings();

private:
    struct AnimationEntry
    {
        CritterStateAnim StateAnim {};
        CritterActionAnim ActionAnim {};
        string Label {};
    };

    void DrawCritterList();
    void DrawPreview();
    void DrawDebugToggles();
    void DrawAnimationList();
    void DrawHierarchy();
#if FO_ENABLE_3D
    void DrawHierarchyNode(ptr<const ModelBone> bone);
#endif

    void LoadSettings();
    void SelectCritter(hstring proto_id);
    void CollectModelLayers(ptr<const ProtoCritter> proto);
    void ApplyDir();
    void CollectAnimations();
    void PlayAnimation(const AnimationEntry& entry, bool looped, bool instant = false);
    void PlayIdle(bool instant = false);
    void PrewarmModel();
    void RenderPreview();
    void DrawRootCrosshair(ipos32 anchor);
    void DrawOverlays(ipos32 sprite_pos, isize32 sprite_size, float32_t draw_scale);

    auto MakeAnimationLabel(CritterStateAnim state_anim, CritterActionAnim action_anim) const -> string;
    [[nodiscard]] static auto BoneColor(hstring bone_name) -> ucolor;

    ptr<BaseEngine> _engine;
    ptr<SpriteManager> _sprMngr;
    ptr<ResourceManager> _resMngr;
    ptr<GameTimer> _gameTime;
    SettingsStorage _settings {"AnimationViewer"};

    bool _visible {};
    bool _fillViewport {};
    array<char, 128> _filterBuf {};
    hstring _selectedProtoId {};
    string _selectedModelName {};
    string _selectionError {};
    shared_ptr<Sprite> _previewSprite {};
    unique_nptr<RenderTexture> _renderTarget {};
    isize32 _renderTargetSize {};
    vector<AnimationEntry> _animations {};
    vector<int32_t> _modelLayers {};
    int32_t _protoNameOffset {};  // per-critter NameOffset from the selected proto
    int32_t _playingIndex {-1};
    bool _looped {true};
    // Facing is an angle (degrees), the same currency the engine uses: a sprite
    // takes its hex direction from the angle and a model rotates to it. Default
    // 210° is hex direction 3 (dir*60+30), the game's front-facing review angle.
    // Held-LMB drag over the preview turns it left/right.
    float32_t _dirAngle {210.0f};
    float32_t _zoom {1.0f};
    fpos32 _pan {};  // camera pan offset (screen px), held-RMB drag

    // Direct draw renders a 3D model straight into the scene (real geometry +
    // depth) instead of through the cached atlas sprite. Off by default.
    bool _directDraw {};

    // Direct draw scales the model to the zoom for a crisp real-geometry render;
    // sprite mode keeps it at native scale (scale 1). Tracked to avoid redundant
    // relayouts when the effective scale is unchanged.
    float32_t _appliedModelScale {};

    // Debug overlays, all opt-in (off by default) for verifying a model's
    // authored anchor geometry.
    bool _drawRoot {};
    bool _drawNameLevel {};
    bool _drawRenderRect {};
    bool _drawViewRect {};
    unordered_set<hstring> _enabledBones {};  // bones whose position marker is shown
};

FO_END_NAMESPACE
