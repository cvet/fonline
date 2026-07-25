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

#include "ParticleEditor.h"
#include "ImGuiStuff.h"
#include "Mapper.h"
#include "ParticleSprites.h"
#include "SparkParticleEditor.h"

FO_BEGIN_NAMESPACE

static constexpr int32_t PARTICLE_PREVIEW_OFFSET_LIMIT = 100000;

template<size_t Size>
static auto ParticleInputBufferView(const array<char, Size>& buffer) -> string_view
{
    FO_STACK_TRACE_ENTRY();

    auto end = std::find(buffer.begin(), buffer.end(), '\0');
    return {buffer.data(), numeric_cast<size_t>(std::distance(buffer.begin(), end))};
}

static auto ParticlePathContainsCaseInsensitive(string_view text, string_view filter) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (filter.empty()) {
        return true;
    }

    string normalized_text = strex(text).lower().str();
    string normalized_filter = strex(filter).lower().str();
    return normalized_text.find(normalized_filter) != string::npos;
}

class ParticlePreviewSubEditor final : public ParticleSubEditor
{
public:
    explicit ParticlePreviewSubEditor(ptr<MapperEngine> mapper) :
        _mapper {mapper}
    {
        FO_STACK_TRACE_ENTRY();
    }

    ~ParticlePreviewSubEditor() override { FO_STACK_TRACE_ENTRY(); }

    void Initialize() override;
    void Shutdown() override;
    void ResetLayout() override;
    void OnFocusGained() override;
    void OnCurrentMapChanging(nptr<MapView> next_map) override;
    void OnMapUnloading(ptr<MapView> map) override;
    void DrawMenuItem() override;
    void DrawWindows() override;

private:
    void Play(mpos hex);
    void AttachMapSprite();
    void Remove();
    void RefreshResources(bool force_reload = false);
    auto ResolveHex() -> optional<mpos>;

    ptr<MapperEngine> _mapper;
    nptr<ParticleSpriteFactory> _particleFactory {};
    vector<string> _extensions {};
    vector<string> _resourcePaths {};
    unordered_map<string, pair<size_t, uint64_t>> _resourceIndex {};
    shared_ptr<Sprite> _previewSprite {};
    nptr<MapView> _previewMap {};
    nptr<MapSprite> _previewMapSprite {};
    string _resourcePath {};
    optional<mpos> _mouseHex {};
    mpos _previewHex {};
    ipos32 _offset {};
    array<char, 160> _filterBuf {};
    float32_t _scale {1.0f};
    int32_t _seed {};
    bool _prewarm {};
    bool _useMapCenter {true};
    bool _resourcesIndexed {};
    bool _mapSpriteValid {};
    bool _windowVisible {};
    bool _enabled {};
};

void ParticlePreviewSubEditor::Initialize()
{
    FO_STACK_TRACE_ENTRY();

    _particleFactory = _mapper->SprMngr.GetSpriteFactory(typeid(ParticleSpriteFactory)).dyn_cast<ParticleSpriteFactory>();
    FO_VERIFY_AND_THROW(_particleFactory, "Particle sprite factory is not registered");

    _extensions = _particleFactory->GetExtensions();
    _enabled = !_extensions.empty();

    if (!_enabled || _mapper->Settings->ParticlePreviewEffect.empty() || !_mapper->GetCurMap()) {
        return;
    }

    RefreshResources();
    _resourcePath = _mapper->Settings->ParticlePreviewEffect;
    _seed = _mapper->Settings->ParticlePreviewSeed;
    _scale = std::isfinite(_mapper->Settings->ParticlePreviewScale) ? std::clamp(_mapper->Settings->ParticlePreviewScale, 0.01f, 100.0f) : 1.0f;
    _prewarm = _mapper->Settings->ParticlePreviewPrewarm;
    _windowVisible = true;

    if (optional<mpos> preview_hex = ResolveHex()) {
        Play(*preview_hex);
    }
    else {
        WriteLog(LogType::Warning, "Mapper startup particle preview cannot resolve a placement hex for '{}'", _resourcePath);
    }
}

void ParticlePreviewSubEditor::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    Remove();
}

void ParticlePreviewSubEditor::ResetLayout()
{
    FO_STACK_TRACE_ENTRY();

    _windowVisible = false;
    Remove();
}

void ParticlePreviewSubEditor::OnFocusGained()
{
    FO_STACK_TRACE_ENTRY();

    if (_resourcesIndexed) {
        RefreshResources();
    }
}

void ParticlePreviewSubEditor::OnCurrentMapChanging(nptr<MapView> next_map)
{
    FO_STACK_TRACE_ENTRY();

    if (_previewMap && _previewMap != next_map) {
        Remove();
    }

    _mouseHex.reset();
}

void ParticlePreviewSubEditor::OnMapUnloading(ptr<MapView> map)
{
    FO_STACK_TRACE_ENTRY();

    nptr<MapView> map_view = map;

    if (_previewMap == map_view) {
        Remove();
    }
    if (_mapper->GetCurMap() == map_view) {
        _mouseHex.reset();
    }
}

void ParticlePreviewSubEditor::DrawMenuItem()
{
    FO_STACK_TRACE_ENTRY();

    if (_enabled) {
        ImGui::MenuItem("Particle preview", nullptr, &_windowVisible);
    }
}

void ParticlePreviewSubEditor::DrawWindows()
{
    FO_STACK_TRACE_ENTRY();

    if (!_enabled) {
        return;
    }
    if (!_windowVisible) {
        Remove();
        return;
    }

    if (!_mapSpriteValid && _previewMapSprite) {
        _previewMapSprite.reset();

        if (_mapper->GetCurMap() && _previewMap && _previewSprite && _mapper->GetCurMap() == _previewMap) {
            AttachMapSprite();
        }
        else {
            Remove();
        }
    }

    if (_mapper->GetCurMap() && !_mapper->IsImGuiMouseCaptured()) {
        auto cur_map = _mapper->GetCurMap().as_ptr();
        mpos mouse_hex;

        if (cur_map->GetHexAtScreen(_mapper->MousePos, mouse_hex, nullptr)) {
            _mouseHex = mouse_hex;
        }
        else {
            _mouseHex.reset();
        }
    }

    ImGui::SetNextWindowPos({128.0f, 96.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({480.0f, 600.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Particle Preview", &_windowVisible, 0)) {
        ImGui::End();

        if (!_windowVisible) {
            Remove();
        }

        return;
    }

    if (!_resourcesIndexed || (ImGui::IsWindowAppearing() && !_mapSpriteValid)) {
        RefreshResources();
    }

    ImGui::InputTextWithHint("##ParticlePreviewFilter", "Search particle resources...", _filterBuf.data(), _filterBuf.size());
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        RefreshResources(true);
    }

    string_view particle_filter = ParticleInputBufferView(_filterBuf);
    size_t visible_resource_count = std::ranges::count_if(_resourcePaths, [&](const string& particle_path) { return ParticlePathContainsCaseInsensitive(particle_path, particle_filter); });
    ImGui::Text("Resources: %d", numeric_cast<int32_t>(visible_resource_count));

    if (ImGui::BeginChild("##ParticlePreviewResources", {0.0f, 230.0f}, true)) {
        for (const string& particle_path : _resourcePaths) {
            if (!ParticlePathContainsCaseInsensitive(particle_path, particle_filter)) {
                continue;
            }

            bool selected = _resourcePath == particle_path;

            if (ImGui::Selectable(particle_path.c_str(), selected) && !selected) {
                Remove();
                _resourcePath = particle_path;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (_resourcePath.empty()) {
        ImGui::TextDisabled("Select a particle resource");
    }
    else {
        ImGui::TextWrapped("Selected: %s", _resourcePath.c_str());
    }

    if (ImGui::RadioButton("Mouse position", !_useMapCenter)) {
        _useMapCenter = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("View center", _useMapCenter)) {
        _useMapCenter = true;
    }

    optional<mpos> placement_hex = ResolveHex();

    if (placement_hex) {
        ImGui::Text("Placement hex: %d, %d", placement_hex->x, placement_hex->y);
    }
    else if (!_mapper->GetCurMap()) {
        ImGui::TextDisabled("Load a map to place the preview");
    }
    else if (!_useMapCenter) {
        ImGui::TextDisabled("Move the pointer over the map to capture a hex");
    }
    else {
        ImGui::TextDisabled("The view center is outside the map");
    }

    ImGui::DragFloat("Scale", &_scale, 0.05f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    _scale = std::isfinite(_scale) ? std::clamp(_scale, 0.01f, 100.0f) : 1.0f;
    ImGui::DragInt("Offset X", &_offset.x, 1.0f, -PARTICLE_PREVIEW_OFFSET_LIMIT, PARTICLE_PREVIEW_OFFSET_LIMIT, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragInt("Offset Y", &_offset.y, 1.0f, -PARTICLE_PREVIEW_OFFSET_LIMIT, PARTICLE_PREVIEW_OFFSET_LIMIT, "%d", ImGuiSliderFlags_AlwaysClamp);
    _offset.x = std::clamp(_offset.x, -PARTICLE_PREVIEW_OFFSET_LIMIT, PARTICLE_PREVIEW_OFFSET_LIMIT);
    _offset.y = std::clamp(_offset.y, -PARTICLE_PREVIEW_OFFSET_LIMIT, PARTICLE_PREVIEW_OFFSET_LIMIT);

    ImGui::InputInt("Seed", &_seed);

    ImGui::Checkbox("Prewarm", &_prewarm);
    ImGui::TextDisabled("Scale, offset, seed and prewarm apply on Play or Restart.");

    bool can_play = !_resourcePath.empty() && placement_hex.has_value();
    ImGui::BeginDisabled(!can_play);
    if (ImGui::Button("Play")) {
        Play(*placement_hex);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!_mapSpriteValid);
    if (ImGui::Button("Restart")) {
        Play(_previewHex);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        Remove();
    }
    ImGui::EndDisabled();

    if (_mapSpriteValid) {
        ImGui::Text("Active at: %d, %d", _previewHex.x, _previewHex.y);
    }

    ImGui::End();

    if (!_windowVisible) {
        Remove();
    }
}

void ParticlePreviewSubEditor::Play(mpos hex)
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapper->GetCurMap() || _resourcePath.empty()) {
        return;
    }

    shared_ptr<Sprite> preview_sprite = _mapper->SprMngr.LoadSprite(_resourcePath, AtlasType::MapSprites);

    if (!preview_sprite) {
        _mapper->AddMess(strex("Particle preview load failed: {}", _resourcePath));
        return;
    }

    auto particle_sprite = preview_sprite.dyn_cast<ParticleSprite>();

    if (!particle_sprite) {
        _mapper->AddMess(strex("Particle preview resource is not a particle sprite: {}", _resourcePath));
        return;
    }

    particle_sprite->PlayWithSeed(_seed);

    particle_sprite->GetParticle()->SetScale(_scale);

    if (_prewarm) {
        preview_sprite->Prewarm();
    }

    Remove();

    _previewSprite = std::move(preview_sprite);
    _previewMap = _mapper->GetCurMap().as_ptr();
    _previewHex = hex;
    AttachMapSprite();
    WriteLog("Mapper particle preview started: '{}' at ({}, {}), seed {}, scale {}, prewarm {}", _resourcePath, hex.x, hex.y, _seed, _scale, _prewarm);
}

void ParticlePreviewSubEditor::AttachMapSprite()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_previewSprite, "Particle preview sprite is missing", _resourcePath);
    FO_VERIFY_AND_THROW(_previewMap && _mapper->GetCurMap() == _previewMap, "Particle preview map is not current", _resourcePath, _previewHex);
    FO_VERIFY_AND_THROW(!_previewMapSprite, "Particle preview already has a map sprite", _resourcePath, _previewHex);

    auto preview_map = _previewMap.as_ptr();
    _mapSpriteValid = false;
    _previewMapSprite = preview_map->AddMapSprite(_previewSprite.as_ptr(), _previewHex, DrawOrderType::Particles, 0, _offset, nullptr, nullptr, &_mapSpriteValid);

    FO_VERIFY_AND_THROW(_mapSpriteValid, "Particle preview map sprite was not activated", _resourcePath, _previewHex);
}

void ParticlePreviewSubEditor::Remove()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!_mapSpriteValid || _previewMapSprite, "Valid particle preview is missing its map sprite", _resourcePath);

    if (_mapSpriteValid) {
        _previewMapSprite->Invalidate();
    }

    _mapSpriteValid = false;
    _previewMapSprite.reset();
    _previewMap.reset();
    _previewSprite.reset();
}

void ParticlePreviewSubEditor::RefreshResources(bool force_reload)
{
    FO_STACK_TRACE_ENTRY();

    bool resources_changed = _mapper->Resources.ReindexDataSources();

    if (_resourcesIndexed && !force_reload && !resources_changed) {
        return;
    }

    vector<string> resource_paths;
    unordered_set<string> resource_dirs;
    unordered_map<string, pair<size_t, uint64_t>> resource_index;

    for (const string& particle_extension : _extensions) {
        FileCollection particle_files = _mapper->Resources.FilterFiles(particle_extension);
        resource_paths.reserve(resource_paths.size() + particle_files.GetFilesCount());

        for (const auto& particle_file : particle_files) {
            string particle_path {particle_file.GetPath()};
            resource_paths.emplace_back(particle_path);
            resource_dirs.emplace(strex(particle_path).extract_dir().format_path().str());
        }
    }

    std::ranges::sort(resource_paths);
    resource_paths.erase(std::ranges::unique(resource_paths).begin(), resource_paths.end());

    for (const string& resource_dir : resource_dirs) {
        FileCollection dependency_files = _mapper->Resources.FilterFiles("", resource_dir);

        for (const auto& dependency_file : dependency_files) {
            resource_index.insert_or_assign(string(dependency_file.GetPath()), pair {dependency_file.GetSize(), dependency_file.GetWriteTime()});
        }
    }

    unordered_set<string> changed_paths;
    size_t added_count = 0;
    size_t modified_count = 0;
    size_t removed_count = 0;

    for (const auto& [path, stamp] : resource_index) {
        if (auto it = _resourceIndex.find(path); it == _resourceIndex.end()) {
            changed_paths.emplace(path);
            added_count++;
        }
        else if (it->second != stamp) {
            changed_paths.emplace(path);
            modified_count++;
        }
    }

    for (const auto& [path, stamp] : _resourceIndex) {
        ignore_unused(stamp);

        if (!resource_index.contains(path)) {
            changed_paths.emplace(path);
            removed_count++;
        }
    }

    if (force_reload) {
        for (const string& path : resource_index | std::views::keys) {
            changed_paths.emplace(path);
        }
    }

    for (const string& changed_path : changed_paths) {
        _mapper->SprMngr.InvalidateSpriteResource(changed_path);
    }

    bool had_index = _resourcesIndexed;
    bool preview_was_active = static_cast<bool>(_previewSprite);
    mpos preview_hex = _previewHex;
    bool selected_resource_changed = force_reload;

    if (!_resourcePath.empty() && !selected_resource_changed) {
        string selected_dir = strex(_resourcePath).extract_dir().format_path().str();
        string selected_dir_prefix = selected_dir.empty() ? string() : strex("{}/", selected_dir).str();

        selected_resource_changed = std::ranges::any_of(changed_paths, [&](const string& changed_path) { return changed_path == _resourcePath || (!selected_dir_prefix.empty() && changed_path.starts_with(selected_dir_prefix)); });
    }

    _resourcePaths = std::move(resource_paths);
    _resourceIndex = std::move(resource_index);
    _resourcesIndexed = true;

    bool selected_resource_exists = _resourcePath.empty() || std::ranges::find(_resourcePaths, _resourcePath) != _resourcePaths.end();

    if (!selected_resource_exists) {
        Remove();
        _mapper->AddMess(strex("Particle preview resource removed: {}", _resourcePath));
        _resourcePath.clear();
    }
    else if (had_index && preview_was_active && selected_resource_changed) {
        Remove();
        Play(preview_hex);
    }

    for (const string& particle_path : _resourcePaths) {
        _mapper->SprMngr.ForgetFailedSprite(particle_path);
    }

    if (had_index && (added_count != 0 || modified_count != 0 || removed_count != 0)) {
        WriteLog("Mapper particle resources reindexed: {} added, {} modified, {} removed", added_count, modified_count, removed_count);
    }
}

auto ParticlePreviewSubEditor::ResolveHex() -> optional<mpos>
{
    FO_STACK_TRACE_ENTRY();

    if (!_mapper->GetCurMap()) {
        return std::nullopt;
    }

    if (!_useMapCenter) {
        return _mouseHex;
    }

    auto cur_map = _mapper->GetCurMap().as_ptr();
    isize32 screen_size = cur_map->GetScreenSize();
    ipos32 screen_center {screen_size.width / 2, screen_size.height / 2};
    mpos center_hex;

    if (!cur_map->GetHexAtScreen(screen_center, center_hex, nullptr)) {
        return std::nullopt;
    }

    return center_hex;
}

ParticleEditorManager::ParticleEditorManager(ptr<MapperEngine> mapper)
{
    FO_STACK_TRACE_ENTRY();

#if FO_SPARK_PARTICLES
    _subEditors.emplace_back(CreateSparkParticleSubEditor(mapper));
#endif
    _subEditors.emplace_back(SafeAlloc::MakeUnique<ParticlePreviewSubEditor>(mapper));
}

ParticleEditorManager::~ParticleEditorManager()
{
    FO_STACK_TRACE_ENTRY();
}

void ParticleEditorManager::Initialize()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->Initialize();
    }
}

void ParticleEditorManager::Shutdown()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->Shutdown();
    }
}

void ParticleEditorManager::ResetLayout()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->ResetLayout();
    }
}

void ParticleEditorManager::OnFocusGained()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->OnFocusGained();
    }
}

void ParticleEditorManager::OnCurrentMapChanging(nptr<MapView> next_map)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->OnCurrentMapChanging(next_map);
    }
}

void ParticleEditorManager::OnMapUnloading(ptr<MapView> map)
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->OnMapUnloading(map);
    }
}

void ParticleEditorManager::DrawMenuItems()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        sub_editor->DrawMenuItem();
    }
}

void ParticleEditorManager::DrawWindows()
{
    FO_STACK_TRACE_ENTRY();

    for (auto& sub_editor : _subEditors) {
        try {
            sub_editor->DrawWindows();
        }
        catch (const std::exception& ex) {
            ReportExceptionAndContinue(ex);
        }
    }
}

FO_END_NAMESPACE
