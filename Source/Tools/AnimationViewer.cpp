// FOnline Engine
// SPDX-License-Identifier: MIT

#include "AnimationViewer.h"

#include "Application.h"
#include "DefaultSprites.h"
#include "ImGuiStuff.h"

#if FO_ENABLE_3D
#include "ModelHierarchy.h"
#include "ModelInformation.h"
#include "ModelInstance.h"
#include "ModelSprites.h"
#endif

FO_BEGIN_NAMESPACE

// Preview surface: large enough for oversized creatures at in-game scale, and
// the same for every critter so silhouettes stay comparable between takes
static constexpr isize32 PREVIEW_SIZE {512, 512};

// Fixed screen point the critter root (hex center) is pinned to. Kept below the
// middle so the body, which rises from the ground point, has headroom above
static constexpr ipos32 ROOT_ANCHOR {PREVIEW_SIZE.width / 2, PREVIEW_SIZE.height * 2 / 3};

// Faint cyan so the ground crosshair reads as a reference without competing
// with the model drawn over it
static constexpr ucolor ROOT_CROSSHAIR_COLOR {100, 200, 220, 110};

// Overlay colours, drawn over the model so they stay legible
static constexpr ucolor NAME_POINT_COLOR {255, 220, 40, 230}; // where the name would sit
static constexpr ucolor RENDER_RECT_COLOR {90, 220, 90, 200}; // full sprite frame
static constexpr ucolor VIEW_RECT_COLOR {235, 90, 220, 200}; // visual (view) rect
static constexpr ucolor PARTICLE_ATTACH_COLOR {80, 220, 235, 230}; // where an attached particle effect emits from
static constexpr ucolor MODEL_ATTACH_COLOR {240, 175, 70, 230}; // where a bone-attached child model hangs
static constexpr int32_t OVERLAY_CROSS_HALF {5}; // marker cross arm length, screen pixels
static constexpr ucolor AXIS_X_COLOR {235, 70, 70, 235}; // joint local X
static constexpr ucolor AXIS_Y_COLOR {80, 215, 90, 235}; // joint local Y: the direction an attached effect emits along
static constexpr ucolor AXIS_Z_COLOR {80, 130, 255, 235}; // joint local Z
static constexpr float32_t ARROW_HEAD_LENGTH {7.0f}; // screen pixels
static constexpr float32_t ARROW_HEAD_HALF_WIDTH {3.5f}; // screen pixels

// Distinct bright colours cycled by bone-name hash so a bone's tree entry and
// its viewport marker share the same colour
static constexpr array<ucolor, 10> BONE_PALETTE {
    ucolor {239, 83, 80},
    ucolor {255, 167, 38},
    ucolor {255, 238, 88},
    ucolor {156, 204, 101},
    ucolor {38, 198, 218},
    ucolor {66, 165, 245},
    ucolor {126, 87, 194},
    ucolor {236, 64, 122},
    ucolor {141, 110, 99},
    ucolor {189, 189, 189},
};

// 2D critters have no authored clip table, so the viewer probes the sprite resources across the enum
// ranges; the bound stays generous for project-defined actions while keeping the probe cheap
static constexpr int32_t PROBE_STATE_ANIM_MAX {32};
static constexpr int32_t PROBE_ACTION_ANIM_MAX {700};
static constexpr float32_t MIN_ZOOM {0.25f};
static constexpr float32_t MAX_ZOOM {8.0f};
static constexpr float32_t ZOOM_WHEEL_STEP {1.15f};

// Held-LMB drag over the preview turns the critter: degrees of facing per pixel
// of horizontal drag (about one full turn per screen width)
static constexpr float32_t DRAG_DEG_PER_PX {0.7f};

// Wrap a facing angle into [0, 360)
static auto NormalizeAngle(float32_t angle) -> float32_t
{
    return angle - std::floor(angle / 360.0f) * 360.0f;
}

template<size_t Size>
static auto InputBufferView(const array<char, Size>& buffer) -> string_view
{
    auto end {std::find(buffer.begin(), buffer.end(), char {0})};
    return {buffer.data(), numeric_cast<size_t>(std::distance(buffer.begin(), end))};
}

AnimationViewer::AnimationViewer(ptr<BaseEngine> engine, ptr<SpriteManager> spr_mngr, ptr<ResourceManager> res_mngr, ptr<CritterFramesCache> critter_frames, ptr<GameTimer> game_time) :
    _engine {engine},
    _sprMngr {spr_mngr},
    _resMngr {res_mngr},
    _critterFrames {critter_frames},
    _gameTime {game_time}
{
    LoadSettings();
}

AnimationViewer::~AnimationViewer() = default;

void AnimationViewer::LoadSettings()
{
    FO_TRACE_ZONE(Editor);

    _pendingImguiLayout = _settings.GetString("ImGuiLayout");
    _zoom = numeric_cast<float32_t>(_settings.GetFloat("Zoom", _zoom));
    _dirAngle = numeric_cast<float32_t>(_settings.GetFloat("DirAngle", _dirAngle));
    _directDraw = _settings.GetBool("DirectDraw", _directDraw);
    _looped = _settings.GetBool("Looped", _looped);
    _drawRoot = _settings.GetBool("DrawRoot", _drawRoot);
    _drawNameLevel = _settings.GetBool("DrawNameLevel", _drawNameLevel);
    _drawRenderRect = _settings.GetBool("DrawRenderRect", _drawRenderRect);
    _drawViewRect = _settings.GetBool("DrawViewRect", _drawViewRect);

    // Re-open the last previewed critter, but only if that prototype still exists (content may have changed since
    // the last run), so a stale id never surfaces as a selection error on startup
    string last_proto {_settings.GetString("SelectedProto")};

    if (!last_proto.empty()) {
        for (const auto& [proto_id, proto] : _engine->GetProtoCritters()) {
            if (proto_id.as_str() == last_proto) {
                SelectCritter(proto_id);
                break;
            }
        }
    }
}

void AnimationViewer::SaveSettings()
{
    FO_TRACE_ZONE(Editor);

    if (auto ctx {make_nptr(ImGui::GetCurrentContext())}) {
        size_t ini_size {0};

        if (auto ini_data {make_nptr(ImGui::SaveIniSettingsToMemory(&ini_size))}) {
            _settings.SetString("ImGuiLayout", string_view(ini_data.get(), ini_size));
        }
    }

    _settings.SetFloat("Zoom", _zoom);
    _settings.SetFloat("DirAngle", _dirAngle);
    _settings.SetBool("DirectDraw", _directDraw);
    _settings.SetBool("Looped", _looped);
    _settings.SetBool("DrawRoot", _drawRoot);
    _settings.SetBool("DrawNameLevel", _drawNameLevel);
    _settings.SetBool("DrawRenderRect", _drawRenderRect);
    _settings.SetBool("DrawViewRect", _drawViewRect);
    _settings.SetString("SelectedProto", _selectedProtoId.as_str());
}

void AnimationViewer::Draw()
{
    FO_TRACE_ZONE(Editor);

    if (!_pendingImguiLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(_pendingImguiLayout.c_str(), _pendingImguiLayout.size());
        ImGui::GetIO().WantSaveIniSettings = false;
        _pendingImguiLayout.clear();
    }

    if (!_visible) {
        return;
    }

    ImGuiWindowFlags window_flags {0};
    bool keep_open {true};

    if (_fillViewport) {
        auto viewport {make_ptr(ImGui::GetMainViewport())};
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    }
    else {
        ImGui::SetNextWindowSize({860.0f, 620.0f}, ImGuiCond_FirstUseEver);
    }

    // A viewport-filling window has no close button, so it must not be handed a
    // visibility flag it could never turn back on
    if (!ImGui::Begin("Animation Viewer", _fillViewport ? nullptr : &keep_open, window_flags)) {
        ImGui::End();
        return;
    }

    if (!_fillViewport) {
        _visible = keep_open;
    }

    if (ImGui::BeginChild("Critters", {240.0f, 0.0f}, ImGuiChildFlags_Borders)) {
        DrawCritterList();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("Preview", {numeric_cast<float32_t>(PREVIEW_SIZE.width) + 16.0f, 0.0f}, ImGuiChildFlags_Borders)) {
        DrawPreview();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right column split: animation list on top, model hierarchy below it
    if (ImGui::BeginChild("RightColumn", {0.0f, 0.0f}, ImGuiChildFlags_None)) {
        float32_t column_height {ImGui::GetContentRegionAvail().y};

        if (ImGui::BeginChild("Animations", {0.0f, column_height * 0.5f}, ImGuiChildFlags_Borders)) {
            DrawAnimationList();
        }

        ImGui::EndChild();

        if (ImGui::BeginChild("Hierarchy", {0.0f, 0.0f}, ImGuiChildFlags_Borders)) {
            DrawHierarchy();
        }

        ImGui::EndChild();
    }

    ImGui::EndChild();

    ImGui::End();
}

void AnimationViewer::DrawCritterList()
{
    FO_TRACE_ZONE(Editor);

    ImGui::TextUnformatted("Critters");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##Filter", "Filter", _filterBuf.data(), _filterBuf.size());
    string filter {strex(InputBufferView(_filterBuf)).trim().lower().str()};

    for (const auto& [proto_id, proto] : _engine->GetProtoCritters()) {
        string proto_name {string(proto_id)};

        if (!filter.empty() && strex(proto_name).lower().str().find(filter) == string::npos) {
            continue;
        }

        bool selected {proto_id == _selectedProtoId};

        if (ImGui::Selectable(proto_name.c_str(), selected)) {
            SelectCritter(proto_id);
        }
    }
}

void AnimationViewer::DrawPreview()
{
    FO_TRACE_ZONE(Editor);

    if (!_selectedProtoId) {
        ImGui::TextUnformatted("Select a critter");
        return;
    }

    ImGui::TextUnformatted(_selectedModelName.c_str());

    if (!_selectionError.empty()) {
        ImGui::TextUnformatted(_selectionError.c_str());
        return;
    }
    if (!_previewSprite) {
        return;
    }

    // Facing is an angle; a 2D sprite snaps it to the nearest hex frame, a model
    // turns to it. Drag the preview with the left button to rotate, too
    ImGui::SetNextItemWidth(110.0f);

    if (ImGui::SliderFloat("Angle", &_dirAngle, 0.0f, 360.0f, "%.0f deg")) {
        _dirAngle = NormalizeAngle(_dirAngle);
        ApplyDir();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Zoom", &_zoom, MIN_ZOOM, MAX_ZOOM, "%.2fx");

    isize32 sprite_size {_previewSprite->GetSize()};
    ImGui::Text("Sprite: %d x %d px", sprite_size.width, sprite_size.height);

    DrawDebugToggles();

    RenderPreview();

    if (_renderTarget) {
        auto draw_list {make_ptr(ImGui::GetWindowDrawList())};
        ImVec2 pos {ImGui::GetCursorScreenPos()};
        ImVec2 end {pos.x + numeric_cast<float32_t>(PREVIEW_SIZE.width), pos.y + numeric_cast<float32_t>(PREVIEW_SIZE.height)};

        if (GetApp()->Render.IsRenderTargetFlipped()) {
            draw_list->AddImage(_renderTarget.get(), pos, end, {0.0f, 1.0f}, {1.0f, 0.0f});
        }
        else {
            draw_list->AddImage(_renderTarget.get(), pos, end, {0.0f, 0.0f}, {1.0f, 1.0f});
        }

        // Invisible button over the image so the preview captures the mouse:
        // held-left drag turns the critter, held-right drag pans, wheel zooms
        ImGui::InvisibleButton("PreviewArea", {numeric_cast<float32_t>(PREVIEW_SIZE.width), numeric_cast<float32_t>(PREVIEW_SIZE.height)}, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        if (ImGui::IsItemActive()) {
            ImVec2 drag {ImGui::GetIO().MouseDelta};

            // Held-LMB horizontal drag rotates the facing left/right
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && drag.x != 0.0f) {
                _dirAngle = NormalizeAngle(_dirAngle - drag.x * DRAG_DEG_PER_PX);
                ApplyDir();
            }

            // Held-RMB drag pans the view (moves the camera anchor)
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                _pan.x += drag.x;
                _pan.y += drag.y;
            }
        }

        // Wheel over the preview zooms, which is what the hand reaches for
        // first when judging a model's size
        if (ImGui::IsItemHovered()) {
            float32_t wheel {ImGui::GetIO().MouseWheel};

            if (wheel != 0.0f) {
                _zoom = std::clamp(_zoom * (wheel > 0.0f ? ZOOM_WHEEL_STEP : 1.0f / ZOOM_WHEEL_STEP), MIN_ZOOM, MAX_ZOOM);
            }
        }
    }
}

void AnimationViewer::DrawDebugToggles()
{
    ImGui::Checkbox("Direct draw", &_directDraw);
    ImGui::SameLine();
    ImGui::Checkbox("Root", &_drawRoot);
    ImGui::SameLine();
    ImGui::Checkbox("Name level", &_drawNameLevel);
    ImGui::Checkbox("Draw rect", &_drawRenderRect);
    ImGui::SameLine();
    ImGui::Checkbox("View rect", &_drawViewRect);
}

void AnimationViewer::DrawAnimationList()
{
    FO_TRACE_ZONE(Editor);

    ImGui::TextUnformatted("Animations");
    ImGui::Separator();

    if (!_previewSprite) {
        ImGui::TextUnformatted("No critter selected");
        return;
    }

    ImGui::Checkbox("Loop", &_looped);
    ImGui::SameLine();

    if (ImGui::Button("Idle")) {
        PlayIdle();
    }

    ImGui::Separator();

    if (_animations.empty()) {
        ImGui::TextUnformatted("No animations found");
        return;
    }

    for (size_t i {0}; i < _animations.size(); i++) {
        const auto& entry {_animations[i]};
        bool playing {numeric_cast<int32_t>(i) == _playingIndex};

        if (ImGui::Selectable(entry.Label.c_str(), playing)) {
            _playingIndex = numeric_cast<int32_t>(i);
            PlayAnimation(entry, _looped);
        }
    }
}

void AnimationViewer::SelectCritter(hstring proto_id)
{
    FO_TRACE_ZONE(Editor);

    _selectedProtoId = proto_id;
    _selectionError.clear();
    _animations.clear();
    _enabledBones.clear();
    _enabledAttachments.clear();
    _appliedModelScale = 0.0f;
    _pan = {};
    _protoNameOffset = 0;
    _playingIndex = -1;
    _previewSprite = nullptr;

    auto proto {_engine->GetProtoCritter(proto_id)};

    if (!proto) {
        _selectionError = "Critter prototype is not available";
        return;
    }

    // Per-critter name offset (engine property, present on the proto) — combined
    // with the global setting to place the name-level marker like the game does
    _protoNameOffset = proto->GetNameOffset();

    hstring model_name {proto->GetModelName()};
    _selectedModelName = string(model_name);

    if (!model_name) {
        _selectionError = "Prototype declares no model";
        return;
    }

    _previewSprite = _resMngr->LoadSprite(model_name, AtlasType::MapSprites, true);

    if (!_previewSprite) {
        _selectionError = "Model resource could not be loaded";
        return;
    }

    CollectModelLayers(proto);
    ApplyDir();

    CollectAnimations();
    PrewarmModel();
}

void AnimationViewer::ApplyDir()
{
    if (!_previewSprite) {
        return;
    }

    mdir dir {iround<int32_t>(_dirAngle)};
    _previewSprite->SetDirection(dir);

#if FO_ENABLE_3D
    // Sprite::SetDirection starts a smooth turn that converges only while frames advance, but a review window
    // wants the pose at once, so the directions are snapped and a redraw forced
    if (auto model_spr {_previewSprite.dyn_cast<ModelSprite>()}) {
        auto model {model_spr->GetModel()};
        model->SetLookDir(dir);
        model->SetMoveDir(dir, false);
        model->RequestRedraw();
    }
#endif
}

void AnimationViewer::CollectModelLayers(ptr<const ProtoCritter> proto)
{
    _modelLayers.clear();

    // Which critter property feeds which model layer is game-specific, so `Render.ModelLayerProperties`
    // declares the mapping and a prototype is dressed here without game knowledge in the engine
    const auto& mapping {_engine->Settings->Render.ModelLayerProperties};

    if (mapping.empty()) {
        return;
    }

    auto registrar {proto->GetProperties()->GetRegistrar()};

    for (const auto& pair_text : mapping) {
        auto sep {pair_text.find('=')};

        if (sep == string::npos) {
            logging::write("Animation viewer: bad Render.ModelLayerProperties entry, expected <PropertyName>=<LayerIndex>: {}", pair_text);
            continue;
        }

        string prop_name {string(strvex(string_view(pair_text).substr(0, sep)).trim())};
        string index_text {string(strvex(string_view(pair_text).substr(sep + 1)).trim())};
        auto prop {registrar->FindProperty(prop_name)};

        if (!prop || prop->IsDisabled()) {
            continue;
        }

        if (!strex(index_text).is_number()) {
            logging::write("Animation viewer: bad layer index in Render.ModelLayerProperties: {}", pair_text);
            continue;
        }

        int32_t layer_index {strex(index_text).to_int32()};

        if (layer_index < 0) {
            continue;
        }

        if (numeric_cast<size_t>(layer_index) >= _modelLayers.size()) {
            _modelLayers.resize(numeric_cast<size_t>(layer_index) + 1);
        }

        _modelLayers[numeric_cast<size_t>(layer_index)] = proto->GetValueAsInt(prop.get());
    }
}

void AnimationViewer::CollectAnimations()
{
    FO_TRACE_ZONE(Editor);

    _animations.clear();

    if (!_previewSprite) {
        return;
    }

#if FO_ENABLE_3D
    // 3D: the model description carries the authored clip table, so the list is
    // exactly what the critter has - no probing, no false entries
    if (auto model_spr {_previewSprite.dyn_cast<ModelSprite>()}) {
        auto model {model_spr->GetModel()};

        for (const auto& [state_anim, action_anim] : model->GetInformation()->GetAvailableAnimations()) {
            _animations.emplace_back(AnimationEntry {.StateAnim = state_anim, .ActionAnim = action_anim, .Label = MakeAnimationLabel(state_anim, action_anim)});
        }

        return;
    }
#endif

    // 2D: sprite-sheet critters have no clip table; a frame set that resolves
    // for a given pair is the evidence that the animation exists
    for (int32_t state_anim {1}; state_anim <= PROBE_STATE_ANIM_MAX; state_anim++) {
        for (int32_t action_anim {1}; action_anim <= PROBE_ACTION_ANIM_MAX; action_anim++) {
            auto state_value {static_cast<CritterStateAnim>(state_anim)};
            auto action_value {static_cast<CritterActionAnim>(action_anim)};

            if (_critterFrames->GetCritterAnimFrames(_selectedProtoId ? _engine->GetProtoCritter(_selectedProtoId)->GetModelName() : hstring(), state_value, action_value, mdir(iround<int32_t>(_dirAngle)))) {
                _animations.emplace_back(AnimationEntry {.StateAnim = state_value, .ActionAnim = action_value, .Label = MakeAnimationLabel(state_value, action_value)});
            }
        }
    }
}

void AnimationViewer::PlayAnimation(const AnimationEntry& entry, bool looped, bool instant)
{
    if (!_previewSprite) {
        return;
    }

#if FO_ENABLE_3D
    if (auto model_spr {_previewSprite.dyn_cast<ModelSprite>()}) {
        auto model {model_spr->GetModel()};

        auto flags {looped ? ModelAnimFlags::None : combine_enum(ModelAnimFlags::PlayOnce, ModelAnimFlags::NoRotate)};

        // Instant playback drops the cross-fade, which is what lets the prewarm open on the idle pose
        // instead of a blend from the bind pose
        if (instant) {
            flags = combine_enum(flags, ModelAnimFlags::NoSmooth);
        }

        auto layers {_modelLayers.empty() ? nptr<const int32_t> {} : nptr<const int32_t> {_modelLayers.data()}};
        (void)model->PlayAnim(entry.StateAnim, entry.ActionAnim, layers, 0.0f, flags);
        model->StartMeshGeneration();
        return;
    }

#else
    ignore_unused(looped, instant);
#endif

    auto frames {_critterFrames->GetCritterAnimFrames(_engine->Hashes.to_hashed_string(_selectedModelName), entry.StateAnim, entry.ActionAnim, mdir(iround<int32_t>(_dirAngle)))};

    if (frames) {
        _previewSprite = frames->MakeCopy();
        _previewSprite->PlayDefault();
    }
}

void AnimationViewer::PlayIdle(bool instant)
{
    _playingIndex = -1;

    if (!_previewSprite) {
        return;
    }

    // The project chooses the resting preview pair shown between clips
    auto idle_entry {AnimationEntry {.StateAnim = _engine->Settings->Animation.PreviewStateAnim, .ActionAnim = _engine->Settings->Animation.PreviewActionAnim, .Label = {}}};
    PlayAnimation(idle_entry, true, instant);
}

void AnimationViewer::PrewarmModel()
{
    if (!_previewSprite) {
        return;
    }

    // Settle the preview and warm the particle systems, so the first shown frame is the configured pose
    // with its effects already emitting
    PlayIdle(true);
    _previewSprite->Prewarm();
}

void AnimationViewer::RenderPreview()
{
    FO_TRACE_ZONE(Editor);

    if (!_previewSprite) {
        return;
    }

    if (!_renderTarget || _renderTargetSize != PREVIEW_SIZE) {
        // Nearest filtering, because ImGui presents the target scaled by the display's DPI scale and a
        // linear target would blur the magnified sprite
        _renderTarget = GetApp()->Render.CreateTexture(PREVIEW_SIZE, false, true);
        _renderTargetSize = PREVIEW_SIZE;
    }

    // A finished one-shot clip would otherwise leave the critter frozen on its
    // last frame; fall back to idle so the window keeps showing a live pose
    if (_playingIndex >= 0 && !_looped && !_previewSprite->IsPlaying()) {
        PlayIdle();
    }

    // Sprite mode magnifies the baked atlas frame, so zoom shows the same pixelation the game shows;
    // direct draw renders real geometry at the zoom and is possible only for 3D models
    float32_t draw_scale {_zoom};
    bool direct {false};

#if FO_ENABLE_3D
    auto model_spr {_previewSprite.dyn_cast<ModelSprite>()};

    if (model_spr) {
        direct = _directDraw;

        // Direct: scale the model to the zoom and draw it 1:1. Sprite: keep it at
        // native scale and let the baked frame be magnified (`draw_scale`)
        float32_t model_scale {direct ? _zoom : 1.0f};

        if (model_scale != _appliedModelScale) {
            model_spr->GetModel()->SetScale(model_scale, model_scale, model_scale);
            _appliedModelScale = model_scale;
        }

        draw_scale = _zoom / model_scale;
    }
#endif

    // Direct mode draws the model itself below; atlas mode renders it to the
    // atlas now (skipped for direct so the global atlas path is not triggered)
    if (direct) {
#if FO_ENABLE_3D
        model_spr->GetModel()->PrepareFrameLayout();
#endif
    }
    else {
        (void)_previewSprite->Update();
    }

    auto prev_rt {GetApp()->Render.GetRenderTarget()};
    GetApp()->Render.SetRenderTarget(_renderTarget);
    auto rt_guard {scope_fail([&]() noexcept { GetApp()->Render.SetRenderTarget(prev_rt); })};
    GetApp()->Render.ClearRenderTarget(ucolor::clear, true);

    // Camera pan shifts the whole view (model + crosshair + overlays) together
    ipos32 anchor {ROOT_ANCHOR.x + iround<int32_t>(_pan.x), ROOT_ANCHOR.y + iround<int32_t>(_pan.y)};

    // Background crosshair through the (panned) root anchor, drawn first so it
    // reads as a ground reference behind the model
    if (_drawRoot) {
        DrawRootCrosshair(anchor);
    }

    // Anchored by the root — the hex ground point critters stand on, as MapSprite::GetSpriteRootOffset
    // computes it — rather than by the bounds, so the preview holds still while a clip changes the shape
    isize32 sprite_size {_previewSprite->GetSize()};
    ipos32 sprite_offset {_previewSprite->GetOffset()};
    ipos32 root_in_sprite {sprite_size.width / 2 - sprite_offset.x, sprite_size.height - sprite_offset.y};
    ipos32 pos {
        anchor.x - iround<int32_t>(numeric_cast<float32_t>(root_in_sprite.x) * draw_scale),
        anchor.y - iround<int32_t>(numeric_cast<float32_t>(root_in_sprite.y) * draw_scale),
    };

    if (direct) {
#if FO_ENABLE_3D
        // Clear depth again so the crosshair cannot occlude the model, and widen the ortho range so a
        // scaled-up model is not clipped by the near/far planes
        GetApp()->Render.ClearRenderTarget(std::nullopt, true);
        float32_t depth_half {std::max(64.0f, _appliedModelScale * 64.0f)};
        GetApp()->Render.SetOrthoDepthRange(-depth_half, depth_half);
        auto restore_depth {scope_exit([]() noexcept { GetApp()->Render.SetOrthoDepthRange(ORTHO_DEPTH_DEFAULT_NEAR, ORTHO_DEPTH_DEFAULT_FAR); })};
        _previewSprite->DrawInScene(fpos32 {numeric_cast<float32_t>(anchor.x), numeric_cast<float32_t>(anchor.y)}, 0.0f);
#endif
    }
    else {
        isize32 scaled {iround<int32_t>(numeric_cast<float32_t>(sprite_size.width) * draw_scale), iround<int32_t>(numeric_cast<float32_t>(sprite_size.height) * draw_scale)};
        _sprMngr->DrawSpriteSize(_previewSprite.get(), pos, scaled, true, false, Color::Neutral);
        _sprMngr->Flush();
    }

    // Diagnostic overlays go on top of the model so they stay readable
    DrawOverlays(pos, sprite_size, draw_scale);

    GetApp()->Render.SetRenderTarget(prev_rt);
}

void AnimationViewer::DrawRootCrosshair(ipos32 anchor)
{
    // Two full-span segments (LineList draws consecutive point pairs), crossing
    // at the anchor to mark the root the same way for every clip
    array<PrimitivePoint, 4> lines {
        PrimitivePoint {.PointPos = {anchor.x, 0}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {anchor.x, PREVIEW_SIZE.height}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {0, anchor.y}, .PointColor = ROOT_CROSSHAIR_COLOR},
        PrimitivePoint {.PointPos = {PREVIEW_SIZE.width, anchor.y}, .PointColor = ROOT_CROSSHAIR_COLOR},
    };

    _sprMngr->DrawPoints(lines, RenderPrimitiveType::LineList);
}

void AnimationViewer::DrawOverlays(ipos32 sprite_pos, isize32 sprite_size, float32_t draw_scale)
{
    // Map a sprite-local pixel to the preview render target (same transform the
    // model draw uses: top-left at sprite_pos, scaled by the residual draw scale;
    // sprite-local geometry already carries the model's own render scale)
    auto to_screen {[&](ipos32 sl) -> ipos32 { return {sprite_pos.x + iround<int32_t>(numeric_cast<float32_t>(sl.x) * draw_scale), sprite_pos.y + iround<int32_t>(numeric_cast<float32_t>(sl.y) * draw_scale)}; }};

    vector<PrimitivePoint> lines {};

    auto add_segment {[&](ipos32 a, ipos32 b, ucolor color) {
        lines.emplace_back(PrimitivePoint {.PointPos = a, .PointColor = color});
        lines.emplace_back(PrimitivePoint {.PointPos = b, .PointColor = color});
    }};

    auto add_rect {[&](irect32 sprite_rect, ucolor color) {
        ipos32 tl {to_screen({sprite_rect.x, sprite_rect.y})};
        ipos32 tr {to_screen({sprite_rect.x + sprite_rect.width, sprite_rect.y})};
        ipos32 br {to_screen({sprite_rect.x + sprite_rect.width, sprite_rect.y + sprite_rect.height})};
        ipos32 bl {to_screen({sprite_rect.x, sprite_rect.y + sprite_rect.height})};
        add_segment(tl, tr, color);
        add_segment(tr, br, color);
        add_segment(br, bl, color);
        add_segment(bl, tl, color);
    }};
    // Fixed screen-size cross (takes an already-mapped screen point) so a marker
    // stays the same size at any zoom
    auto add_cross {[&](ipos32 c, ucolor color) {
        add_segment({c.x - OVERLAY_CROSS_HALF, c.y}, {c.x + OVERLAY_CROSS_HALF, c.y}, color);
        add_segment({c.x, c.y - OVERLAY_CROSS_HALF}, {c.x, c.y + OVERLAY_CROSS_HALF}, color);
    }};
    // A shaft with a two-barb head at the tip; an axis seen end-on projects to almost nothing and is drawn as the cross alone
    auto add_arrow {[&](ipos32 from, ipos32 to, ucolor color, bool bold) {
        float32_t dx {numeric_cast<float32_t>(to.x - from.x)};
        float32_t dy {numeric_cast<float32_t>(to.y - from.y)};
        float32_t length {std::sqrt(dx * dx + dy * dy)};

        if (length < ARROW_HEAD_LENGTH) {
            return;
        }

        float32_t ux {dx / length};
        float32_t uy {dy / length};
        ipos32 left {to.x - iround<int32_t>(ux * ARROW_HEAD_LENGTH - uy * ARROW_HEAD_HALF_WIDTH), to.y - iround<int32_t>(uy * ARROW_HEAD_LENGTH + ux * ARROW_HEAD_HALF_WIDTH)};
        ipos32 right {to.x - iround<int32_t>(ux * ARROW_HEAD_LENGTH + uy * ARROW_HEAD_HALF_WIDTH), to.y - iround<int32_t>(uy * ARROW_HEAD_LENGTH - ux * ARROW_HEAD_HALF_WIDTH)};

        add_segment(from, to, color);
        add_segment(to, left, color);
        add_segment(to, right, color);

        // LineList has no width: a bold arrow is a second shaft one pixel aside
        if (bold) {
            ipos32 side {iround<int32_t>(-uy), iround<int32_t>(ux)};
            add_segment({from.x + side.x, from.y + side.y}, {to.x + side.x, to.y + side.y}, color);
        }
    }};
    // The joint frame as three arrows from its origin: X red, Y green, Z blue; `main_color` re-colours the Y axis of an attachment
    auto add_axes {[&](const ModelAxisTips& axes, optional<ucolor> main_color) {
        ipos32 origin {to_screen(axes.Origin)};
        add_arrow(origin, to_screen(axes.X), AXIS_X_COLOR, false);
        add_arrow(origin, to_screen(axes.Z), AXIS_Z_COLOR, false);
        add_arrow(origin, to_screen(axes.Y), main_color.value_or(AXIS_Y_COLOR), true);
    }};

    if (_drawRenderRect) {
        // The draw rect is the maximal area the model rasterizes into, larger than the cropped sprite
        irect32 render_local {0, 0, sprite_size.width, sprite_size.height};
#if FO_ENABLE_3D
        if (auto model_spr {_previewSprite.dyn_cast<ModelSprite>()}) {
            // Both corners come from the tight draw rect, whose origin is the model root. Sizing this box by the
            // allocated frame instead drew it off centre: the frame is rounded up onto the 16 px grid per side
            irect32 draw_rect {model_spr->GetModel()->GetDrawRect()};
            ipos32 sprite_offset {_previewSprite->GetOffset()};
            ipos32 root_in_sprite {sprite_size.width / 2 - sprite_offset.x, sprite_size.height - sprite_offset.y};
            render_local = {
                root_in_sprite.x + draw_rect.x,
                root_in_sprite.y + draw_rect.y,
                draw_rect.width,
                draw_rect.height,
            };
        }
#endif
        add_rect(render_local, RENDER_RECT_COLOR);
    }

    // View rect / name point live in the sprite's view size, which the map
    // renderer positions bottom-centred inside the frame (MapSprite::GetViewRect)
    if (auto view_size {_previewSprite->GetViewSize()}; view_size.has_value() && (_drawViewRect || _drawNameLevel)) {
        irect32 view_local {
            sprite_size.width / 2 - view_size->width / 2 + view_size->x,
            sprite_size.height - view_size->height + view_size->y,
            view_size->width,
            view_size->height,
        };

        if (_drawViewRect) {
            add_rect(view_local, VIEW_RECT_COLOR);
        }
        if (_drawNameLevel) {
            // Matches CritterHexView::GetNameTextPos, whose offsets are game pixels and therefore scale
            // by the full on-screen zoom rather than the residual draw scale
            ipos32 name_top {to_screen({view_local.x + view_local.width / 2, view_local.y})};
            int32_t name_y {name_top.y + iround<int32_t>(numeric_cast<float32_t>(_engine->Settings->CritterView.NameOffset + _protoNameOffset) * _zoom)};
            add_segment({0, name_y}, {PREVIEW_SIZE.width, name_y}, NAME_POINT_COLOR);
        }
    }

#if FO_ENABLE_3D
    if (!_enabledBones.empty() || !_enabledAttachments.empty()) {
        if (auto model_spr {_previewSprite.dyn_cast<ModelSprite>()}) {
            auto model {model_spr->GetModel()};

            for (const auto& bone : _enabledBones) {
                if (auto bone_pos {model->GetBoneSpritePos(bone)}; bone_pos.has_value()) {
                    add_cross(to_screen(*bone_pos), BoneColor(bone));
                }

                if (auto bone_axes {model->GetBoneSpriteAxes(bone)}; bone_axes.has_value()) {
                    add_axes(*bone_axes, std::nullopt);
                }
            }

            // A ticked attachment is marked where it ends up - its bone displaced by the authored offset - so the offset reads
            // off the preview; its bold Y arrow is the direction an effect emits along and a child model's up
            if (!_enabledAttachments.empty()) {
                for (const auto& point : model->GetAttachPoints()) {
                    if (point.SpritePos.has_value() && _enabledAttachments.count(AttachKey(point)) != 0) {
                        ucolor color {point.Kind == ModelAttachKind::Particle ? PARTICLE_ATTACH_COLOR : MODEL_ATTACH_COLOR};
                        add_cross(to_screen(*point.SpritePos), color);

                        if (point.SpriteAxes.has_value()) {
                            add_axes(*point.SpriteAxes, color);
                        }
                    }
                }
            }
        }
    }
#endif

    if (!lines.empty()) {
        _sprMngr->DrawPoints(lines, RenderPrimitiveType::LineList);
    }
}

#if FO_ENABLE_3D
static void CollectBoneNames(ptr<const ModelBone> bone, unordered_set<hstring>& names)
{
    names.insert(bone->Name);

    for (const auto& child : bone->Children) {
        CollectBoneNames(child.as_ptr(), names);
    }
}
#endif

void AnimationViewer::DrawHierarchy()
{
    FO_TRACE_ZONE(Editor);

    ImGui::TextUnformatted("Model hierarchy");
    ImGui::Separator();

    if (!_previewSprite) {
        ImGui::TextUnformatted("No critter selected");
        return;
    }

#if FO_ENABLE_3D
    auto model_spr {_previewSprite.dyn_cast<ModelSprite>()};

    if (!model_spr) {
        ImGui::TextUnformatted("3D models only");
        return;
    }

    auto model {model_spr->GetModel()};
    auto root_bone {model->GetInformation()->GetRootBone()};

    ImGui::TextDisabled("Tick a bone or attachment to mark it in the preview");

    if (!_enabledBones.empty() || !_enabledAttachments.empty()) {
        ImGui::SameLine();

        if (ImGui::SmallButton("Clear")) {
            _enabledBones.clear();
            _enabledAttachments.clear();
        }
    }

    ImGui::TextDisabled("Arrows: local X (red), Y (green, an effect emits along it), Z (blue)");

    // Attachments are listed as sub-nodes of the bone they connect to, so the tree shows what actually hangs where
    // instead of a global toggle that gives no clue which bone an effect or a weapon belongs to
    vector<ModelAttachPoint> attach_points {model->GetAttachPoints()};

    DrawHierarchyNode(root_bone, attach_points);

    // An attachment can hang on a bone that only an attached model has (a stove outfit carries its own smoke bone), which
    // the body skeleton above never reaches: those bones are listed on their own so the effect is not lost from the tree
    unordered_set<hstring> skeleton {};
    CollectBoneNames(root_bone, skeleton);
    vector<hstring> attached_bones {};

    for (const ModelAttachPoint& point : attach_points) {
        if (point.ParentIndex < 0 && skeleton.count(point.BoneName) == 0 && std::ranges::find(attached_bones, point.BoneName) == attached_bones.end()) {
            attached_bones.emplace_back(point.BoneName);
        }
    }

    if (!attached_bones.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Bones of attached models");

        for (hstring name : attached_bones) {
            DrawAttachedBoneNode(name, attach_points);
        }
    }
#else
    ImGui::TextUnformatted("3D models only");
#endif
}

#if FO_ENABLE_3D
void AnimationViewer::DrawAttachedBoneNode(hstring name, const vector<ModelAttachPoint>& attach_points)
{
    string name_str {string(name)};

    ImGui::PushID(name_str.c_str());

    bool enabled {_enabledBones.count(name) != 0};

    if (ImGui::Checkbox("##pt", &enabled)) {
        if (enabled) {
            _enabledBones.insert(name);
        }
        else {
            _enabledBones.erase(name);
        }
    }

    ImGui::SameLine();

    ucolor color {BoneColor(name)};
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(color.comp.r, color.comp.g, color.comp.b, 255));
    bool open {ImGui::TreeNodeEx(name_str.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen)};
    ImGui::PopStyleColor();

    if (open) {
        for (size_t i {0}; i != attach_points.size(); ++i) {
            if (attach_points[i].ParentIndex < 0 && attach_points[i].BoneName == name) {
                DrawAttachNode(attach_points, numeric_cast<int32_t>(i));
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void AnimationViewer::DrawHierarchyNode(ptr<const ModelBone> bone, const vector<ModelAttachPoint>& attach_points)
{
    hstring name {bone->Name};
    string name_str {string(name)};
    bool has_attachments {false};

    for (const ModelAttachPoint& point : attach_points) {
        if (point.ParentIndex < 0 && point.BoneName == name) {
            has_attachments = true;
            break;
        }
    }

    ImGui::PushID(bone.get());

    bool enabled {_enabledBones.count(name) != 0};

    if (ImGui::Checkbox("##pt", &enabled)) {
        if (enabled) {
            _enabledBones.insert(name);
        }
        else {
            _enabledBones.erase(name);
        }
    }

    ImGui::SameLine();

    // Colour the label with the bone's marker colour so tree and preview match
    ucolor color {BoneColor(name)};
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(color.comp.r, color.comp.g, color.comp.b, 255));

    ImGuiTreeNodeFlags flags {ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen};

    if (bone->Children.empty() && !has_attachments) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool open {ImGui::TreeNodeEx(name_str.c_str(), flags)};
    ImGui::PopStyleColor();

    if (open && (!bone->Children.empty() || has_attachments)) {
        // Attachments first: they belong to this bone, while the child bones continue the skeleton below it
        for (size_t i {0}; i != attach_points.size(); ++i) {
            if (attach_points[i].ParentIndex < 0 && attach_points[i].BoneName == name) {
                DrawAttachNode(attach_points, numeric_cast<int32_t>(i));
            }
        }

        for (const auto& child : bone->Children) {
            DrawHierarchyNode(child.as_ptr(), attach_points);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void AnimationViewer::DrawAttachNode(const vector<ModelAttachPoint>& attach_points, int32_t index)
{
    const ModelAttachPoint& point {attach_points[numeric_cast<size_t>(index)]};
    bool particle {point.Kind == ModelAttachKind::Particle};
    string key {AttachKey(point)};
    bool has_children {false};

    for (const ModelAttachPoint& other : attach_points) {
        if (other.ParentIndex == index) {
            has_children = true;
            break;
        }
    }

    ImGui::PushID(index);

    bool enabled {_enabledAttachments.count(key) != 0};

    if (ImGui::Checkbox("##pt", &enabled)) {
        if (enabled) {
            _enabledAttachments.insert(key);
        }
        else {
            _enabledAttachments.erase(key);
        }
    }

    ImGui::SameLine();

    // Colour the label with the marker colour the preview uses for this kind so tree and cross match
    ucolor color {particle ? PARTICLE_ATTACH_COLOR : MODEL_ATTACH_COLOR};
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(color.comp.r, color.comp.g, color.comp.b, 255));

    ImGuiTreeNodeFlags flags {ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen};

    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool open {ImGui::TreeNodeEx(point.Name.c_str(), flags)};
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        if (point.SpritePos.has_value()) {
            ImGui::SetTooltip("%s on %s\nauthored move %.3g %.3g %.3g\nsprite pixel %d,%d", particle ? "particle effect" : "child model", string(point.BoneName).c_str(), point.Move.x, point.Move.y, point.Move.z, point.SpritePos->x, point.SpritePos->y);
        }
        else {
            ImGui::SetTooltip("%s on %s\nauthored move %.3g %.3g %.3g\nno frame to project into yet", particle ? "particle effect" : "child model", string(point.BoneName).c_str(), point.Move.x, point.Move.y, point.Move.z);
        }
    }

    if (open && has_children) {
        for (size_t i {0}; i != attach_points.size(); ++i) {
            if (attach_points[i].ParentIndex == index) {
                DrawAttachNode(attach_points, numeric_cast<int32_t>(i));
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

auto AnimationViewer::AttachKey(const ModelAttachPoint& point) -> string
{
    // Keep the key stable when live attachment indices shift as effects finish, but distinguish repeated uses of the
    // same resource on one bone at different authored offsets
    return strex("{}@{}@{}@{}:{}:{}", static_cast<int32_t>(point.Kind), point.Name, point.BoneName, std::bit_cast<uint32_t>(point.Move.x), std::bit_cast<uint32_t>(point.Move.y), std::bit_cast<uint32_t>(point.Move.z)).str();
}
#endif

auto AnimationViewer::BoneColor(hstring bone_name) -> ucolor
{
    string name {string(bone_name)};
    uint32_t hash {2166136261u};

    // FNV-1a over the raw bytes; static_cast is the byte reinterpretation a hash
    // wants (numeric_cast would throw on the high-bit bytes of non-ASCII names)
    for (char c : name) {
        hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
    }

    return BONE_PALETTE[hash % BONE_PALETTE.size()];
}

auto AnimationViewer::MakeAnimationLabel(CritterStateAnim state_anim, CritterActionAnim action_anim) const -> string
{
    bool state_failed {false};
    bool action_failed {false};
    string_view state_name {_engine->ResolveEnumValueName("CritterStateAnim", static_cast<int32_t>(state_anim), &state_failed)};
    string_view action_name {_engine->ResolveEnumValueName("CritterActionAnim", static_cast<int32_t>(action_anim), &action_failed)};

    // Unnamed values still deserve a row: authored content may use a raw value
    // the enum does not spell out, and hiding it would misreport coverage
    string state_label {state_failed ? strex("{}", static_cast<int32_t>(state_anim)).str() : string(state_name)};
    string action_label {action_failed ? strex("{}", static_cast<int32_t>(action_anim)).str() : string(action_name)};

    return strex("{} / {}", state_label, action_label).str();
}

FO_END_NAMESPACE
