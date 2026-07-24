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

#include "ParticleViewer.h"

#include "Application.h"
#include "ImGuiStuff.h"
#include "ParticleSprites.h"
#include "VisualParticles.h"

FO_BEGIN_NAMESPACE

// Preview surface: the same for every effect so silhouettes stay comparable
// between takes.
static constexpr isize32 PREVIEW_SIZE = {512, 512};

// Fixed screen point the effect root (its ground anchor) is pinned to. Kept
// below the middle so an effect that rises from the ground has headroom above.
static constexpr ipos32 ROOT_ANCHOR = {PREVIEW_SIZE.width / 2, PREVIEW_SIZE.height * 2 / 3};

// Faint cyan so the ground crosshair reads as a reference without competing with
// the effect drawn over it.
static constexpr ucolor ROOT_CROSSHAIR_COLOR = {100, 200, 220, 110};

// Overlay colour, drawn over the effect so it stays legible.
static constexpr ucolor DRAW_RECT_COLOR = {90, 220, 90, 200}; // full sprite draw frame

static constexpr float32_t MIN_ZOOM = 0.25f;
static constexpr float32_t MAX_ZOOM = 8.0f;
static constexpr float32_t ZOOM_WHEEL_STEP = 1.15f;

// Degrees of effect facing per pixel of held-LMB horizontal drag over the preview.
static constexpr float32_t DIR_DRAG_SENSITIVITY = 0.5f;

// Ortho depth half-range used while drawing a draw-in-scene effect, wide enough
// that a magnified effect is not clipped by the near/far planes.
static constexpr float32_t SCENE_DEPTH_HALF = 64.0f;

template<size_t Size>
static auto InputBufferView(const array<char, Size>& buffer) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    auto end = std::find(buffer.begin(), buffer.end(), char {0});
    return {buffer.data(), numeric_cast<size_t>(std::distance(buffer.begin(), end))};
}

ParticleViewer::ParticleViewer(ptr<BaseEngine> engine, ptr<SpriteManager> spr_mngr) :
    _engine {engine},
    _sprMngr {spr_mngr}
{
    FO_STACK_TRACE_ENTRY();

    LoadSettings();
}

ParticleViewer::~ParticleViewer() = default;

void ParticleViewer::LoadSettings()
{
    FO_STACK_TRACE_ENTRY();

    // The ImGui context may not exist yet (a headless host constructs the viewer without UI), so the saved
    // layout is only remembered here and applied lazily on the first Draw, when a context is guaranteed.
    _pendingImguiLayout = _settings.GetString("ImGuiLayout");

    _zoom = numeric_cast<float32_t>(_settings.GetFloat("Zoom", _zoom));
    _dirAngle = numeric_cast<float32_t>(_settings.GetFloat("DirAngle", _dirAngle));
    _seed = numeric_cast<int32_t>(_settings.GetInt("Seed", _seed));
    _looped = _settings.GetBool("Looped", _looped);
    _prewarm = _settings.GetBool("Prewarm", _prewarm);
    _drawRoot = _settings.GetBool("DrawRoot", _drawRoot);
    _drawDrawRect = _settings.GetBool("DrawDrawRect", _drawDrawRect);
    _showWireframe = _settings.GetBool("ShowWireframe", _showWireframe);

    // Re-open the last previewed effect, but only if that resource still exists, so a removed/renamed file never
    // surfaces as a selection error on startup.
    auto last_path = _settings.GetString("SelectedPath");

    if (!last_path.empty()) {
        RefreshResourceList();

        if (std::ranges::find(_resourcePaths, last_path) != _resourcePaths.end()) {
            SelectParticle(last_path);
        }
    }
}

void ParticleViewer::SaveSettings()
{
    FO_STACK_TRACE_ENTRY();

    if (auto ctx = make_nptr(ImGui::GetCurrentContext())) {
        size_t ini_size = 0;

        if (auto ini_data = make_nptr(ImGui::SaveIniSettingsToMemory(&ini_size))) {
            _settings.SetString("ImGuiLayout", string_view(ini_data.get(), ini_size));
        }
    }

    _settings.SetFloat("Zoom", _zoom);
    _settings.SetFloat("DirAngle", _dirAngle);
    _settings.SetInt("Seed", _seed);
    _settings.SetBool("Looped", _looped);
    _settings.SetBool("Prewarm", _prewarm);
    _settings.SetBool("DrawRoot", _drawRoot);
    _settings.SetBool("DrawDrawRect", _drawDrawRect);
    _settings.SetBool("ShowWireframe", _showWireframe);
    _settings.SetString("SelectedPath", _selectedPath);
}

void ParticleViewer::Draw()
{
    FO_STACK_TRACE_ENTRY();

    if (!_pendingImguiLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(_pendingImguiLayout.c_str(), _pendingImguiLayout.size());
        ImGui::GetIO().WantSaveIniSettings = false;
        _pendingImguiLayout.clear();
    }

    if (!_visible) {
        return;
    }

    ImGuiWindowFlags window_flags = 0;
    bool keep_open = true;

    if (_fillViewport) {
        auto viewport = make_ptr(ImGui::GetMainViewport());
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    }
    else {
        ImGui::SetNextWindowSize({720.0f, 620.0f}, ImGuiCond_FirstUseEver);
    }

    // A viewport-filling window has no close button, so it must not be handed a
    // visibility flag it could never turn back on.
    if (!ImGui::Begin("Particle Viewer", _fillViewport ? nullptr : &keep_open, window_flags)) {
        ImGui::End();
        return;
    }

    if (!_fillViewport) {
        _visible = keep_open;
    }

    if (ImGui::BeginChild("Particles", {260.0f, 0.0f}, ImGuiChildFlags_Borders)) {
        DrawResourceList();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("Preview", {0.0f, 0.0f}, ImGuiChildFlags_Borders)) {
        DrawPreview();
    }
    ImGui::EndChild();

    ImGui::End();
}

void ParticleViewer::RefreshResourceList()
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcesIndexed) {
        return;
    }

    _resourcesIndexed = true;

    // The particle sprite factory advertises the baked runtime extensions it can
    // load (e.g. "spk" for Spark, "efk" for Effekseer); enumerate every matching
    // resource file so the list reflects the loaded content exactly.
    auto factory = _sprMngr->GetSpriteFactory(typeid(ParticleSpriteFactory)).dyn_cast<ParticleSpriteFactory>();

    if (!factory) {
        return;
    }

    for (const string& particle_extension : factory->GetExtensions()) {
        for (const auto& particle_file : _engine->Resources.FilterFiles(particle_extension)) {
            _resourcePaths.emplace_back(particle_file.GetPath());
        }
    }

    std::ranges::sort(_resourcePaths);
    _resourcePaths.erase(std::ranges::unique(_resourcePaths).begin(), _resourcePaths.end());
}

void ParticleViewer::DrawResourceList()
{
    FO_STACK_TRACE_ENTRY();

    RefreshResourceList();

    ImGui::TextUnformatted("Particles");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##Filter", "Filter", _filterBuf.data(), _filterBuf.size());
    string filter = strex(InputBufferView(_filterBuf)).trim().lower().str();

    for (const string& path : _resourcePaths) {
        if (!filter.empty() && strex(path).lower().str().find(filter) == string::npos) {
            continue;
        }

        bool selected = path == _selectedPath;

        if (ImGui::Selectable(path.c_str(), selected)) {
            SelectParticle(path);
        }
    }
}

void ParticleViewer::DrawPreview()
{
    FO_STACK_TRACE_ENTRY();

    if (_selectedPath.empty()) {
        ImGui::TextUnformatted("Select a particle");
        return;
    }

    ImGui::TextUnformatted(_selectedPath.c_str());

    if (!_selectionError.empty()) {
        ImGui::TextUnformatted(_selectionError.c_str());
        return;
    }
    if (!_previewSprite) {
        return;
    }

    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("Zoom", &_zoom, MIN_ZOOM, MAX_ZOOM, "%.2fx");

    isize32 sprite_size = _previewSprite->GetSize();
    ImGui::SameLine();
    ImGui::Text("Frame: %d x %d px", sprite_size.width, sprite_size.height);

    DrawControls();
    DrawDebugToggles();

    RenderPreview();

    if (_renderTarget) {
        auto draw_list = make_ptr(ImGui::GetWindowDrawList());
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 end = {pos.x + numeric_cast<float32_t>(PREVIEW_SIZE.width), pos.y + numeric_cast<float32_t>(PREVIEW_SIZE.height)};

        if (GetApp()->Render.IsRenderTargetFlipped()) {
            draw_list->AddImage(_renderTarget.get(), pos, end, {0.0f, 1.0f}, {1.0f, 0.0f});
        }
        else {
            draw_list->AddImage(_renderTarget.get(), pos, end, {0.0f, 0.0f}, {1.0f, 1.0f});
        }

        // Invisible button over the image so the preview captures the mouse:
        // held-left drag turns the effect, held-right drag pans, wheel zooms.
        ImGui::InvisibleButton("PreviewArea", {numeric_cast<float32_t>(PREVIEW_SIZE.width), numeric_cast<float32_t>(PREVIEW_SIZE.height)}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        if (ImGui::IsItemActive()) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                ImVec2 drag = ImGui::GetIO().MouseDelta;
                PanBy(fpos32 {drag.x, drag.y});
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImVec2 drag = ImGui::GetIO().MouseDelta;
                _dirAngle += drag.x * DIR_DRAG_SENSITIVITY;
                _dirAngle -= std::floor(_dirAngle / 360.0f) * 360.0f;
                ApplyDirection();
            }
        }

        if (ImGui::IsItemHovered()) {
            float32_t wheel = ImGui::GetIO().MouseWheel;

            if (wheel != 0.0f) {
                _zoom = std::clamp(_zoom * (wheel > 0.0f ? ZOOM_WHEEL_STEP : 1.0f / ZOOM_WHEEL_STEP), MIN_ZOOM, MAX_ZOOM);
            }
        }
    }
}

void ParticleViewer::DrawControls()
{
    FO_STACK_TRACE_ENTRY();

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Seed", &_seed);

    ImGui::SameLine();
    if (ImGui::Button("New seed")) {
        _seed++;
        PlayCurrent();
    }

    ImGui::SameLine();
    if (ImGui::Button("Replay")) {
        PlayCurrent();
    }

    ImGui::Checkbox("Loop", &_looped);
    ImGui::SameLine();
    ImGui::Checkbox("Prewarm", &_prewarm);

    // Review facing: the look-direction a critter would pass to an attached effect. Held-LMB drag over the preview
    // turns it too.
    ImGui::SetNextItemWidth(200.0f);

    if (ImGui::SliderFloat("Direction", &_dirAngle, 0.0f, 360.0f, "%.0f deg")) {
        ApplyDirection();
    }
}

void ParticleViewer::DrawDebugToggles()
{
    FO_STACK_TRACE_ENTRY();

    ImGui::Checkbox("Root", &_drawRoot);
    ImGui::SameLine();
    ImGui::Checkbox("Draw rect", &_drawDrawRect);
    ImGui::SameLine();
    ImGui::Checkbox("Show wireframe", &_showWireframe);
}

void ParticleViewer::SelectParticle(string_view path)
{
    FO_STACK_TRACE_ENTRY();

    _selectedPath = path;
    _selectionError.clear();
    _pan = {};
    _previewSprite = nullptr;

    // The viewer browses every particle resource, including ones the runtime cannot load (e.g. a texture too large
    // for its atlas). Such a load throws deep in the engine; catch it here so a bad resource shows an error line
    // instead of unwinding through the ImGui frame and corrupting it.
    shared_ptr<Sprite> sprite;

    try {
        sprite = _sprMngr->LoadSprite(_selectedPath, AtlasType::MapSprites);
    }
    catch (const std::exception& ex) {
        _selectionError = strex("Failed to load: {}", ex.what()).str();
        WriteLog("ParticleViewer: failed to load '{}': {}", _selectedPath, ex.what());
        return;
    }

    if (!sprite) {
        _selectionError = "Particle resource failed to load";
        return;
    }

    if (!sprite.dyn_cast<ParticleSprite>()) {
        _selectionError = "Resource is not a particle sprite";
        return;
    }

    _previewSprite = std::move(sprite);
    PlayCurrent();
}

void ParticleViewer::PlayCurrent()
{
    FO_STACK_TRACE_ENTRY();

    if (!_previewSprite) {
        return;
    }

    auto particle_sprite = _previewSprite.dyn_cast<ParticleSprite>();

    if (!particle_sprite) {
        return;
    }

    particle_sprite->PlayWithSeed(_seed);

    // Apply the review facing before prewarming so the warmed-up particles are already emitted in the right
    // direction (Respawn re-applied the effect's stored setup, which carries the previous angle).
    ApplyDirection();

    if (_prewarm) {
        _previewSprite->Prewarm();
    }
}

void ParticleViewer::ApplyDirection()
{
    FO_STACK_TRACE_ENTRY();

    if (_previewSprite) {
        _previewSprite->SetDir(mdir(iround<int32_t>(_dirAngle)));
    }
}

void ParticleViewer::PanBy(fpos32 screen_delta)
{
    FO_STACK_TRACE_ENTRY();

    _pan.x += screen_delta.x;
    _pan.y += screen_delta.y;

    // Pass the inverse of the camera move to the effect as emitter motion, so world-space particles trail as if the
    // emitter (a moving critter) had travelled through them instead of the whole view sliding rigidly.
    if (auto particle_sprite = _previewSprite.dyn_cast<ParticleSprite>()) {
        float32_t px_per_world_unit = _engine->Settings->ModelProjFactor * _zoom;

        if (px_per_world_unit > 0.0f) {
            vec3 world_move = {-screen_delta.x / px_per_world_unit, screen_delta.y / px_per_world_unit, 0.0f};
            particle_sprite->GetParticle()->RebaseWorldParticles(world_move);
        }
    }
}

void ParticleViewer::RenderPreview()
{
    FO_STACK_TRACE_ENTRY();

    if (!_previewSprite) {
        return;
    }

    // Scope the wireframe overlay to the preview render only: the particle backends draw quad edges while
    // Render.DrawWireframe is set, and restoring it right after keeps a hosting mapper scene unaffected.
    bool prev_draw_wireframe = _engine->Settings->DrawWireframe;
    _engine->Settings->DrawWireframe = _showWireframe;
    auto wireframe_guard = scope_exit([&]() noexcept { _engine->Settings->DrawWireframe = prev_draw_wireframe; });

    if (!_renderTarget || _renderTargetSize != PREVIEW_SIZE) {
        // Nearest filtering: ImGui presents the target scaled by the display's
        // framebuffer (DPI) scale, and a linear target would blur the magnified
        // effect there. Nearest keeps the honest sprite pixelation crisp.
        _renderTarget = GetApp()->Render.CreateTexture(PREVIEW_SIZE, false, true);
        _renderTargetSize = PREVIEW_SIZE;
    }

    // A finite burst would otherwise freeze once emitted; restart it when looping
    // is on so the window keeps showing the live effect.
    if (_looped && !_previewSprite->IsPlaying()) {
        PlayCurrent();
    }

    // Step the particle simulation. In atlas mode this also renders the current
    // frame into the factory's atlas (drawn below); draw-in-scene effects are
    // rendered straight into the preview target instead.
    (void)_previewSprite->Update();

    bool direct = _previewSprite->IsDirectDraw();
    float32_t draw_scale = _zoom;

    auto prev_rt = GetApp()->Render.GetRenderTarget();
    GetApp()->Render.SetRenderTarget(_renderTarget);
    auto rt_guard = scope_fail([&]() noexcept { GetApp()->Render.SetRenderTarget(prev_rt); });
    GetApp()->Render.ClearRenderTarget(ucolor::clear, true);

    // Camera pan shifts the whole view (effect + crosshair + overlays) together.
    ipos32 anchor = {ROOT_ANCHOR.x + iround<int32_t>(_pan.x), ROOT_ANCHOR.y + iround<int32_t>(_pan.y)};

    // Background crosshair through the (panned) root anchor, drawn first so it
    // reads as a ground reference behind the effect.
    if (_drawRoot) {
        DrawRootCrosshair(anchor);
    }

    // Anchor by the effect root - the ground point the game pins the effect to -
    // rather than centring the bounds, so it stays put while the effect evolves.
    // The root inside a sprite is bottom-centre adjusted by its offset, exactly
    // as MapSprite::GetSpriteRootOffset computes it for the map.
    isize32 sprite_size = _previewSprite->GetSize();
    ipos32 sprite_offset = _previewSprite->GetOffset();
    ipos32 root_in_sprite = {sprite_size.width / 2 - sprite_offset.x, sprite_size.height - sprite_offset.y};
    ipos32 pos = {
        anchor.x - iround<int32_t>(numeric_cast<float32_t>(root_in_sprite.x) * draw_scale),
        anchor.y - iround<int32_t>(numeric_cast<float32_t>(root_in_sprite.y) * draw_scale),
    };

    if (direct) {
        // Clear depth again so the crosshair's 2D draw cannot occlude the effect,
        // and widen the ortho depth range so a scaled effect is not clipped by the
        // near/far planes. DrawInScene anchors the effect root to `anchor`.
        GetApp()->Render.ClearRenderTarget(std::nullopt, true);
        GetApp()->Render.SetOrthoDepthRange(-SCENE_DEPTH_HALF, SCENE_DEPTH_HALF);
        auto restore_depth = scope_exit([]() noexcept { GetApp()->Render.SetOrthoDepthRange(ORTHO_DEPTH_DEFAULT_NEAR, ORTHO_DEPTH_DEFAULT_FAR); });
        _previewSprite->DrawInScene(fpos32 {numeric_cast<float32_t>(anchor.x), numeric_cast<float32_t>(anchor.y)}, 0.0f);
    }
    else {
        isize32 scaled = {iround<int32_t>(numeric_cast<float32_t>(sprite_size.width) * draw_scale), iround<int32_t>(numeric_cast<float32_t>(sprite_size.height) * draw_scale)};
        _sprMngr->DrawSpriteSize(_previewSprite.get(), pos, scaled, true, false, Color::Neutral);
        _sprMngr->Flush();
    }

    // Diagnostic overlays go on top of the effect so they stay readable.
    DrawOverlays(pos, sprite_size, draw_scale);

    GetApp()->Render.SetRenderTarget(prev_rt);
}

void ParticleViewer::DrawRootCrosshair(ipos32 anchor)
{
    FO_STACK_TRACE_ENTRY();

    // Two full-span segments (LineList draws consecutive point pairs), crossing
    // at the anchor to mark the root.
    array<PrimitivePoint, 4> lines = {
        PrimitivePoint {.PointPos = {anchor.x, 0}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {anchor.x, PREVIEW_SIZE.height}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {0, anchor.y}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {PREVIEW_SIZE.width, anchor.y}, .PointColor = ROOT_CROSSHAIR_COLOR},
    };

    _sprMngr->DrawPoints(lines, RenderPrimitiveType::LineList);
}

void ParticleViewer::DrawOverlays(ipos32 sprite_pos, isize32 sprite_size, float32_t draw_scale)
{
    FO_STACK_TRACE_ENTRY();

    // Map a sprite-local pixel to the preview render target (same transform the
    // effect draw uses: top-left at sprite_pos, scaled by the draw scale).
    auto to_screen = [&](ipos32 sl) -> ipos32 { return {sprite_pos.x + iround<int32_t>(numeric_cast<float32_t>(sl.x) * draw_scale), sprite_pos.y + iround<int32_t>(numeric_cast<float32_t>(sl.y) * draw_scale)}; };

    vector<PrimitivePoint> lines;

    auto add_segment = [&](ipos32 a, ipos32 b, ucolor color) {
        lines.emplace_back(PrimitivePoint {.PointPos = a, .PointColor = color});
        lines.emplace_back(PrimitivePoint {.PointPos = b, .PointColor = color});
    };
    auto add_rect = [&](irect32 sprite_rect, ucolor color) {
        ipos32 tl = to_screen({sprite_rect.x, sprite_rect.y});
        ipos32 tr = to_screen({sprite_rect.x + sprite_rect.width, sprite_rect.y});
        ipos32 br = to_screen({sprite_rect.x + sprite_rect.width, sprite_rect.y + sprite_rect.height});
        ipos32 bl = to_screen({sprite_rect.x, sprite_rect.y + sprite_rect.height});
        add_segment(tl, tr, color);
        add_segment(tr, br, color);
        add_segment(br, bl, color);
        add_segment(bl, tl, color);
    };

    // The draw rect is the whole sprite frame the effect rasterizes into - the
    // maximal area it can cover.
    if (_drawDrawRect) {
        add_rect({0, 0, sprite_size.width, sprite_size.height}, DRAW_RECT_COLOR);
    }

    if (!lines.empty()) {
        _sprMngr->DrawPoints(lines, RenderPrimitiveType::LineList);
    }
}

FO_END_NAMESPACE
