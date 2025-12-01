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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "SparkExtension.h"
#include "VisualParticles.h"

#include "SPARK.h"

FO_BEGIN_NAMESPACE();

struct ParticleEditor::Impl
{
    // Generic
    void DrawGenericSparkObject(const SPK::Ref<SPK::SPKObject>& obj);
    // Core
    void DrawSparkTransformable(const SPK::Ref<SPK::Transformable>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::System>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Group>& obj);
    // Interpolators
    void DrawSparkObject(const SPK::Ref<SPK::FloatDefaultInitializer>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ColorDefaultInitializer>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::FloatRandomInitializer>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ColorRandomInitializer>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::FloatSimpleInterpolator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ColorSimpleInterpolator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::FloatRandomInterpolator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ColorRandomInterpolator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::FloatGraphInterpolator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ColorGraphInterpolator>& obj);
    // Zones
    void DrawSparkInnerZone(const char* name, const function<SPK::Ref<SPK::Zone>()>& get, const function<void(const SPK::Ref<SPK::Zone>&)>& set);
    void DrawSparkZone(const SPK::Ref<SPK::Zone>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Point>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Sphere>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Plane>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Ring>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Box>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Cylinder>& obj);
    // Emitters
    void DrawSparkEmitter(const SPK::Ref<SPK::Emitter>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::StaticEmitter>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::RandomEmitter>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::StraightEmitter>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::SphericEmitter>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::NormalEmitter>& obj);
    // Modifiers
    void DrawSparkModifier(const SPK::Ref<SPK::Modifier>& obj);
    void DrawSparkZonedModifier(const SPK::Ref<SPK::ZonedModifier>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Gravity>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Friction>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Obstacle>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Rotator>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Collider>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Destroyer>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::Vortex>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::EmitterAttacher>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::PointMass>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::RandomForce>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::LinearForce>& obj);
    // Actions
    void DrawSparkAction(const SPK::Ref<SPK::Action>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::ActionSet>& obj);
    void DrawSparkObject(const SPK::Ref<SPK::SpawnParticlesAction>& obj);
    // Renderers
    void DrawSparkObject(const SPK::Ref<SPK::FO::SparkQuadRenderer>& obj);
    // Helpers
    void DrawSparkArray(const char* label, bool opened, const function<size_t()>& get_size, const function<const SPK::Ref<SPK::SPKObject>(size_t)>& get, const function<void(size_t)>& del, const function<void()>& add_draw);
    void DrawSparkNullableField(const char* label, const function<SPK::Ref<SPK::SPKObject>()>& get, const function<void()>& del, const function<void()>& add_draw);

    unique_ptr<EffectManager> EffectMngr {};
    unique_ptr<GameTimer> GameTime {};
    unique_ptr<ParticleManager> ParticleMngr {};
    unique_ptr<ParticleSystem> Particle {};
    SPK::Ref<SPK::System> SystemBackup {};
    bool AddingMode {true};
    bool RemovingMode {true};
    bool NamingMode {};
    vector<unique_ptr<RenderTexture>> LoadedTextures {};
    vector<string> AllEffects {};
    vector<string> AllTextures {};
    unique_ptr<RenderTexture> RenderTarget {};
    bool Changed {};
};

ParticleEditor::ParticleEditor(string_view asset_path, FOEditor& editor) :
    EditorAssetView("Particle Editor", editor, asset_path),
    _impl {SafeAlloc::MakeUnique<Impl>()}
{
    FO_STACK_TRACE_ENTRY();

    _impl->EffectMngr = SafeAlloc::MakeUnique<EffectManager>(_editor->Settings, _editor->BakedResources);

    _impl->GameTime = SafeAlloc::MakeUnique<GameTimer>(_editor->Settings);

    _impl->ParticleMngr = SafeAlloc::MakeUnique<ParticleManager>(_editor->Settings, *_impl->EffectMngr, _editor->BakedResources, *_impl->GameTime, [&editor, this](string_view path) -> pair<RenderTexture*, frect32> {
        const auto file = editor.BakedResources.ReadFile(path);
        FO_RUNTIME_ASSERT(file);
        auto reader = file.GetReader();

        const auto check_number = reader.GetUInt8();
        FO_RUNTIME_ASSERT(check_number == 42);

        (void)reader.GetLEUInt16();
        (void)reader.GetLEUInt16();
        (void)reader.GetUInt8();
        (void)reader.GetLEInt16();
        (void)reader.GetLEInt16();
        (void)reader.GetUInt8();
        const auto w = reader.GetLEUInt16();
        const auto h = reader.GetLEUInt16();
        (void)reader.GetLEInt16();
        (void)reader.GetLEInt16();

        const auto* data = reader.GetCurBuf();

        auto tex = App->Render.CreateTexture({w, h}, true, false);
        tex->UpdateTextureRegion({}, {w, h}, reinterpret_cast<const ucolor*>(data));

        RenderTexture* tex_raw_ptr = tex.get();
        _impl->LoadedTextures.emplace_back(std::move(tex));

        return {tex_raw_ptr, {0.0f, 0.0f, 1.0f, 1.0f}};
    });

    _impl->Particle = _impl->ParticleMngr->CreateParticle(asset_path);
    _impl->SystemBackup = SPK::SPKObject::copy(SPK::Ref<SPK::System>(_impl->Particle->GetBaseSystem()));

    _impl->RenderTarget = App->Render.CreateTexture({200, 200}, true, true);

    auto fofx_files = _editor->RawResources.FilterFiles("fofx");

    for (const auto& file_header : fofx_files) {
        _impl->AllEffects.emplace_back(file_header.GetPath());
    }

    auto tex_files = _editor->RawResources.FilterFiles("tga", strex(asset_path).extract_dir());

    for (const auto& file_header : tex_files) {
        _impl->AllTextures.emplace_back(file_header.GetPath().substr(strex(asset_path).extract_dir().length() + 1));
    }
}

ParticleEditor::~ParticleEditor() = default;

void ParticleEditor::OnDraw()
{
    FO_STACK_TRACE_ENTRY();

    EditorAssetView::OnDraw();

    _impl->GameTime->FrameAdvance();

    _impl->Changed = false;

    auto&& [draw_width, draw_height] = _impl->Particle->GetDrawSize();

    if (ImGui::BeginChild("Info", {0.0f, numeric_cast<float32>(draw_height + 120)})) {
        ImGui::Checkbox("Adding mode", &_impl->AddingMode);
        ImGui::SameLine();
        ImGui::Checkbox("Removing mode", &_impl->RemovingMode);
        ImGui::SameLine();
        ImGui::Checkbox("Naming mode", &_impl->NamingMode);
        ImGui::Checkbox("Auto replay", &_autoReplay);
        ImGui::Text("Elapsed: %.2f", numeric_cast<float64>(_impl->Particle->GetElapsedTime()));
        ImGui::SameLine();
        ImGui::SliderFloat("Dir angle", &_dirAngle, 0.0f, 360.0f);

        if (ImGui::Button("Respawn")) {
            _impl->Particle->Respawn();
        }

        if (_changed) {
            ImGui::SameLine();
            if (ImGui::Button("Discard")) {
                _changed = _impl->Changed = false;
                _impl->Particle->SetBaseSystem(SPK::System::copy(_impl->SystemBackup).get());
                _impl->Particle->Respawn();
            }

            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                const auto file = _editor->RawResources.ReadFileHeader(_assetPath);
                FO_RUNTIME_ASSERT(file);

                const auto* saver = SPK::IO::IOManager::get().getSaver("xml");
                FO_RUNTIME_ASSERT(saver);

                const auto path = std::string(file.GetDiskPath());

                if (saver->save(path, _impl->Particle->GetBaseSystem(), path)) {
                    _changed = _impl->Changed = false;
                    _impl->SystemBackup = SPK::System::copy(SPK::Ref<SPK::System>(_impl->Particle->GetBaseSystem()));
                    _impl->Particle->Respawn();
                }
            }
        }
    }
    ImGui::EndChild();

    if (ImGui::BeginChild("Hierarchy")) {
        _impl->DrawGenericSparkObject(_impl->Particle->GetBaseSystem());
    }
    ImGui::EndChild();

    _changed |= _impl->Changed;

    if (_impl->Changed) {
        _impl->Particle->Respawn();
    }

    const auto frame_width = numeric_cast<float32>(draw_width);
    const auto frame_height = numeric_cast<float32>(draw_height);
    const auto frame_ratio = frame_width / frame_height;
    const auto proj_height = frame_height * (1.0f / _editor->Settings.ModelProjFactor);
    const auto proj_width = proj_height * frame_ratio;

    const mat44 proj = App->Render.CreateOrthoMatrix(0.0f, proj_width, 0.0f, proj_height, -10.0f, 10.0f);

    mat44 world;
    mat44::Translation({proj_width / 2.0f, proj_height / 4.0f, 0.0f}, world);

    vec3 pos_offest;
    pos_offest = vec3();

    vec3 view_offset;
    view_offset = vec3();

    _impl->Particle->Setup(proj, world, pos_offest, _dirAngle, view_offset);

    auto* prev_rt = App->Render.GetRenderTarget();
    App->Render.SetRenderTarget(_impl->RenderTarget.get());
    App->Render.ClearRenderTarget(ucolor::clear, true);
    _impl->Particle->Draw();
    App->Render.SetRenderTarget(prev_rt);

    auto* draw_list = ImGui::GetWindowDrawList();

    auto pos = ImGui::GetWindowPos();
    pos.x += 120.0f;
    pos.y += 140.0f;

    const auto border_col = ImGui::ColorConvertFloat4ToU32({1.0f, 0.0f, 0.0f, 1.0f});

    draw_list->AddLine({pos.x + frame_width / 2.0f, pos.y}, {pos.x + frame_width / 2.0f, pos.y + frame_height}, border_col);
    draw_list->AddLine({pos.x, pos.y + frame_height - frame_height / 4.0f}, {pos.x + frame_width, pos.y + frame_height - frame_height / 4.0f}, border_col);
    if (App->Render.IsRenderTargetFlipped()) {
        draw_list->AddImage(_impl->RenderTarget.get(), pos, {pos.x + frame_width, pos.y + frame_height}, {0.0f, 1.0f}, {1.0f, 0.0f});
    }
    else {
        draw_list->AddImage(_impl->RenderTarget.get(), pos, {pos.x + frame_width, pos.y + frame_height}, {0.0f, 0.0f}, {1.0f, 1.0f});
    }
    draw_list->AddRect({pos.x - 1.0f, pos.y - 1.0f}, {pos.x + frame_width + 2.0f, pos.y + frame_height + 2.0f}, border_col);

    if (!_impl->Particle->IsActive() && _autoReplay) {
        _impl->Particle->Respawn();
    }
}

#define DRAW_SPK_FLOAT(label, get, set) \
    do { \
        float32 val = obj->get(); \
        Changed |= ImGui::InputFloat(label, &val); \
        obj->set(val); \
    } while (0)
#define DRAW_SPK_BOOL(label, get, set) \
    do { \
        bool val = obj->get(); \
        Changed |= ImGui::Checkbox(label, &val); \
        obj->set(val); \
    } while (0)
#define DRAW_SPK_BOOL_BOOL(label1, label2, get1, get2, set) \
    do { \
        bool val1 = obj->get1(); \
        bool val2 = obj->get2(); \
        Changed |= ImGui::Checkbox(label1, &val1); \
        Changed |= ImGui::Checkbox(label2, &val2); \
        obj->set(val1, val2); \
    } while (0)
#define DRAW_SPK_FLOAT_FLOAT(label1, label2, get1, get2, set) \
    do { \
        float32 val1 = obj->get1(); \
        float32 val2 = obj->get2(); \
        Changed |= ImGui::InputFloat(label1, &val1); \
        Changed |= ImGui::InputFloat(label2, &val2); \
        obj->set(val1, val2); \
    } while (0)
#define DRAW_SPK_INT(label, get, set) \
    do { \
        int32 val = numeric_cast<int32>(obj->get()); \
        Changed |= ImGui::InputInt(label, &val); \
        obj->set(val); \
    } while (0)
#define DRAW_SPK_INT_INT(label1, label2, get1, get2, set) \
    do { \
        int32 val1 = numeric_cast<int32>(obj->get1()); \
        int32 val2 = numeric_cast<int32>(obj->get2()); \
        Changed |= ImGui::InputInt(label1, &val1); \
        Changed |= ImGui::InputInt(label2, &val2); \
        obj->set(val1, val2); \
    } while (0)
#define DRAW_SPK_FLOAT_BOOL(label1, label2, get1, get2, set) \
    do { \
        float32 val1 = obj->get1(); \
        bool val2 = obj->get2(); \
        Changed |= ImGui::InputFloat(label1, &val1); \
        Changed |= ImGui::Checkbox(label2, &val2); \
        obj->set(val1, val2); \
    } while (0)
#define DRAW_SPK_VECTOR(label, get, set) \
    do { \
        float32 val[3] = {obj->get().x, obj->get().y, obj->get().z}; \
        Changed |= ImGui::InputFloat3(label, val); \
        obj->set(SPK::Vector3D(val[0], val[1], val[2])); \
    } while (0)
#define DRAW_SPK_VECTOR_VECTOR(label1, label2, get1, get2, set) \
    do { \
        float32 val1[3] = {obj->get1().x, obj->get1().y, obj->get1().z}; \
        float32 val2[3] = {obj->get2().x, obj->get2().y, obj->get2().z}; \
        Changed |= ImGui::InputFloat3(label1, val1); \
        Changed |= ImGui::InputFloat3(label2, val2); \
        obj->set(SPK::Vector3D(val1[0], val1[1], val1[2]), SPK::Vector3D(val2[0], val2[1], val2[2])); \
    } while (0)
#define DRAW_SPK_COLOR(label, get, set) \
    do { \
        int32 val[4] = {obj->get().r, obj->get().g, obj->get().b, obj->get().a}; \
        Changed |= ImGui::InputInt4(label, val); \
        obj->set(SPK::Color(val[0], val[1], val[2], val[3])); \
    } while (0)
#define DRAW_SPK_COLOR_COLOR(label1, label2, get1, get2, set) \
    do { \
        int32 val1[4] = {obj->get1().r, obj->get1().g, obj->get1().b, obj->get1().a}; \
        int32 val2[4] = {obj->get2().r, obj->get2().g, obj->get2().b, obj->get2().a}; \
        Changed |= ImGui::InputInt4(label1, val1); \
        Changed |= ImGui::InputInt4(label2, val2); \
        obj->set(SPK::Color(val1[0], val1[1], val1[2], val1[3]), SPK::Color(val2[0], val2[1], val2[2], val2[3])); \
    } while (0)
#define DRAW_SPK_COMBO(label, get, set, ...) \
    do { \
        auto val = static_cast<int32>(obj->get()); \
        const char* items[] = {__VA_ARGS__}; \
        Changed |= ImGui::Combo(label, &val, items, sizeof(items) / sizeof(items[0])); \
        obj->set(static_cast<decltype(obj->get())>(val)); \
    } while (0)
#define DRAW_SPK_COMBO_COMBO(label1, label2, get1, get2, set) \
    do { \
        auto val1 = static_cast<int32>(obj->get1()); \
        auto val2 = static_cast<int32>(obj->get2()); \
        Changed |= ImGui::Combo(label1, &val1, items1, sizeof(items1) / sizeof(items1[0])); \
        Changed |= ImGui::Combo(label2, &val2, items2, sizeof(items2) / sizeof(items2[0])); \
        obj->set(static_cast<decltype(obj->get1())>(val1), static_cast<decltype(obj->get2())>(val2)); \
    } while (0)
#define DRAW_SPK_COMBO_COMBO_COMBO(label1, label2, label3, get1, get2, get3, set) \
    do { \
        auto val1 = static_cast<int32>(obj->get1()); \
        auto val2 = static_cast<int32>(obj->get2()); \
        auto val3 = static_cast<int32>(obj->get3()); \
        Changed |= ImGui::Combo(label1, &val1, items1, sizeof(items1) / sizeof(items1[0])); \
        Changed |= ImGui::Combo(label2, &val2, items2, sizeof(items2) / sizeof(items2[0])); \
        Changed |= ImGui::Combo(label3, &val3, items3, sizeof(items3) / sizeof(items3[0])); \
        obj->set(static_cast<decltype(obj->get1())>(val1), static_cast<decltype(obj->get2())>(val2), static_cast<decltype(obj->get3())>(val3)); \
    } while (0)
#define DRAW_SPK_INTERPOLATOR_FIELD(name, get, set) \
    DrawSparkNullableField( \
        name, [obj] { return obj->get(); }, [obj] { obj->set(SPK::Ref<SPK::FloatInterpolator>()); }, \
        [obj] { \
            if (ImGui::Button("Add FloatDefaultInitializer")) { \
                obj->set(SPK::FloatDefaultInitializer::create(0.0f)); \
            } \
            if (ImGui::Button("Add FloatRandomInitializer")) { \
                obj->set(SPK::FloatRandomInitializer::create(0.0f, 0.0f)); \
            } \
            if (ImGui::Button("Add FloatSimpleInterpolator")) { \
                obj->set(SPK::FloatSimpleInterpolator::create(0.0f, 0.0f)); \
            } \
            if (ImGui::Button("Add FloatRandomInterpolator")) { \
                obj->set(SPK::FloatRandomInterpolator::create(0.0f, 0.0f, 0.0f, 0.0f)); \
            } \
            if (ImGui::Button("Add FloatGraphInterpolator")) { \
                obj->set(SPK::FloatGraphInterpolator::create()); \
            } \
        })

// Generic
void ParticleEditor::Impl::DrawGenericSparkObject(const SPK::Ref<SPK::SPKObject>& obj)
{
    FO_STACK_TRACE_ENTRY();

    if (NamingMode) {
        char buf[1000];
        strcpy(buf, obj->getName().c_str());
        if (ImGui::InputText("Name", buf, 1000)) {
            Changed |= true;
            obj->setName(buf);
        }
    }

    /*auto shared = obj->isShared();
    if (ImGui::Checkbox("Shared", &shared)) {
        Changed |= true;
        obj->setShared(shared);
    }*/

#define CHECK_AND_DRAW(cls) \
    do { \
        if (auto&& p = SPK::dynamicCast<cls>(obj)) { \
            DrawSparkObject(p); \
            return; \
        } \
    } while (0)
    CHECK_AND_DRAW(SPK::System);
    CHECK_AND_DRAW(SPK::Group);
    CHECK_AND_DRAW(SPK::FloatDefaultInitializer);
    CHECK_AND_DRAW(SPK::ColorDefaultInitializer);
    CHECK_AND_DRAW(SPK::FloatRandomInitializer);
    CHECK_AND_DRAW(SPK::ColorRandomInitializer);
    CHECK_AND_DRAW(SPK::FloatSimpleInterpolator);
    CHECK_AND_DRAW(SPK::ColorSimpleInterpolator);
    CHECK_AND_DRAW(SPK::FloatRandomInterpolator);
    CHECK_AND_DRAW(SPK::ColorRandomInterpolator);
    CHECK_AND_DRAW(SPK::FloatGraphInterpolator);
    CHECK_AND_DRAW(SPK::ColorGraphInterpolator);
    CHECK_AND_DRAW(SPK::Point);
    CHECK_AND_DRAW(SPK::Sphere);
    CHECK_AND_DRAW(SPK::Plane);
    CHECK_AND_DRAW(SPK::Ring);
    CHECK_AND_DRAW(SPK::Box);
    CHECK_AND_DRAW(SPK::Cylinder);
    CHECK_AND_DRAW(SPK::StaticEmitter);
    CHECK_AND_DRAW(SPK::RandomEmitter);
    CHECK_AND_DRAW(SPK::StraightEmitter);
    CHECK_AND_DRAW(SPK::SphericEmitter);
    CHECK_AND_DRAW(SPK::NormalEmitter);
    CHECK_AND_DRAW(SPK::Gravity);
    CHECK_AND_DRAW(SPK::Friction);
    CHECK_AND_DRAW(SPK::Obstacle);
    CHECK_AND_DRAW(SPK::Rotator);
    CHECK_AND_DRAW(SPK::Collider);
    CHECK_AND_DRAW(SPK::Destroyer);
    CHECK_AND_DRAW(SPK::Vortex);
    CHECK_AND_DRAW(SPK::EmitterAttacher);
    CHECK_AND_DRAW(SPK::PointMass);
    CHECK_AND_DRAW(SPK::RandomForce);
    CHECK_AND_DRAW(SPK::LinearForce);
    CHECK_AND_DRAW(SPK::ActionSet);
    CHECK_AND_DRAW(SPK::SpawnParticlesAction);
    CHECK_AND_DRAW(SPK::FO::SparkQuadRenderer);
#undef CHECK_AND_DRAW

    FO_UNREACHABLE_PLACE();
}

// Core
void ParticleEditor::Impl::DrawSparkTransformable(const SPK::Ref<SPK::Transformable>& obj)
{
    FO_STACK_TRACE_ENTRY();

    const auto is_custom_transform = !obj->getTransform().isLocalIdentity();

    if (is_custom_transform) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
    }

    if (ImGui::TreeNodeEx("Transform")) {
        if (is_custom_transform) {
            ImGui::PopStyleColor();
        }

        Changed |= ImGui::InputFloat3("Position", const_cast<float32*>(&obj->getTransform().getLocal()[12]));
        Changed |= ImGui::InputFloat3("UpAxis", const_cast<float32*>(&obj->getTransform().getLocal()[4]));

        if (ImGui::Button("Fix transform")) {
            Changed |= true;
            obj->getTransform().setPosition(obj->getTransform().getLocalPos());
        }

        if (is_custom_transform) {
            if (ImGui::Button("Reset transform")) {
                Changed |= true;
                obj->getTransform().reset();
            }
        }

        ImGui::TreePop();
    }
    else {
        if (is_custom_transform) {
            ImGui::PopStyleColor();
        }
    }
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::System>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkTransformable(obj);

    DrawSparkArray(
        "Groups", true, [obj] { return obj->getNbGroups(); }, [obj](auto i) { return obj->getGroup(i); }, [obj](auto i) { obj->removeGroup(obj->getGroup(i)); },
        [obj] {
            if (ImGui::Button("Add group")) {
                obj->addGroup(SPK::Group::create());
            }
            else if (ImGui::Button("Clone group")) {
                obj->addGroup(SPK::Group::copy(obj->getGroup(obj->getNbGroups() - 1)));
            }
        });
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Group>& obj)
{
    FO_STACK_TRACE_ENTRY();

    if (auto&& renderer = obj->getRenderer()) {
        bool val = renderer->isActive();
        Changed |= ImGui::Checkbox("Active", &val);
        renderer->setActive(val);
    }

    DrawSparkTransformable(obj);

    DRAW_SPK_INT("Capacity", getCapacity, reallocate);
    DRAW_SPK_FLOAT_FLOAT("Min life time", "Max life time", getMinLifeTime, getMaxLifeTime, setLifeTime);
    DRAW_SPK_BOOL("Immortal", isImmortal, setImmortal);
    DRAW_SPK_BOOL("Still", isStill, setStill);
    DRAW_SPK_BOOL("Distance computation", isDistanceComputationEnabled, enableDistanceComputation);
    DRAW_SPK_BOOL("Sorting", isSortingEnabled, enableSorting);
    DRAW_SPK_FLOAT("Radius", getGraphicalRadius, setRadius);

    DrawSparkNullableField(
        "Color interpolator", [obj] { return obj->getColorInterpolator(); }, [obj] { obj->setColorInterpolator(SPK::Ref<SPK::ColorInterpolator>()); },
        [obj] {
            if (ImGui::Button("Add ColorDefaultInitializer")) {
                obj->setColorInterpolator(SPK::ColorDefaultInitializer::create(SPK::Color()));
            }
            if (ImGui::Button("Add ColorRandomInitializer")) {
                obj->setColorInterpolator(SPK::ColorRandomInitializer::create(SPK::Color(), SPK::Color()));
            }
            if (ImGui::Button("Add ColorSimpleInterpolator")) {
                obj->setColorInterpolator(SPK::ColorSimpleInterpolator::create(SPK::Color(), SPK::Color()));
            }
            if (ImGui::Button("Add ColorRandomInterpolator")) {
                obj->setColorInterpolator(SPK::ColorRandomInterpolator::create(SPK::Color(), SPK::Color(), SPK::Color(), SPK::Color()));
            }
            if (ImGui::Button("Add ColorGraphInterpolator")) {
                obj->setColorInterpolator(SPK::ColorGraphInterpolator::create());
            }
        });

    DRAW_SPK_INTERPOLATOR_FIELD("Scale interpolator", getScaleInterpolator, setScaleInterpolator);
    DRAW_SPK_INTERPOLATOR_FIELD("Mass interpolator", getMassInterpolator, setMassInterpolator);
    DRAW_SPK_INTERPOLATOR_FIELD("Angle interpolator", getAngleInterpolator, setAngleInterpolator);
    DRAW_SPK_INTERPOLATOR_FIELD("Texture index interpolator", getTextureIndexInterpolator, setTextureIndexInterpolator);
    DRAW_SPK_INTERPOLATOR_FIELD("Rotation speed interpolator", getRotationSpeedInterpolator, setRotationSpeedInterpolator);

    DrawSparkArray(
        "Emitters", false, [obj] { return obj->getNbEmitters(); }, [obj](auto i) { return obj->getEmitter(i); }, [obj](auto i) { obj->removeEmitter(obj->getEmitter(i)); },
        [obj] {
            if (ImGui::Button("Add StaticEmitter")) {
                obj->addEmitter(SPK::StaticEmitter::create());
            }
            if (ImGui::Button("Add RandomEmitter")) {
                obj->addEmitter(SPK::RandomEmitter::create());
            }
            if (ImGui::Button("Add StraightEmitter")) {
                obj->addEmitter(SPK::StraightEmitter::create());
            }
            if (ImGui::Button("Add SphericEmitter")) {
                obj->addEmitter(SPK::SphericEmitter::create());
            }
            if (ImGui::Button("Add NormalEmitter")) {
                obj->addEmitter(SPK::NormalEmitter::create());
            }
        });

    DrawSparkArray(
        "Modifiers", false, [obj] { return obj->getNbModifiers(); }, [obj](auto i) { return obj->getModifier(i); }, [obj](auto i) { obj->removeModifier(obj->getModifier(i)); },
        [obj] {
            if (ImGui::Button("Add Gravity")) {
                obj->addModifier(SPK::Gravity::create());
            }
            if (ImGui::Button("Add Friction")) {
                obj->addModifier(SPK::Friction::create());
            }
            if (ImGui::Button("Add Obstacle")) {
                obj->addModifier(SPK::Obstacle::create());
            }
            if (ImGui::Button("Add Rotator")) {
                obj->addModifier(SPK::Rotator::create());
            }
            if (ImGui::Button("Add Collider")) {
                obj->addModifier(SPK::Collider::create());
            }
            if (ImGui::Button("Add Destroyer")) {
                obj->addModifier(SPK::Destroyer::create());
            }
            if (ImGui::Button("Add Vortex")) {
                obj->addModifier(SPK::Vortex::create());
            }
            // Todo: improve EmitterAttacher
            // if (ImGui::Button("Add EmitterAttacher")) {
            //    obj->addModifier(SPK::EmitterAttacher::create());
            //}
            if (ImGui::Button("Add PointMass")) {
                obj->addModifier(SPK::PointMass::create());
            }
            if (ImGui::Button("Add RandomForce")) {
                obj->addModifier(SPK::RandomForce::create());
            }
            if (ImGui::Button("Add LinearForce")) {
                obj->addModifier(SPK::LinearForce::create());
            }
        });

    DrawSparkNullableField(
        "Birth action", [obj] { return obj->getBirthAction(); }, [obj] { obj->setBirthAction(SPK::Ref<SPK::Action>()); },
        [obj] {
            // Todo: improve EmitterAttacher
            // if (ImGui::Button("Add SpawnParticlesAction")) {
            //	obj->setBirthAction(SPK::SpawnParticlesAction::create());
            // }
            // Todo: improve ActionSet
            // else if (ImGui::Button("Add ActionSet")) {
            //	obj->setBirthAction(SPK::ActionSet::create());
            // }
        });

    DrawSparkNullableField(
        "Death action", [obj] { return obj->getDeathAction(); }, [obj] { obj->setDeathAction(SPK::Ref<SPK::Action>()); },
        [obj] {
            // if (ImGui::Button("Add SpawnParticlesAction")) {
            //	obj->setDeathAction(SPK::SpawnParticlesAction::create());
            // }
            // else if (ImGui::Button("Add ActionSet")) {
            //	obj->setDeathAction(SPK::ActionSet::create());
            // }
        });

    DrawSparkNullableField(
        "Renderer", [obj] { return obj->getRenderer(); }, [obj] { obj->setRenderer(SPK::Ref<SPK::Renderer>()); },
        [obj] {
            if (ImGui::Button("Add SparkQuadRenderer")) {
                obj->setRenderer(SPK::FO::SparkQuadRenderer::Create());
            }
        });
}

// Interpolators
void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FloatDefaultInitializer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_FLOAT("Default value", getDefaultValue, setDefaultValue);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ColorDefaultInitializer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_COLOR("Default color", getDefaultValue, setDefaultValue);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FloatRandomInitializer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_FLOAT_FLOAT("Min value", "Max value", getMinValue, getMaxValue, setValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ColorRandomInitializer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_COLOR_COLOR("Min color", "Max color", getMinValue, getMaxValue, setValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FloatSimpleInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_FLOAT_FLOAT("Birth value", "Death value", getBirthValue, getDeathValue, setValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ColorSimpleInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_COLOR_COLOR("Birth value", "Death value", getBirthValue, getDeathValue, setValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FloatRandomInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_FLOAT_FLOAT("Birth min value", "Birth max value", getMinBirthValue, getMaxBirthValue, setBirthValues);
    DRAW_SPK_FLOAT_FLOAT("Death min value", "Death max value", getMinDeathValue, getMaxDeathValue, setDeathValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ColorRandomInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_COLOR_COLOR("Birth min color", "Birth max color", getMinBirthValue, getMaxBirthValue, setBirthValues);
    DRAW_SPK_COLOR_COLOR("Death min color", "Death max color", getMinDeathValue, getMaxDeathValue, setDeathValues);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FloatGraphInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    auto&& graph = obj->getGraph();
    if (ImGui::TreeNodeEx("Keys", 0, "Keys (%d)", numeric_cast<int32>(graph.size()))) {
        int32 delIndex = -1;
        int32 index = 0;

        for (auto it = graph.begin(); it != graph.end(); ++it) {
            const auto& entry = *it;
            string name = strex("{}: {} => {}", entry.x, entry.y0, entry.y1);

            if (ImGui::TreeNodeEx(strex("{}", static_cast<const void*>(&entry)).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", name.c_str())) {
                ImGui::InputFloat("Start", const_cast<float32*>(&entry.y0));
                ImGui::InputFloat("End", const_cast<float32*>(&entry.y1));

                ImGui::TreePop();
            }

            if (RemovingMode) {
                if (ImGui::Button(strex("Remove at {}", entry.x).c_str())) {
                    delIndex = index;
                }
            }

            index++;
        }

        if (delIndex != -1) {
            auto it = graph.begin();
            for (int32 i = 0; i < delIndex; i++) {
                ++it;
            }
            graph.erase(it);
        }

        if (AddingMode) {
            static float32 x = 0.0f;
            ImGui::InputFloat("Position", &x);
            if (ImGui::Button("Insert entry")) {
                obj->addEntry(x, 0.0f, 0.0f);
            }
        }

        ImGui::TreePop();
    }

    const char* items1[] = {"INTERPOLATOR_LIFETIME", "INTERPOLATOR_AGE", "INTERPOLATOR_PARAM", "INTERPOLATOR_VELOCITY"};
    const char* items2[] = {"PARAM_SCALE", "PARAM_MASS", "PARAM_ANGLE", "PARAM_TEXTURE_INDEX", "PARAM_ROTATION_SPEED"};
    DRAW_SPK_COMBO_COMBO("Interpolation type", "Interpolation param", getType, getInterpolatorParam, setType);

    DRAW_SPK_BOOL("Looping", isLoopingEnabled, enableLooping);
    DRAW_SPK_FLOAT("Scale variation", getScaleXVariation, setScaleXVariation);
    DRAW_SPK_FLOAT("Offset variation", getOffsetXVariation, setOffsetXVariation);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ColorGraphInterpolator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    auto&& graph = obj->getGraph();
    if (ImGui::TreeNodeEx("Keys", 0, "Keys (%d)", numeric_cast<int32>(graph.size()))) {
        int32 delIndex = -1;
        int32 index = 0;

        for (auto it = graph.begin(); it != graph.end(); ++it) {
            const auto& entry = *it;
            string name = strex("{}: ({}, {}, {}, {}) => ({}, {}, {}, {})", entry.x, entry.y0.r, entry.y0.g, entry.y0.b, entry.y0.a, entry.y1.r, entry.y1.g, entry.y1.b, entry.y1.a);

            if (ImGui::TreeNodeEx(strex("{}", static_cast<const void*>(&entry)).c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", name.c_str())) {
                int32 c1[] = {entry.y0.r, entry.y0.g, entry.y0.b, entry.y0.a};
                ImGui::InputInt4("Start", c1);
                const_cast<unsigned char&>(entry.y0.r) = numeric_cast<unsigned char>(c1[0]);
                const_cast<unsigned char&>(entry.y0.g) = numeric_cast<unsigned char>(c1[1]);
                const_cast<unsigned char&>(entry.y0.b) = numeric_cast<unsigned char>(c1[2]);
                const_cast<unsigned char&>(entry.y0.a) = numeric_cast<unsigned char>(c1[3]);
                int32 c2[] = {entry.y1.r, entry.y1.g, entry.y1.b, entry.y1.a};
                ImGui::InputInt4("End", c2);
                const_cast<unsigned char&>(entry.y1.r) = numeric_cast<unsigned char>(c2[0]);
                const_cast<unsigned char&>(entry.y1.g) = numeric_cast<unsigned char>(c2[1]);
                const_cast<unsigned char&>(entry.y1.b) = numeric_cast<unsigned char>(c2[2]);
                const_cast<unsigned char&>(entry.y1.a) = numeric_cast<unsigned char>(c2[3]);

                ImGui::TreePop();
            }

            if (RemovingMode) {
                if (ImGui::Button(strex("Remove at {}", entry.x).c_str())) {
                    delIndex = index;
                }
            }

            index++;
        }

        if (delIndex != -1) {
            auto it = graph.begin();
            for (int32 i = 0; i < delIndex; i++) {
                ++it;
            }
            graph.erase(it);
        }

        if (AddingMode) {
            static float32 x = 0.0f;
            ImGui::InputFloat("Position", &x);
            if (ImGui::Button("Insert entry")) {
                obj->addEntry(x, SPK::Color(), SPK::Color());
            }
        }

        ImGui::TreePop();
    }

    const char* items1[] = {"INTERPOLATOR_LIFETIME", "INTERPOLATOR_AGE", "INTERPOLATOR_PARAM", "INTERPOLATOR_VELOCITY"};
    const char* items2[] = {"PARAM_SCALE", "PARAM_MASS", "PARAM_ANGLE", "PARAM_TEXTURE_INDEX", "PARAM_ROTATION_SPEED"};
    DRAW_SPK_COMBO_COMBO("Interpolation type", "Interpolation param", getType, getInterpolatorParam, setType);

    DRAW_SPK_BOOL("Looping", isLoopingEnabled, enableLooping);
    DRAW_SPK_FLOAT("Scale variation", getScaleXVariation, setScaleXVariation);
    DRAW_SPK_FLOAT("Offset variation", getOffsetXVariation, setOffsetXVariation);
}

// Zones
void ParticleEditor::Impl::DrawSparkInnerZone(const char* name, const function<SPK::Ref<SPK::Zone>()>& get, const function<void(const SPK::Ref<SPK::Zone>&)>& set)
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::TreeNodeEx(name)) {
        if (get()) {
            DrawGenericSparkObject(get());

            if (RemovingMode) {
                if (ImGui::Button("Remove zone")) {
                    Changed |= true;
                    set(SPK::Ref<SPK::Zone>());
                }
            }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::Text("%s", "No zone");
            ImGui::PopStyleColor();

            if (AddingMode) {
                if (ImGui::Button("Add Point normal zone")) {
                    set(SPK::Point::create());
                }
                if (ImGui::Button("Add Sphere normal zone")) {
                    set(SPK::Sphere::create());
                }
                if (ImGui::Button("Add Plane normal zone")) {
                    set(SPK::Plane::create());
                }
                if (ImGui::Button("Add Ring normal zone")) {
                    set(SPK::Ring::create());
                }
                if (ImGui::Button("Add Box normal zone")) {
                    set(SPK::Box::create());
                }
                if (ImGui::Button("Add Cylinder normal zone")) {
                    set(SPK::Cylinder::create());
                }

                Changed |= !!get();
            }
        }

        ImGui::TreePop();
    }
}

void ParticleEditor::Impl::DrawSparkZone(const SPK::Ref<SPK::Zone>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkTransformable(obj);
    DRAW_SPK_VECTOR("Position", getPosition, setPosition);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Point>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Sphere>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
    DRAW_SPK_FLOAT("Sphere Radius", getRadius, setRadius);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Plane>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
    DRAW_SPK_VECTOR("Plane Normal", getNormal, setNormal);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Ring>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
    DRAW_SPK_VECTOR("Ring Normal", getNormal, setNormal);
    DRAW_SPK_FLOAT_FLOAT("Ring MinRadius", "Ring MaxRadius", getMinRadius, getMaxRadius, setRadius);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Box>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
    DRAW_SPK_VECTOR("Box Dimension", getDimensions, setDimensions);
    DRAW_SPK_VECTOR_VECTOR("Box FrontAxis", "Box UpAxis", getZAxis, getYAxis, setAxis);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Cylinder>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZone(obj);
    DRAW_SPK_FLOAT_FLOAT("Cylinder Height", "Cylinder Radius", getHeight, getRadius, setDimensions);
    DRAW_SPK_VECTOR("Cylinder Axis", getAxis, setAxis);
}

// Emitters
void ParticleEditor::Impl::DrawSparkEmitter(const SPK::Ref<SPK::Emitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkTransformable(obj);
    DRAW_SPK_BOOL("Active", isActive, setActive);

    auto min_tank = obj->getMinTank();
    const auto min_changed = Changed |= ImGui::InputInt("MinTank", &min_tank);
    auto max_tank = obj->getMaxTank();
    Changed |= ImGui::InputInt("MaxTank", &max_tank);
    if ((min_tank >= 0) != (max_tank >= 0)) {
        if (min_changed) {
            max_tank = min_tank;
        }
        else {
            min_tank = max_tank;
        }
    }
    obj->setTank(min_tank, max_tank);

    DRAW_SPK_FLOAT("Flow", getFlow, setFlow);
    DRAW_SPK_FLOAT_FLOAT("MinForce", "MaxForce", getForceMin, getForceMax, setForce);
    DRAW_SPK_BOOL("UseFullZone", isFullZone, setUseFullZone);
    DrawSparkInnerZone("Zone", [obj] { return obj->getZone() != SPK_DEFAULT_ZONE ? obj->getZone() : SPK::Ref<SPK::Zone>(); }, [obj](auto&& zone) { obj->setZone(zone); });
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::StaticEmitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkEmitter(obj);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::RandomEmitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkEmitter(obj);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::StraightEmitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkEmitter(obj);
    DRAW_SPK_VECTOR("StraightEmitter Direction", getDirection, setDirection);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::SphericEmitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkEmitter(obj);
    DRAW_SPK_VECTOR("SphericEmitter Direction", getDirection, setDirection);
    DRAW_SPK_FLOAT_FLOAT("SphericEmitter MinAngle", "SphericEmitter MaxAngle", getAngleMin, getAngleMax, setAngles);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::NormalEmitter>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkEmitter(obj);
    DrawSparkInnerZone("NormalEmitter NormalZone", [obj] { return obj->getNormalZone(); }, [obj](auto&& zone) { obj->setNormalZone(zone); });
    DRAW_SPK_BOOL("NormalEmitter InvertedNormals", isInverted, setInverted);
}

// Modifiers
void ParticleEditor::Impl::DrawSparkModifier(const SPK::Ref<SPK::Modifier>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkTransformable(obj);
    DRAW_SPK_BOOL("Active", isActive, setActive);
    DRAW_SPK_BOOL("LocalToSystem", isLocalToSystem, setLocalToSystem);
}

void ParticleEditor::Impl::DrawSparkZonedModifier(const SPK::Ref<SPK::ZonedModifier>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);
    DrawSparkInnerZone("Zone", [obj] { return obj->getZone(); }, [obj](auto&& zone) { obj->setZone(zone); });
    DRAW_SPK_COMBO("ZoneTest", getZoneTest, setZoneTest, "ZONE_TEST_INSIDE", "ZONE_TEST_OUTSIDE", "ZONE_TEST_INTERSECT", "ZONE_TEST_ENTER", "ZONE_TEST_LEAVE", "ZONE_TEST_ALWAYS");
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Gravity>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);
    DRAW_SPK_VECTOR("Gravity Value", getValue, setValue);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Friction>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);

    auto value = obj->value;
    Changed |= ImGui::InputFloat("Friction Value", &value);
    obj->value = value;
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Obstacle>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZonedModifier(obj);
    DRAW_SPK_FLOAT("Obstacle BouncingRatio", getBouncingRatio, setBouncingRatio);
    DRAW_SPK_FLOAT("Obstacle Friction", getFriction, setFriction);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Rotator>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Collider>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);
    DRAW_SPK_FLOAT("Collider Elasticity", getElasticity, setElasticity);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Destroyer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZonedModifier(obj);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::Vortex>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);

    DRAW_SPK_VECTOR("Vortex Position", getPosition, setPosition);
    DRAW_SPK_VECTOR("Vortex Direction", getDirection, setDirection);
    DRAW_SPK_FLOAT_BOOL("Vortex RotationSpeed", "Vortex RotationSpeedAngular", getRotationSpeed, isRotationSpeedAngular, setRotationSpeed);
    DRAW_SPK_FLOAT_BOOL("Vortex AttractionSpeed", "Vortex AttractionSpeedLinear", getAttractionSpeed, isAttractionSpeedLinear, setAttractionSpeed);
    DRAW_SPK_FLOAT("Vortex EyeRadius", getEyeRadius, setEyeRadius);
    DRAW_SPK_BOOL("Vortex KillingParticles", isParticleKillingEnabled, enableParticleKilling);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::EmitterAttacher>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);

    // SPK_ATTRIBUTE("base emitter",ATTRIBUTE_TYPE_REF)
    DRAW_SPK_BOOL_BOOL("EmitterAttacher OrientationEnabled", "EmitterAttacher RotationEnabled", isEmitterOrientationEnabled, isEmitterRotationEnabled, enableEmitterOrientation);
    // SPK_ATTRIBUTE("target group",ATTRIBUTE_TYPE_REF)

    // Todo: DrawSparkObject SPK::EmitterAttacher
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::PointMass>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);

    DRAW_SPK_VECTOR("PointMass Position", getPosition, setPosition);
    DRAW_SPK_FLOAT("PointMass Mass", getMass, setMass);
    DRAW_SPK_FLOAT("PointMass Offset", getOffset, setOffset);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::RandomForce>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkModifier(obj);
    DRAW_SPK_VECTOR_VECTOR("RandomForce MinVector", "RandomForce MaxVector", getMinVector, getMaxVector, setVectors);
    DRAW_SPK_FLOAT_FLOAT("RandomForce MinPeriod", "RandomForce MaxPeriod", getMinPeriod, getMaxPeriod, setPeriods);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::LinearForce>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkZonedModifier(obj);
    DRAW_SPK_VECTOR("LinearForce Value", getValue, setValue);
    DRAW_SPK_BOOL_BOOL("LinearForce Relative", "LinearForce RelativeSquaredSpeedUsed", isRelative, isSquaredSpeedUsed, setRelative);
    const char* items1[] = {"PARAM_SCALE", "PARAM_MASS", "PARAM_ANGLE", "PARAM_TEXTURE_INDEX", "PARAM_ROTATION_SPEED"};
    const char* items2[] = {"FACTOR_CONSTANT", "FACTOR_LINEAR", "FACTOR_QUADRATIC", "FACTOR_CUBIC"};
    DRAW_SPK_COMBO_COMBO("LinearForce Param", "LinearForce Factor", getParam, getFactor, setParam);
    DRAW_SPK_FLOAT("LinearForce Coef", getCoef, setCoef);
}

// Actions
void ParticleEditor::Impl::DrawSparkAction(const SPK::Ref<SPK::Action>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_BOOL("Active", isActive, setActive);
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::ActionSet>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DrawSparkAction(obj);

    // Todo: DrawSparkObject SPK::ActionSet
}

void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::SpawnParticlesAction>& obj)
{
    FO_STACK_TRACE_ENTRY();

    FO_RUNTIME_ASSERT_STR(false, "Not implemented");

    DrawSparkAction(obj);

    // Todo: DrawSparkObject SPK::SpawnParticlesAction
}

// Renderers
void ParticleEditor::Impl::DrawSparkObject(const SPK::Ref<SPK::FO::SparkQuadRenderer>& obj)
{
    FO_STACK_TRACE_ENTRY();

    DRAW_SPK_BOOL("Active", isActive, setActive);

    bool alpha_test = obj->isRenderingOptionEnabled(SPK::RENDERING_OPTION_ALPHA_TEST);
    Changed |= ImGui::Checkbox("AlphaTest", &alpha_test);
    obj->enableRenderingOption(SPK::RENDERING_OPTION_ALPHA_TEST, alpha_test);

    bool depth_write = obj->isRenderingOptionEnabled(SPK::RENDERING_OPTION_DEPTH_WRITE);
    Changed |= ImGui::Checkbox("DepthWrite", &depth_write);
    obj->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE, depth_write);

    DRAW_SPK_INT_INT("DrawWidth", "DrawHeight", GetDrawWidth, GetDrawHeight, SetDrawSize);

    DRAW_SPK_FLOAT("AlphaThreshold", isActive, setActive);

    // Effect
    {
        auto&& effect_name = obj->GetEffectName();

        int32 index = -1;

        if (const auto it = std::ranges::find(AllEffects, effect_name); it != AllEffects.end()) {
            index = numeric_cast<int32>(std::distance(AllEffects.begin(), it));
        }

        vector<const char*> eff_items;

        for (const auto& eff : AllEffects) {
            eff_items.push_back(eff.c_str());
        }

        if (ImGui::Combo("Effect", &index, eff_items.data(), numeric_cast<int32>(eff_items.size()))) {
            Changed |= true;
            obj->SetEffectName(eff_items[index]);
        }
    }

    // Texture
    {
        auto&& texture_name = obj->GetTextureName();

        int32 index = -1;

        if (const auto it = std::ranges::find(AllTextures, texture_name); it != AllTextures.end()) {
            index = numeric_cast<int32>(std::distance(AllTextures.begin(), it));
        }

        vector<const char*> tex_items;

        for (const auto& tex : AllTextures) {
            tex_items.push_back(tex.c_str());
        }

        if (ImGui::Combo("Texture", &index, tex_items.data(), numeric_cast<int32>(tex_items.size()))) {
            Changed |= true;
            obj->SetTextureName(tex_items[index]);
        }
    }

    DRAW_SPK_FLOAT_FLOAT("ScaleX", "ScaleY", getScaleX, getScaleY, setScale);
    DRAW_SPK_INT_INT("AtlasDimensionX", "AtlasDimensionY", getAtlasDimensionX, getAtlasDimensionY, setAtlasDimensions);

    const char* items1[] = {"LOOK_CAMERA_PLANE", "LOOK_CAMERA_POINT", "LOOK_AXIS", "LOOK_POINT"};
    const char* items2[] = {"UP_CAMERA", "UP_DIRECTION", "UP_AXIS", "UP_POINT"};
    const char* items3[] = {"LOCK_LOOK", "LOCK_UP"};
    DRAW_SPK_COMBO_COMBO_COMBO("LookOrientation", "UpOrientation", "LockedAxis", getLookOrientation, getUpOrientation, getLockedAxis, setOrientation);

    float32 look[3] = {obj->lookVector.x, obj->lookVector.y, obj->lookVector.z};
    Changed |= ImGui::InputFloat3("LookVector", look);
    obj->lookVector = SPK::Vector3D(look[0], look[1], look[2]);

    float32 up[3] = {obj->upVector.x, obj->upVector.y, obj->upVector.z};
    Changed |= ImGui::InputFloat3("UpVector", up);
    obj->upVector = SPK::Vector3D(up[0], up[1], up[2]);
}

// Helpers
void ParticleEditor::Impl::DrawSparkArray(const char* label, bool opened, const function<size_t()>& get_size, const function<const SPK::Ref<SPK::SPKObject>(size_t)>& get, const function<void(size_t)>& del, const function<void()>& add_draw)
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::TreeNodeEx(label, opened ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s (%d)", label, numeric_cast<int32>(get_size()))) {
        int32 delIndex = -1;

        for (size_t i = 0; i < get_size(); i++) {
            auto&& obj = get(i);

            const string name = strex("{} ({})", obj->getName().empty() ? strex("{}", i + 1) : string(obj->getName()), string(obj->getClassName()));

            if (ImGui::TreeNodeEx(strex("{}", static_cast<const void*>(obj.get())).c_str(), 0, "%s", name.c_str())) {
                DrawGenericSparkObject(obj);
                ImGui::TreePop();
            }

            if (RemovingMode) {
                if (ImGui::Button(strex("Remove {}", name).c_str())) {
                    delIndex = numeric_cast<int32>(i);
                }
            }
        }

        if (delIndex != -1) {
            del(delIndex);
        }

        if (AddingMode) {
            const auto prev_size = get_size();
            add_draw();
            Changed |= (get_size() != prev_size);
        }

        ImGui::TreePop();
    }
}

void ParticleEditor::Impl::DrawSparkNullableField(const char* label, const function<SPK::Ref<SPK::SPKObject>()>& get, const function<void()>& del, const function<void()>& add_draw)
{
    FO_STACK_TRACE_ENTRY();

    if (ImGui::TreeNodeEx(label, 0, "%s (%s)", label, get() ? get()->getClassName().c_str() : "Not assigned")) {
        if (get()) {
            DrawGenericSparkObject(get());

            if (RemovingMode) {
                if (ImGui::Button("Remove")) {
                    Changed |= true;
                    del();
                }
            }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::Text("No %s", label);
            ImGui::PopStyleColor();

            if (AddingMode) {
                add_draw();
                Changed |= !!get();
            }
        }

        ImGui::TreePop();
    }
}

#undef DRAW_SPK_FLOAT
#undef DRAW_SPK_BOOL
#undef DRAW_SPK_BOOL_BOOL
#undef DRAW_SPK_FLOAT_FLOAT
#undef DRAW_SPK_INT
#undef DRAW_SPK_INT_INT
#undef DRAW_SPK_FLOAT_BOOL
#undef DRAW_SPK_VECTOR
#undef DRAW_SPK_VECTOR_VECTOR
#undef DRAW_SPK_COLOR
#undef DRAW_SPK_COLOR_COLOR
#undef DRAW_SPK_COMBO
#undef DRAW_SPK_COMBO_COMBO
#undef DRAW_SPK_COMBO_COMBO_COMBO
#undef DRAW_SPK_INTERPOLATOR_FIELD

FO_END_NAMESPACE();
