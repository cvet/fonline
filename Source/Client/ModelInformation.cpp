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

#include "ModelInformation.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "EngineBase.h"
#include "ModelAnimation.h"
#include "ModelAnimationData.h"
#include "ModelBakedData.h"
#include "ModelHierarchy.h"
#include "ModelInstance.h"
#include "ModelManager.h"
#include "ModelMeshData.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

static constexpr size_t BAKED_STRING_MIN_SIZE = sizeof(uint32_t);
static constexpr size_t BAKED_MODEL_DESCRIPTION_LINK_MIN_SIZE = 2 * sizeof(int32_t) + 2 * BAKED_STRING_MIN_SIZE + sizeof(uint8_t) + 10 * sizeof(float32_t) + 5 * sizeof(uint32_t);
static constexpr size_t BAKED_MODEL_DESCRIPTION_CUT_MIN_SIZE = BAKED_STRING_MIN_SIZE + sizeof(uint32_t) + sizeof(uint32_t) + 3 * BAKED_STRING_MIN_SIZE + sizeof(uint8_t);
static constexpr size_t BAKED_MODEL_DESCRIPTION_ANIM_ENTRY_MIN_SIZE = 2 * sizeof(int32_t) + 2 * BAKED_STRING_MIN_SIZE;

ModelInformation::ModelInformation(ptr<ModelManager> model_mngr) :
    _modelMngr {model_mngr}
{
    FO_STACK_TRACE_ENTRY();
}

ModelInformation::ModelInformation(ModelInformation&&) noexcept = default;
ModelInformation::~ModelInformation() = default;

auto ModelInformation::Load(string_view name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    string ext = strex(name).get_file_extension();

    if (ext.empty()) {
        return false;
    }

    // Load fonline 3d file
    if (ext == "fo3d") {
        // Load main fo3d file
        auto fo3d = _modelMngr->_resources->ReadFile(name);

        if (!fo3d) {
            return false;
        }

        auto reader = DataReader(fo3d.GetDataSpan());

        try {
            FO_VERIFY_AND_THROW(LoadBaked(name, reader), "Failed to load baked 3D asset");
        }
        catch (const DataReadingException& ex) {
            throw DataReadingException("Invalid baked model description", name, ex.what());
        }

        return true;
    }

    // Load just model
    auto hierarchy = _modelMngr->GetHierarchy(name);

    if (!hierarchy) {
        return false;
    }

    // Create animation
    _fileName = name;
    _hierarchy = hierarchy;
    IndexDirectPoseJoints();

    optional<ModelBounds3D> hierarchy_bounds = CalculateHierarchyBounds();
    FO_VERIFY_AND_THROW(hierarchy_bounds, "Direct model hierarchy has no valid drawable bounds", name);
    _modelBounds = *hierarchy_bounds;
    _viewBounds = *hierarchy_bounds;

    return true;
}

auto ModelInformation::LoadBaked(string_view name, DataReader& reader) -> bool
{
    FO_STACK_TRACE_ENTRY();

    const_span<uint8_t> magic = reader.ReadBytes(MODEL_DESCRIPTION_MAGIC.size());
    FO_VERIFY_AND_THROW(std::equal(magic.begin(), magic.end(), MODEL_DESCRIPTION_MAGIC.begin()), "Invalid baked model description magic", name);
    uint16_t schema = reader.Read<uint16_t>();
    uint16_t flags = reader.Read<uint16_t>();
    FO_VERIFY_AND_THROW(schema == MODEL_DESCRIPTION_SCHEMA_VERSION, "Unsupported baked model description schema", name, schema, MODEL_DESCRIPTION_SCHEMA_VERSION);
    FO_VERIFY_AND_THROW((flags & ~MODEL_DESCRIPTION_SUPPORTED_FLAGS) == 0, "Baked model description contains unsupported flags", name, flags);

    string model = reader.ReadString();
    bool disable_animation_interpolation = reader.Read<uint8_t>() != 0;
    _disableBackwardAnim = reader.Read<uint8_t>() != 0;
    _shadowDisabled = reader.Read<uint8_t>() != 0;

    int32_t draw_width = reader.Read<int32_t>();
    int32_t draw_height = reader.Read<int32_t>();
    int32_t view_width = reader.Read<int32_t>();
    int32_t view_height = reader.Read<int32_t>();

    FO_VERIFY_AND_THROW(draw_width == 0 && draw_height == 0 && view_width == 0 && view_height == 0, "Baked model description contains obsolete explicit sprite dimensions", name, draw_width, draw_height, view_width, view_height);

    string rotation_bone = reader.ReadString();
    _rotationBone = !rotation_bone.empty() ? _modelMngr->GetBoneHashedString(rotation_bone) : hstring {};

    BakedModelDescriptionLink default_link = ReadBakedModelDescriptionLink(reader, name);

    uint32_t links_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, links_count, BAKED_MODEL_DESCRIPTION_LINK_MIN_SIZE, "links", name);
    vector<BakedModelDescriptionLink> links;
    links.reserve(links_count);

    for (uint32_t i = 0; i < links_count; i++) {
        BakedModelDescriptionLink link = ReadBakedModelDescriptionLink(reader, name);
        FO_VERIFY_AND_THROW(link.Data.Layer >= 0 && link.Data.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Baked model description contains a model link layer out of range", link.Data.Layer, name);
        FO_VERIFY_AND_THROW(link.Data.LayerValue != 0, "Baked model description contains a model link with zero layer value", name);
        links.emplace_back(std::move(link));
    }

    uint32_t anim_entries_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, anim_entries_count, BAKED_MODEL_DESCRIPTION_ANIM_ENTRY_MIN_SIZE, "animation entries", name);
    vector<BakedModelDescriptionAnimationEntry> anim_entries;
    anim_entries.reserve(anim_entries_count);

    for (uint32_t i = 0; i < anim_entries_count; i++) {
        anim_entries.emplace_back(ReadBakedModelDescriptionAnimationEntry(reader));
    }

    uint32_t anim_speed_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, anim_speed_count, 2 * sizeof(int32_t) + sizeof(float32_t), "animation speeds", name);

    for (uint32_t i = 0; i < anim_speed_count; i++) {
        int32_t state_anim = reader.Read<int32_t>();
        int32_t action_anim = reader.Read<int32_t>();
        float32_t speed = reader.Read<float32_t>();
        FO_VERIFY_AND_THROW(speed > 0.0f, "Baked model description contains a non-positive animation speed", state_anim, action_anim, name, speed);
        _animSpeed.emplace(std::make_pair(static_cast<CritterStateAnim>(state_anim), static_cast<CritterActionAnim>(action_anim)), speed);
    }

    uint32_t anim_layer_values_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, anim_layer_values_count, 4 * sizeof(int32_t), "animation layer values", name);

    for (uint32_t i = 0; i < anim_layer_values_count; i++) {
        BakedModelDescriptionAnimLayerValue value = ReadBakedModelDescriptionAnimLayerValue(reader);
        auto index = std::make_pair(static_cast<CritterStateAnim>(value.StateAnim), static_cast<CritterActionAnim>(value.ActionAnim));
        FO_VERIFY_AND_THROW(value.Layer >= 0 && value.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Baked model description contains an animation layer out of range", value.Layer, name);

        if (_animLayerValues.count(index) == 0) {
            _animLayerValues.emplace(index, vector<pair<int32_t, int32_t>>());
        }

        _animLayerValues[index].emplace_back(value.Layer, value.LayerValue);
    }

    uint32_t fast_transition_bones_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, fast_transition_bones_count, BAKED_STRING_MIN_SIZE, "fast-transition bones", name);

    for (uint32_t i = 0; i < fast_transition_bones_count; i++) {
        string bone_name = reader.ReadString();
        _fastTransitionBones.insert(_modelMngr->GetBoneHashedString(bone_name));
    }

    uint32_t state_anim_equals_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, state_anim_equals_count, 2 * sizeof(int32_t), "state-animation aliases", name);

    for (uint32_t i = 0; i < state_anim_equals_count; i++) {
        int32_t from = reader.Read<int32_t>();
        int32_t to = reader.Read<int32_t>();
        _stateAnimEquals.emplace(static_cast<CritterStateAnim>(from), static_cast<CritterStateAnim>(to));
    }

    uint32_t action_anim_equals_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, action_anim_equals_count, 2 * sizeof(int32_t), "action-animation aliases", name);

    for (uint32_t i = 0; i < action_anim_equals_count; i++) {
        int32_t from = reader.Read<int32_t>();
        int32_t to = reader.Read<int32_t>();
        _actionAnimEquals.emplace(static_cast<CritterActionAnim>(from), static_cast<CritterActionAnim>(to));
    }

    uint64_t animation_rig_data_size = reader.Read<uint64_t>();
    FO_VERIFY_AND_THROW(animation_rig_data_size != 0 && animation_rig_data_size <= std::numeric_limits<size_t>::max(), "Baked model description contains invalid animation rig-data size", name, animation_rig_data_size);
    VerifyModelBakedCountFitsData(reader, animation_rig_data_size, sizeof(uint8_t), "animation rig data", name);
    unique_ptr<ModelAnimationRuntimeRig> animation_runtime_rig = LoadModelAnimationRuntimeRig(reader.ReadBytes(static_cast<size_t>(animation_rig_data_size)), name, model, disable_animation_interpolation);

    reader.VerifyEnd();

    FO_VERIFY_AND_THROW(!model.empty(), "Baked model description has no Model section", name);

    auto hierarchy = _modelMngr->GetHierarchy(model);
    FO_VERIFY_AND_THROW(hierarchy, "Model hierarchy was not found for a baked model description", model, name);
    FO_VERIFY_AND_THROW(!hierarchy->_allDrawBones.empty(), "Model hierarchy has no drawable meshes for a baked model description", model, name);

    _fileName = name;
    _hierarchy = hierarchy;
    IndexAnimationPoseJoints(*animation_runtime_rig);

    auto anim_info = _modelMngr->_engineMetadata->GetAnimationInfo(_modelMngr->_engineMetadata->Hashes.ToHashedString(name));
    FO_VERIFY_AND_THROW(anim_info && anim_info->Model, "Baked model animation information was not found", name);
    _modelBounds = anim_info->Model->ModelBounds;
    _viewBounds = anim_info->Model->ViewBounds;
    FO_VERIFY_AND_THROW(IsValidModelBounds(_modelBounds) && IsValidModelBounds(_viewBounds), "Baked model contains invalid model or view bounds", name);
    _animationBounds.resize(animation_runtime_rig->GetClipCount());
    FO_VERIFY_AND_THROW(!_rotationBone || FindModelBone(_hierarchy->_rootBone, _rotationBone) != nullptr, "Rotation bone was not found in a baked model description", rotation_bone, name);

    for (const hstring& bone_name : _fastTransitionBones) {
        FO_VERIFY_AND_THROW(FindModelBone(_hierarchy->_rootBone, bone_name) != nullptr, "Fast transition bone was not found in a baked model description", bone_name, name);
    }

    auto append_cut_info = [this](ModelAnimationData& link, const BakedModelDescriptionCutInfo& raw_cut) {
        auto area = _modelMngr->GetHierarchy(raw_cut.FileName);
        FO_VERIFY_AND_THROW(area, "Cut file was not found", raw_cut.FileName);

        auto cut_holder = SafeAlloc::MakeUnique<ModelCutData>();
        auto cut = cut_holder.as_ptr();
        _cutData.emplace_back(std::move(cut_holder));

        link.CutInfo.emplace_back(cut);
        cut->Layers = raw_cut.Layers;
        cut->UnskinBone1 = !raw_cut.UnskinBone1.empty() ? _modelMngr->GetBoneHashedString(raw_cut.UnskinBone1) : hstring {};
        cut->UnskinBone2 = !raw_cut.UnskinBone2.empty() ? _modelMngr->GetBoneHashedString(raw_cut.UnskinBone2) : hstring {};
        cut->RevertUnskinShape = raw_cut.RevertUnskinShape;

        hstring unskin_shape_name;

        if (cut->UnskinBone1 && cut->UnskinBone2 && !raw_cut.UnskinShape.empty()) {
            unskin_shape_name = _modelMngr->GetBoneHashedString(raw_cut.UnskinShape);
            bool unskin_shape_found = false;

            for (ptr<ModelBone> bone : area->_allDrawBones) {
                if (unskin_shape_name == bone->Name) {
                    auto mesh = bone->AttachedMesh ? make_nptr(&*bone->AttachedMesh) : nullptr;
                    cut->UnskinShape = CreateCutShape(mesh);
                    unskin_shape_found = true;
                    break;
                }
            }

            FO_VERIFY_AND_THROW(unskin_shape_found, "Unskin shape was not found in a cut file", raw_cut.UnskinShape, raw_cut.FileName);
        }

        for (const string& shape : raw_cut.Shapes) {
            hstring shape_name = !shape.empty() ? _modelMngr->GetBoneHashedString(shape) : hstring {};
            bool shape_found = false;

            for (ptr<ModelBone> bone : area->_allDrawBones) {
                if ((!shape_name || shape_name == bone->Name) && bone->Name != unskin_shape_name) {
                    auto mesh = bone->AttachedMesh ? make_nptr(&*bone->AttachedMesh) : nullptr;
                    cut->Shapes.emplace_back(CreateCutShape(mesh));
                    shape_found = true;
                }
            }

            FO_VERIFY_AND_THROW(shape_found, "Cut shape was not found in a cut file", !shape.empty() ? shape : string {"All"}, raw_cut.FileName);
        }

        FO_VERIFY_AND_THROW(!cut->Shapes.empty(), "Cut file produced no cut shapes", raw_cut.FileName);
    };

    _animDataDefault = std::move(default_link.Data);

    for (const BakedModelDescriptionCutInfo& cut : default_link.CutInfo) {
        append_cut_info(_animDataDefault, cut);
    }

    _animData.reserve(links.size());

    for (BakedModelDescriptionLink& link : links) {
        link.Data.Id = ++_modelMngr->_linkId;

        for (const BakedModelDescriptionCutInfo& cut : link.CutInfo) {
            append_cut_info(link.Data, cut);
        }

        _animData.emplace_back(std::move(link.Data));
    }

    if (!anim_entries.empty()) {
        _animController.emplace(2);
    }

    set<pair<int32_t, int32_t>> expected_runtime_bindings;

    for (const BakedModelDescriptionAnimationEntry& anim_entry : anim_entries) {
        pair<int32_t, int32_t> anim_pair {anim_entry.StateAnim, anim_entry.ActionAnim};

        if (!expected_runtime_bindings.emplace(anim_pair).second) {
            continue;
        }

        auto binding = animation_runtime_rig->FindBinding(anim_entry.StateAnim, anim_entry.ActionAnim);
        FO_VERIFY_AND_THROW(binding, "Animation runtime rig is missing a baked animation binding", name, anim_entry.StateAnim, anim_entry.ActionAnim);
        FO_VERIFY_AND_THROW(binding->Reversed == anim_entry.Name.starts_with('~'), "Animation runtime rig binding has a reverse flag mismatch", name, anim_entry.StateAnim, anim_entry.ActionAnim, anim_entry.Name);

        auto bounds_it = anim_info->Model->AnimationBounds.find({static_cast<CritterStateAnim>(anim_entry.StateAnim), static_cast<CritterActionAnim>(anim_entry.ActionAnim)});
        FO_VERIFY_AND_THROW(bounds_it != anim_info->Model->AnimationBounds.end(), "Animation bounds are missing for a baked model binding", name, anim_entry.StateAnim, anim_entry.ActionAnim);
        FO_VERIFY_AND_THROW(binding->ClipIndex < _animationBounds.size(), "Animation runtime clip index is outside the bounds table", name, binding->ClipIndex, _animationBounds.size());
        FO_VERIFY_AND_THROW(IncludeModelBounds(_animationBounds[binding->ClipIndex], bounds_it->second), "Animation bounds are invalid", name, anim_entry.StateAnim, anim_entry.ActionAnim);
    }

    FO_VERIFY_AND_THROW(expected_runtime_bindings.size() == animation_runtime_rig->GetBindings().size(), "Animation runtime rig binding count does not match the baked model description", name, expected_runtime_bindings.size(), animation_runtime_rig->GetBindings().size());

    if (_animController) {
        for (const BakedModelDescriptionAnimationEntry& anim_entry : anim_entries) {
            auto anim_pair = std::make_pair(static_cast<CritterStateAnim>(anim_entry.StateAnim), static_cast<CritterActionAnim>(anim_entry.ActionAnim));

            if (_animIndexes.count(anim_pair) != 0) {
                continue;
            }

            string anim_path;

            if (anim_entry.FileName == "ModelFile") {
                anim_path = model;
            }
            else {
                anim_path = strex(name).extract_dir().combine_path(anim_entry.FileName);
            }

            bool reversed = anim_entry.Name.starts_with('~');
            string anim_name = reversed ? anim_entry.Name.substr(1) : anim_entry.Name;

            auto binding = animation_runtime_rig->FindBinding(anim_entry.StateAnim, anim_entry.ActionAnim);
            FO_VERIFY_AND_THROW(binding, "Animation runtime rig is missing a resolved animation binding", name, anim_entry.StateAnim, anim_entry.ActionAnim);
            const ModelAnimationRuntimeClip& runtime_clip = animation_runtime_rig->GetClip(binding->ClipIndex);
            bool source_matches = strex(runtime_clip.GetSourceFile()).compare_ignore_case(anim_path);
            bool clip_matches = anim_name == "Base" || strex(runtime_clip.GetClipName()).compare_ignore_case(anim_name);
            FO_VERIFY_AND_THROW(source_matches && clip_matches, "Animation identity does not match its baked model-description entry", name, anim_entry.StateAnim, anim_entry.ActionAnim, anim_path, anim_name, runtime_clip.GetSourceFile(), runtime_clip.GetClipName());
            FO_VERIFY_AND_THROW(binding->Reversed == reversed, "Animation reverse flag does not match its baked model-description entry", name, anim_entry.StateAnim, anim_entry.ActionAnim, anim_entry.Name);

            unordered_set<hstring> bound_bones = BuildModelAnimationBoundBones(_poseJointCanonicalNames, _poseJointRuntimeNames, runtime_clip.GetJointPresence(), name);
            int32_t anim_index = _animController->RegisterAnimation(binding->ClipIndex, runtime_clip.GetDuration(), reversed, std::move(bound_bones));
            _animIndexes.emplace(anim_pair, anim_index);
        }

        int32_t anim_count = _animController->GetAnimationsCount();
        FO_VERIFY_AND_THROW(anim_count != 0, "No animations registered for a baked model description", name);
        FO_STRONG_ASSERT(numeric_cast<size_t>(anim_count) == _animIndexes.size(), "Animation controller metadata count differs from its state/action index", name, anim_count, _animIndexes.size());
    }

    _animationRuntimeRig = std::move(animation_runtime_rig);

    return true;
}

static void IndexModelRestPoseJoints(ptr<const ModelBone> bone, int32_t parent_joint_index, const unordered_map<ptr<const ModelBone>, uint32_t>& pose_joint_indexes, span<ModelPoseJoint> rest_pose_joints, span<uint8_t> indexed_joints, string_view context)
{
    FO_STACK_TRACE_ENTRY();

    int32_t child_parent_joint_index = parent_joint_index;

    if (auto joint_it = pose_joint_indexes.find(bone); joint_it != pose_joint_indexes.end()) {
        uint32_t joint_index = joint_it->second;
        FO_STRONG_ASSERT(joint_index < rest_pose_joints.size() && joint_index < indexed_joints.size(), "Model pose joint index is outside rest-pose metadata", context, joint_index, rest_pose_joints.size(), indexed_joints.size());
        FO_VERIFY_AND_THROW(indexed_joints[joint_index] == 0, "Model pose hierarchy resolves a canonical joint more than once", context, joint_index, bone->Name);
        FO_VERIFY_AND_THROW(parent_joint_index < 0 || numeric_cast<uint32_t>(parent_joint_index) < joint_index, "Model canonical pose is not in parent-before-child order", context, joint_index, parent_joint_index, bone->Name);
        rest_pose_joints[joint_index] = ModelPoseJoint {parent_joint_index, bone->RestLocalTransform};
        indexed_joints[joint_index] = 1;
        child_parent_joint_index = numeric_cast<int32_t>(joint_index);
    }

    for (const auto& child : bone->Children) {
        IndexModelRestPoseJoints(child, child_parent_joint_index, pose_joint_indexes, rest_pose_joints, indexed_joints, context);
    }
}

void ModelInformation::IndexDirectPoseJoints()
{
    FO_STACK_TRACE_ENTRY();

    _poseBones.assign(_hierarchy->_sourceBones.begin(), _hierarchy->_sourceBones.end());
    _poseJointCanonicalNames.clear();
    _poseJointRuntimeNames.clear();
    _poseJointCanonicalNames.reserve(_poseBones.size());
    _poseJointRuntimeNames.reserve(_poseBones.size());

    for (ptr<const ModelBone> bone : _hierarchy->_sourceBones) {
        _poseJointCanonicalNames.emplace_back(bone->SourceName);
        _poseJointRuntimeNames.emplace_back(bone->Name);
    }

    IndexPoseJointLookups();

    vector<ModelPoseJoint> rest_pose_joints(_poseBones.size());
    vector<uint8_t> indexed_joints(_poseBones.size());
    IndexModelRestPoseJoints(_hierarchy->_rootBone, -1, _poseBoneJointIndexes, rest_pose_joints, indexed_joints, _fileName);

    for (size_t joint_index = 0; joint_index < indexed_joints.size(); joint_index++) {
        FO_VERIFY_AND_THROW(indexed_joints[joint_index] != 0, "Direct model pose bone is absent from the source hierarchy", _fileName, joint_index, _poseJointRuntimeNames[joint_index]);
    }

    _restPoseJoints = std::move(rest_pose_joints);
}

void ModelInformation::IndexAnimationPoseJoints(const ModelAnimationRuntimeRig& rig)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelAnimationRuntimeBaseJoints(rig, _hierarchy->_sourceJoints, _fileName);
    size_t canonical_joint_count = rig.GetJointCount();
    const_span<uint32_t> base_joint_mapping = rig.GetBaseJointMapping();
    _poseBones.assign(canonical_joint_count, nullptr);
    _poseJointCanonicalNames.resize(canonical_joint_count);
    _poseJointRuntimeNames.resize(canonical_joint_count);

    for (size_t joint_index = 0; joint_index < canonical_joint_count; joint_index++) {
        hstring canonical_name = _modelMngr->GetBoneHashedString(rig.GetJointName(joint_index));
        _poseJointCanonicalNames[joint_index] = canonical_name;
        _poseJointRuntimeNames[joint_index] = canonical_name;
    }

    FO_STRONG_ASSERT(base_joint_mapping.size() == _hierarchy->_sourceBones.size(), "Animation base-joint mapping does not match the physical source-bone map", _fileName, base_joint_mapping.size(), _hierarchy->_sourceBones.size());

    for (size_t source_index = 0; source_index < _hierarchy->_sourceBones.size(); source_index++) {
        uint32_t canonical_index = base_joint_mapping[source_index];
        FO_STRONG_ASSERT(canonical_index < _poseBones.size(), "Animation base-joint mapping index is outside the canonical pose", _fileName, source_index, canonical_index, _poseBones.size());
        FO_STRONG_ASSERT(!_poseBones[canonical_index], "Animation base-joint mapping resolves more than one physical bone to a canonical joint", _fileName, source_index, canonical_index);
        auto source_bone = _hierarchy->_sourceBones[source_index];
        _poseBones[canonical_index] = source_bone;
        _poseJointRuntimeNames[canonical_index] = source_bone->Name;
    }

    IndexPoseJointLookups();
    _restPoseJoints.clear();
}

void ModelInformation::IndexPoseJointLookups()
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_poseBones.size() == _poseJointCanonicalNames.size() && _poseBones.size() == _poseJointRuntimeNames.size(), "Model pose metadata sizes differ", _fileName, _poseBones.size(), _poseJointCanonicalNames.size(), _poseJointRuntimeNames.size());
    auto pose_joint_indexes = BuildModelPoseJointNameIndex(_poseJointRuntimeNames, _fileName);
    unordered_map<ptr<const ModelBone>, uint32_t> pose_bone_joint_indexes;
    pose_bone_joint_indexes.reserve(_hierarchy->_sourceBones.size());

    for (size_t joint_index = 0; joint_index < _poseBones.size(); joint_index++) {
        auto source_bone = _poseBones[joint_index];

        if (!source_bone) {
            continue;
        }

        FO_VERIFY_AND_THROW(source_bone->Name == _poseJointRuntimeNames[joint_index], "Physical model bone runtime name differs from canonical pose lookup metadata", _fileName, joint_index, source_bone->Name, _poseJointRuntimeNames[joint_index]);
        const auto [it, inserted] = pose_bone_joint_indexes.emplace(source_bone.as_ptr(), numeric_cast<uint32_t>(joint_index));
        ignore_unused(it);
        FO_VERIFY_AND_THROW(inserted, "Model pose contains the same physical bone more than once", _fileName, source_bone->Name, joint_index);
    }

    _poseJointIndexes = std::move(pose_joint_indexes);
    _poseBoneJointIndexes = std::move(pose_bone_joint_indexes);
    _bodyRotationJointIndex.reset();
    _headRotationJointIndex.reset();

    if (_rotationBone) {
        if (auto body_it = _poseJointIndexes.find(_rotationBone); body_it != _poseJointIndexes.end()) {
            _bodyRotationJointIndex = body_it->second;
        }
    }
    if (_modelMngr->_headBone) {
        if (auto head_it = _poseJointIndexes.find(_modelMngr->_headBone); head_it != _poseJointIndexes.end()) {
            _headRotationJointIndex = head_it->second;
        }
    }
}

auto ModelInformation::ReadBakedModelDescriptionLink(DataReader& reader, string_view context) const -> ModelInformation::BakedModelDescriptionLink
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionLink link;
    link.Data.Layer = reader.Read<int32_t>();
    link.Data.LayerValue = reader.Read<int32_t>();
    FO_VERIFY_AND_THROW(link.Data.Layer >= 0 && link.Data.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Model link layer is out of range", link.Data.Layer);

    string link_bone = reader.ReadString();
    link.Data.LinkBone = !link_bone.empty() ? _modelMngr->GetBoneHashedString(link_bone) : hstring {};
    link.Data.ChildName = reader.ReadString();
    link.Data.IsParticles = reader.Read<uint8_t>() != 0;
    link.Data.RotX = reader.Read<float32_t>();
    link.Data.RotY = reader.Read<float32_t>();
    link.Data.RotZ = reader.Read<float32_t>();
    link.Data.MoveX = reader.Read<float32_t>();
    link.Data.MoveY = reader.Read<float32_t>();
    link.Data.MoveZ = reader.Read<float32_t>();
    link.Data.ScaleX = reader.Read<float32_t>();
    link.Data.ScaleY = reader.Read<float32_t>();
    link.Data.ScaleZ = reader.Read<float32_t>();
    link.Data.SpeedAjust = reader.Read<float32_t>();
    FO_VERIFY_AND_THROW(link.Data.SpeedAjust >= 0.0f, "Model link speed adjust is negative");
    link.Data.DisabledLayer = reader.ReadSizedObjectVector<int32_t>();

    for (int32_t disabled_layer : link.Data.DisabledLayer) {
        FO_VERIFY_AND_THROW(disabled_layer >= 0 && disabled_layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Disabled model layer is out of range", disabled_layer);
    }

    uint32_t disabled_mesh_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, disabled_mesh_count, BAKED_STRING_MIN_SIZE, "disabled meshes", context);
    link.Data.DisabledMesh.reserve(disabled_mesh_count);

    for (uint32_t i = 0; i < disabled_mesh_count; i++) {
        string mesh_name = reader.ReadString();
        link.Data.DisabledMesh.emplace_back(!mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {});
    }

    uint32_t texture_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, texture_count, 2 * BAKED_STRING_MIN_SIZE + sizeof(int32_t), "texture overrides", context);
    link.Data.TextureInfo.reserve(texture_count);

    for (uint32_t i = 0; i < texture_count; i++) {
        string texture_name = reader.ReadString();
        string mesh_name = reader.ReadString();
        int32_t texture_index = reader.Read<int32_t>();
        FO_VERIFY_AND_THROW(texture_index >= 0 && texture_index < numeric_cast<int32_t>(MODEL_MAX_TEXTURES), "Texture index is out of range", texture_index);
        link.Data.TextureInfo.emplace_back(std::move(texture_name), !mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {}, texture_index);
    }

    uint32_t effect_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, effect_count, 2 * BAKED_STRING_MIN_SIZE, "effect overrides", context);
    link.Data.EffectInfo.reserve(effect_count);

    for (uint32_t i = 0; i < effect_count; i++) {
        string effect_name = reader.ReadString();
        string mesh_name = reader.ReadString();
        link.Data.EffectInfo.emplace_back(std::move(effect_name), !mesh_name.empty() ? _modelMngr->GetBoneHashedString(mesh_name) : hstring {});
    }

    uint32_t cut_count = reader.Read<uint32_t>();
    VerifyModelBakedCountFitsData(reader, cut_count, BAKED_MODEL_DESCRIPTION_CUT_MIN_SIZE, "cut entries", context);
    link.CutInfo.reserve(cut_count);

    for (uint32_t i = 0; i < cut_count; i++) {
        link.CutInfo.emplace_back(ReadBakedModelDescriptionCutInfo(reader));
    }

    return link;
}

auto ModelInformation::ReadBakedModelDescriptionCutInfo(DataReader& reader) const -> ModelInformation::BakedModelDescriptionCutInfo
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionCutInfo cut;
    cut.FileName = reader.ReadString();
    cut.Layers = reader.ReadSizedObjectVector<int32_t>();
    cut.Shapes = reader.ReadStringVector();
    cut.UnskinBone1 = reader.ReadString();
    cut.UnskinBone2 = reader.ReadString();
    cut.UnskinShape = reader.ReadString();
    cut.RevertUnskinShape = reader.Read<uint8_t>() != 0;
    return cut;
}

auto ModelInformation::ReadBakedModelDescriptionAnimationEntry(DataReader& reader) const -> ModelInformation::BakedModelDescriptionAnimationEntry
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionAnimationEntry anim_entry;
    anim_entry.StateAnim = reader.Read<int32_t>();
    anim_entry.ActionAnim = reader.Read<int32_t>();
    anim_entry.FileName = reader.ReadString();
    anim_entry.Name = reader.ReadString();
    return anim_entry;
}

auto ModelInformation::ReadBakedModelDescriptionAnimLayerValue(DataReader& reader) const -> ModelInformation::BakedModelDescriptionAnimLayerValue
{
    FO_STACK_TRACE_ENTRY();

    BakedModelDescriptionAnimLayerValue value;
    value.StateAnim = reader.Read<int32_t>();
    value.ActionAnim = reader.Read<int32_t>();
    value.Layer = reader.Read<int32_t>();
    value.LayerValue = reader.Read<int32_t>();
    FO_VERIFY_AND_THROW(value.Layer >= 0 && value.Layer < numeric_cast<int32_t>(MODEL_LAYERS_COUNT), "Animation layer is out of range", value.Layer);
    return value;
}

auto ModelInformation::GetAnimationIndex(CritterStateAnim& state_anim, CritterActionAnim& action_anim, nptr<float32_t> speed) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    int32_t anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);

    if (anim_index != -1) {
        return anim_index;
    }

    // Find substitute animation
    hstring base_model_name = _modelMngr->_engineMetadata->Hashes.ToHashedString(_fileName);
    auto base_state_anim = state_anim;
    auto base_action_anim = action_anim;

    while (anim_index == -1) {
        hstring model_name = base_model_name;
        auto state_anim_ = state_anim;
        auto action_anim_ = action_anim;

        if (_modelMngr->_animNameResolver->ResolveCritterAnimationSubstitute(base_model_name, base_state_anim, base_action_anim, model_name, state_anim, action_anim) && (state_anim != state_anim_ || action_anim != action_anim_)) {
            anim_index = GetAnimationIndexEx(state_anim, action_anim, speed);
        }
        else {
            break;
        }
    }

    return anim_index;
}

auto ModelInformation::GetAnimationIndexEx(CritterStateAnim state_anim, CritterActionAnim action_anim, nptr<float32_t> speed) const -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (auto it1 = _stateAnimEquals.find(state_anim); it1 != _stateAnimEquals.end()) {
        state_anim = it1->second;
    }
    if (auto it2 = _actionAnimEquals.find(action_anim); it2 != _actionAnimEquals.end()) {
        action_anim = it2->second;
    }

    auto index = std::make_pair(state_anim, action_anim);

    if (speed) {
        auto it = _animSpeed.find(index);

        if (it != _animSpeed.end()) {
            *speed = it->second;
        }
        else {
            *speed = 1.0f;
        }
    }

    if (auto it = _animIndexes.find(index); it != _animIndexes.end()) {
        return it->second;
    }

    return -1;
}

auto ModelInformation::CalculateHierarchyBounds() const -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_hierarchy, "Missing model hierarchy while calculating bounds", _fileName);

    optional<ModelBounds3D> bounds;

    for (ptr<ModelBone> bone : _hierarchy->_allDrawBones) {
        FO_VERIFY_AND_THROW(bone->AttachedMesh, "Drawable model bone has no mesh", bone->Name, _fileName);
        const MeshData& mesh = *bone->AttachedMesh;
        vector<bool> referenced_vertices(mesh.Vertices.size());

        for (vindex_t vertex_index : mesh.Indices) {
            FO_VERIFY_AND_THROW(numeric_cast<size_t>(vertex_index) < mesh.Vertices.size(), "Model mesh index is out of range while calculating bounds", vertex_index, mesh.Vertices.size(), _fileName);
            referenced_vertices[numeric_cast<size_t>(vertex_index)] = true;
        }

        FO_VERIFY_AND_THROW(mesh.SkinBones.size() == mesh.SkinBoneOffsets.size(), "Model skin bone and offset counts differ while calculating bounds", mesh.SkinBones.size(), mesh.SkinBoneOffsets.size(), _fileName);

        for (size_t vertex_index = 0; vertex_index < mesh.Vertices.size(); vertex_index++) {
            if (!referenced_vertices[vertex_index]) {
                continue;
            }

            const Vertex3D& vertex = mesh.Vertices[vertex_index];
            vec3 transformed_point;

            if (mesh.SkinBones.empty()) {
                glm::vec4 transformed = bone->GlobalTransformationMatrix * glm::vec4 {vertex.Position, 1.0f};
                FO_VERIFY_AND_THROW(is_float_equal(transformed.w, 1.0f), "Unskinned model vertex has invalid homogeneous coordinate", transformed.w, _fileName);
                transformed_point = vec3 {transformed};
            }
            else {
                glm::vec4 transformed {};
                float64_t weight_sum = 0.0;

                for (size_t influence = 0; influence < MODEL_BONES_PER_VERTEX; influence++) {
                    float32_t weight = vertex.BlendWeights[influence];

                    if (weight == 0.0f) {
                        continue;
                    }

                    float32_t raw_index = vertex.BlendIndices[influence];
                    int32_t index = iround<int32_t>(raw_index);
                    FO_VERIFY_AND_THROW(std::isfinite(weight) && weight > 0.0f && std::isfinite(raw_index) && index >= 0 && numeric_cast<size_t>(index) < mesh.SkinBones.size() && is_float_equal(raw_index, numeric_cast<float32_t>(index)), "Model vertex has invalid skin influence while calculating bounds", weight, raw_index, mesh.SkinBones.size(), _fileName);
                    nptr<ModelBone> skin_bone = mesh.SkinBones[numeric_cast<size_t>(index)];
                    FO_VERIFY_AND_THROW(skin_bone, "Model vertex references a missing skin bone while calculating bounds", index, _fileName);
                    transformed += skin_bone->GlobalTransformationMatrix * mesh.SkinBoneOffsets[numeric_cast<size_t>(index)] * glm::vec4 {vertex.Position, 1.0f} * weight;
                    weight_sum += weight;
                }

                FO_VERIFY_AND_THROW(std::abs(weight_sum - 1.0) <= MODEL_MESH_SKIN_WEIGHT_SUM_TOLERANCE, "Model vertex skin weights do not sum to one while calculating bounds", weight_sum, _fileName);
                FO_VERIFY_AND_THROW(is_float_equal(transformed.w, 1.0f), "Skinned model vertex has invalid homogeneous coordinate", transformed.w, _fileName);
                transformed_point = vec3 {transformed};
            }

            if (!IncludeModelBoundsPoint(bounds, transformed_point)) {
                return std::nullopt;
            }
        }
    }

    if (!bounds) {
        return std::nullopt;
    }

    return CalculateGuardedModelBounds(*bounds);
}

auto ModelInformation::CreateInstance() -> unique_ptr<ModelInstance>
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_hierarchy != nullptr, "Missing required hierarchy");
    FO_VERIFY_AND_THROW(IsValidModelBounds(_modelBounds), "Model has invalid bounds", _fileName, _modelBounds.Min.x, _modelBounds.Min.y, _modelBounds.Min.z, _modelBounds.Max.x, _modelBounds.Max.y, _modelBounds.Max.z);
    FO_VERIFY_AND_THROW(IsValidModelBounds(_viewBounds), "Model has invalid view bounds", _fileName, _viewBounds.Min.x, _viewBounds.Min.y, _viewBounds.Min.z, _viewBounds.Max.x, _viewBounds.Max.y, _viewBounds.Max.z);
    FO_VERIFY_AND_THROW(!_poseJointRuntimeNames.empty() && _poseJointRuntimeNames.size() == _poseJointCanonicalNames.size() && _poseJointRuntimeNames.size() == _poseBones.size(), "Model canonical pose metadata is incomplete", _fileName, _poseJointRuntimeNames.size(), _poseJointCanonicalNames.size(), _poseBones.size());
    FO_VERIFY_AND_THROW(_animationRuntimeRig || _restPoseJoints.size() == _poseJointRuntimeNames.size(), "Direct model rest-pose metadata count differs from its canonical pose", _fileName, _restPoseJoints.size(), _poseJointRuntimeNames.size());
    FO_VERIFY_AND_THROW(!_animController || _animationRuntimeRig, "Animated model has no animation runtime rig", _fileName);
    FO_VERIFY_AND_THROW(!_animController || _animIndexes.size() == numeric_cast<size_t>(_animController->GetAnimationsCount()), "Animation controller metadata count differs from its state/action index", _fileName, _animIndexes.size(), _animController ? _animController->GetAnimationsCount() : 0);

    auto model = SafeAlloc::MakeUnique<ModelInstance>(_modelMngr, this);

    if (_animationRuntimeRig) {
        model->_animationRuntimePose = SafeAlloc::MakeUnique<ModelAnimationRuntimePose>(_animationRuntimeRig);
    }

    if (_animController) {
        model->_bodyAnimController.emplace(_animController->Copy());

        if (_rotationBone) {
            model->_moveAnimController.emplace(_animController->Copy());
        }
    }

    return model;
}

auto ModelInformation::CreateCutShape(ptr<const MeshData> mesh) const -> ModelCutData::Shape
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!mesh->Vertices.empty(), "Cut mesh has no vertices");

    ModelCutData::Shape shape;
    shape.GlobalTransformationMatrix = mesh->Owner->GlobalTransformationMatrix;

    if (mesh->Vertices.size() != 36) {
        shape.IsSphere = true;

        // Evaluate sphere radius
        float32_t vmin = mesh->Vertices[0].Position.x;
        float32_t vmax = mesh->Vertices[0].Position.x;

        for (size_t i = 0; i < mesh->Vertices.size(); i++) {
            const Vertex3D& v = mesh->Vertices[i];
            vmin = std::min(v.Position.x, vmin);
            vmax = std::max(v.Position.x, vmax);
        }

        shape.SphereRadius = (vmax - vmin) / 2.0f;
    }
    else {
        shape.IsSphere = false;

        // Calculate bounding box size
        float32_t vmin[3];
        float32_t vmax[3];

        for (size_t i = 0, j = mesh->Vertices.size(); i < j; i++) {
            const Vertex3D& v = mesh->Vertices[i];

            if (i == 0) {
                vmin[0] = vmax[0] = v.Position.x;
                vmin[1] = vmax[1] = v.Position.y;
                vmin[2] = vmax[2] = v.Position.z;
            }
            else {
                vmin[0] = std::max(vmin[0], v.Position.x);
                vmin[1] = std::max(vmin[1], v.Position.y);
                vmin[2] = std::max(vmin[2], v.Position.z);
                vmax[0] = std::min(vmax[0], v.Position.x);
                vmax[1] = std::min(vmax[1], v.Position.y);
                vmax[2] = std::min(vmax[2], v.Position.z);
            }
        }

        shape.BBoxMin.x = vmin[0];
        shape.BBoxMin.y = vmin[1];
        shape.BBoxMin.z = vmin[2];
        shape.BBoxMax.x = vmax[0];
        shape.BBoxMax.y = vmax[1];
        shape.BBoxMax.z = vmax[2];
    }

    return shape;
}

FO_END_NAMESPACE

#endif
