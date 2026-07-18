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

#include "ModelSourceLoader.h"

#if FO_ENABLE_3D

#include "ModelAnimationData.h"
#include "ModelMeshData.h"

#include "ufbx.h"

FO_BEGIN_NAMESPACE

struct ModelSourceValidationContext
{
    string_view FileName {};
    string_view ClipName {};
    string_view NodeName {};
    string_view FieldName {};
    size_t ElementIndex {};
};

static auto ConvertModelSourceString(const ufbx_string& value, const ModelSourceValidationContext& context) -> string;
static auto ConvertModelSourceFloat(double value, const ModelSourceValidationContext& context, string_view component) -> float32_t;
static auto ConvertModelSourceVec3(const ufbx_vec3& value, const ModelSourceValidationContext& context) -> vec3;
static auto ConvertModelSourceQuaternion(const ufbx_quat& value, const ModelSourceValidationContext& context) -> quaternion;
static auto ConvertModelSourceMatrix(const ufbx_matrix& value, const ModelSourceValidationContext& context) -> mat44;
static void AppendModelSourceSkeletonJoint(ptr<const ufbx_node> node, const vector<string>& parent_hierarchy, ModelSkeletonSource& skeleton);
static auto ExtractModelSourceAnimations(ptr<const ufbx_scene> scene, string_view path) -> vector<ModelAnimationSource>;
static auto ExtractModelSourceAnimation(ptr<const ufbx_scene> scene, ptr<const ufbx_anim_stack> anim_stack, string_view path) -> ModelAnimationSource;
static auto BuildModelSourceHierarchy(ptr<const ufbx_node> node, string_view path, string_view clip_name) -> vector<string>;
static void ValidateModelSourceName(string_view value, string_view context, bool allow_empty);
static void ValidateModelSourceAnimation(const ModelAnimationSource& animation, const set<vector<string>>& skeleton_hierarchies);
static void ValidateModelSourceVec3Track(const ModelAnimationVec3Track& track, string_view source_file, string_view clip_name, string_view output_name, string_view track_name);
static void ValidateModelSourceQuaternionTrack(const ModelAnimationQuaternionTrack& track, string_view source_file, string_view clip_name, string_view output_name);
static void ValidateModelSourceTimes(const vector<float32_t>& times, string_view source_file, string_view clip_name, string_view output_name, string_view track_name);

auto LoadModelSourceAsset(string_view path, const File& file) -> ModelSourceAsset
{
    FO_STACK_TRACE_ENTRY();

    if (path.empty()) {
        throw ModelSourceLoaderException("Can't load a model source with an empty path");
    }
    if (!file) {
        throw ModelSourceLoaderException("Model source is not readable", path);
    }
    if (file.GetPath() != path) {
        throw ModelSourceLoaderException("Model source path does not match loaded file", path, file.GetPath());
    }

    ufbx_load_opts opts = {};
    opts.ignore_embedded = true;
    opts.evaluate_skinning = true;
    opts.ignore_missing_external_files = true;
    opts.clean_skin_weights = true;
    opts.generate_missing_normals = true;
    opts.normalize_normals = true;
    opts.normalize_tangents = true;

    ufbx_error fbx_error = {};
    const_span<uint8_t> file_data = file.GetDataSpan();
    auto file_data_bytes = make_nptr(file_data.data());
    FO_VERIFY_AND_THROW(file_data.empty() || file_data_bytes, "Non-empty model source data has a null buffer pointer", path);
    auto scene = make_nptr(ufbx_load_memory(file_data_bytes.get(), file_data.size(), &opts, &fbx_error));

    if (!scene) {
        throw ModelSourceLoaderException("Unable to load model source", path, fbx_error.description.data);
    }

    auto scene_holder = make_unique_del_ptr(scene, [](ufbx_scene* raw_scene) noexcept { ufbx_free_scene(raw_scene); });
    FO_VERIFY_AND_THROW(scene->root_node, "Loaded model source has no root node", path);

    ModelSourceAsset result;
    result.FileName = path;
    result.WriteTime = file.GetWriteTime();
    result.Skeleton.FileName = path;
    AppendModelSourceSkeletonJoint(scene->root_node, {}, result.Skeleton);
    result.Animations = ExtractModelSourceAnimations(scene, path);
    ValidateModelSourceAsset(result);
    return result;
}

void ValidateModelSourceAsset(const ModelSourceAsset& asset)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelSourceName(asset.FileName, "model source file name", false);

    if (asset.Skeleton.FileName != asset.FileName) {
        throw ModelSourceLoaderException("Model source declares mismatched skeleton source", asset.FileName, asset.Skeleton.FileName);
    }
    if (asset.Skeleton.Joints.empty()) {
        throw ModelSourceLoaderException("Model source has no skeleton joints", asset.FileName);
    }
    if (asset.Skeleton.Joints.size() > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelSourceLoaderException("Model source has too many skeleton joints", asset.FileName, asset.Skeleton.Joints.size(), MODEL_ANIMATION_RIG_MAX_JOINTS);
    }
    if (asset.Animations.size() > MODEL_ANIMATION_RIG_MAX_CLIPS) {
        throw ModelSourceLoaderException("Model source has too many animation clips", asset.FileName, asset.Animations.size(), MODEL_ANIMATION_RIG_MAX_CLIPS);
    }

    set<vector<string>> skeleton_hierarchies;
    map<string, vector<string>> hierarchy_by_name;
    vector<string> root_hierarchy;

    for (const ModelSkeletonJoint& joint : asset.Skeleton.Joints) {
        ValidateModelSourceName(joint.Name, strex("skeleton joint in '{}'", asset.FileName), joint.Hierarchy.size() == 1);

        if (joint.Hierarchy.empty()) {
            throw ModelSourceLoaderException("Model source contains a joint with an empty hierarchy", asset.FileName, joint.Name);
        }
        if (joint.Hierarchy.back() != joint.Name) {
            throw ModelSourceLoaderException("Model source contains a joint whose hierarchy ends with a different name", asset.FileName, joint.Name, joint.Hierarchy.back());
        }

        if (joint.Hierarchy.size() > MODEL_MESH_MAX_HIERARCHY_DEPTH) {
            throw ModelSourceLoaderException("Model source joint hierarchy is too deep", asset.FileName, joint.Name, joint.Hierarchy.size(), MODEL_MESH_MAX_HIERARCHY_DEPTH);
        }

        for (size_t i = 0; i < joint.Hierarchy.size(); i++) {
            ValidateModelSourceName(joint.Hierarchy[i], strex("skeleton hierarchy in '{}'", asset.FileName), i == 0);
        }

        for (mat44::length_type column = 0; column < 4; column++) {
            for (mat44::length_type row = 0; row < 4; row++) {
                if (!std::isfinite(joint.RestLocalTransform[column][row])) {
                    throw ModelSourceLoaderException("Model source contains a non-finite rest transform at a joint", asset.FileName, joint.Name);
                }
            }
        }

        if (!skeleton_hierarchies.emplace(joint.Hierarchy).second) {
            throw ModelSourceLoaderException("Model source has a duplicate joint hierarchy", asset.FileName, joint.Name);
        }

        const auto [name_it, name_inserted] = hierarchy_by_name.emplace(joint.Name, joint.Hierarchy);

        if (!name_inserted) {
            throw ModelSourceLoaderException("Model source has a duplicate joint name", asset.FileName, name_it->first);
        }

        if (joint.Hierarchy.size() == 1) {
            if (!root_hierarchy.empty()) {
                throw ModelSourceLoaderException("Model source has multiple root joints", asset.FileName, root_hierarchy.front(), joint.Name);
            }

            root_hierarchy = joint.Hierarchy;
        }
    }

    if (root_hierarchy.empty()) {
        throw ModelSourceLoaderException("Model source has no root joint", asset.FileName);
    }

    for (const ModelSkeletonJoint& joint : asset.Skeleton.Joints) {
        if (joint.Hierarchy.size() <= 1) {
            continue;
        }

        const vector<string> parent_hierarchy(joint.Hierarchy.begin(), joint.Hierarchy.end() - 1);

        if (skeleton_hierarchies.count(parent_hierarchy) == 0) {
            throw ModelSourceLoaderException("Model source has a joint without a parent hierarchy", asset.FileName, joint.Name);
        }
    }

    for (size_t i = 0; i < asset.Animations.size(); i++) {
        const ModelAnimationSource& animation = asset.Animations[i];

        if (animation.FileName != asset.FileName) {
            throw ModelSourceLoaderException("Animation in model source declares a mismatched source file", animation.Name, asset.FileName, animation.FileName);
        }

        for (size_t j = 0; j < i; j++) {
            if (strex(asset.Animations[j].Name).compare_ignore_case(animation.Name)) {
                throw ModelSourceLoaderException("Model source contains case-insensitive duplicate animation names", asset.FileName, asset.Animations[j].Name, animation.Name);
            }
        }

        ValidateModelSourceAnimation(animation, skeleton_hierarchies);
    }
}

ModelSourceAssetCache::ModelSourceAssetCache(const FileCollection& files, LoadCallback load_callback) :
    _files {&files},
    _loadCallback {std::move(load_callback)}
{
    FO_STACK_TRACE_ENTRY();
}

auto ModelSourceAssetCache::Get(string_view path) const -> shared_ptr<const ModelSourceAsset>
{
    FO_STACK_TRACE_ENTRY();

    if (path.empty()) {
        throw ModelSourceLoaderException("Can't cache a model source with an empty path");
    }

    using AssetPtr = shared_ptr<const ModelSourceAsset>;
    auto promise = SafeAlloc::MakeShared<std::promise<AssetPtr>>();
    const PendingAsset pending = promise->get_future().share();
    PendingAsset load;
    bool owner = false;

    {
        const scoped_lock locker {_mutex};
        const auto [it, inserted] = _loads.emplace(string(path), pending);
        load = it->second;
        owner = inserted;
    }

    if (owner) {
        try {
            const File file = _files->FindFileByPath(path);

            if (!file) {
                throw ModelSourceLoaderException("Model source was not found", path);
            }

            ModelSourceAsset asset = _loadCallback ? _loadCallback(path, file) : LoadModelSourceAsset(path, file);

            if (asset.FileName != path) {
                throw ModelSourceLoaderException("Model source cache load returned a mismatched asset", path, asset.FileName);
            }
            if (asset.WriteTime != file.GetWriteTime()) {
                throw ModelSourceLoaderException("Model source cache load returned an unexpected write time", path, asset.WriteTime, file.GetWriteTime());
            }

            ValidateModelSourceAsset(asset);
            promise->set_value(SafeAlloc::MakeShared<ModelSourceAsset>(std::move(asset)));
        }
        catch (...) {
            promise->set_exception(std::current_exception());
        }
    }

    return load.get();
}

static auto ConvertModelSourceString(const ufbx_string& value, const ModelSourceValidationContext& context) -> string
{
    FO_NO_STACK_TRACE_ENTRY();

    auto data = make_nptr(value.data);

    if (value.length != 0 && !data) {
        throw ModelSourceLoaderException("Model source has a null string", context.FileName, context.ClipName, context.NodeName, context.FieldName, context.ElementIndex);
    }

    return value.length != 0 ? string {data.get(), value.length} : string {};
}

static auto ConvertModelSourceFloat(double value, const ModelSourceValidationContext& context, string_view component) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    constexpr double min_float = static_cast<double>(std::numeric_limits<float32_t>::lowest());
    constexpr double max_float = static_cast<double>(std::numeric_limits<float32_t>::max());

    if (!std::isfinite(value) || value < min_float || value > max_float) {
        throw ModelSourceLoaderException("Model source has numeric data that is not representable as a finite float", context.FileName, context.ClipName, context.NodeName, context.FieldName, context.ElementIndex, component, value);
    }

    return numeric_cast<float32_t>(value);
}

static auto ConvertModelSourceVec3(const ufbx_vec3& value, const ModelSourceValidationContext& context) -> vec3
{
    FO_NO_STACK_TRACE_ENTRY();

    return vec3 {
        ConvertModelSourceFloat(value.x, context, "x"),
        ConvertModelSourceFloat(value.y, context, "y"),
        ConvertModelSourceFloat(value.z, context, "z"),
    };
}

static auto ConvertModelSourceQuaternion(const ufbx_quat& value, const ModelSourceValidationContext& context) -> quaternion
{
    FO_NO_STACK_TRACE_ENTRY();

    quaternion result;
    result.x = ConvertModelSourceFloat(value.x, context, "x");
    result.y = ConvertModelSourceFloat(value.y, context, "y");
    result.z = ConvertModelSourceFloat(value.z, context, "z");
    result.w = ConvertModelSourceFloat(value.w, context, "w");
    return result;
}

static auto ConvertModelSourceMatrix(const ufbx_matrix& value, const ModelSourceValidationContext& context) -> mat44
{
    FO_NO_STACK_TRACE_ENTRY();

    mat44 result {1.0f};
    result[0][0] = ConvertModelSourceFloat(value.m00, context, "m00");
    result[1][0] = ConvertModelSourceFloat(value.m01, context, "m01");
    result[2][0] = ConvertModelSourceFloat(value.m02, context, "m02");
    result[3][0] = ConvertModelSourceFloat(value.m03, context, "m03");
    result[0][1] = ConvertModelSourceFloat(value.m10, context, "m10");
    result[1][1] = ConvertModelSourceFloat(value.m11, context, "m11");
    result[2][1] = ConvertModelSourceFloat(value.m12, context, "m12");
    result[3][1] = ConvertModelSourceFloat(value.m13, context, "m13");
    result[0][2] = ConvertModelSourceFloat(value.m20, context, "m20");
    result[1][2] = ConvertModelSourceFloat(value.m21, context, "m21");
    result[2][2] = ConvertModelSourceFloat(value.m22, context, "m22");
    result[3][2] = ConvertModelSourceFloat(value.m23, context, "m23");
    result[0][3] = 0.0f;
    result[1][3] = 0.0f;
    result[2][3] = 0.0f;
    result[3][3] = 1.0f;
    return result;
}

static void AppendModelSourceSkeletonJoint(ptr<const ufbx_node> node, const vector<string>& parent_hierarchy, ModelSkeletonSource& skeleton)
{
    FO_STACK_TRACE_ENTRY();

    if (parent_hierarchy.size() >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
        throw ModelSourceLoaderException("Model source hierarchy depth exceeds the joint limit", skeleton.FileName, MODEL_MESH_MAX_HIERARCHY_DEPTH, node->name.data);
    }
    if (skeleton.Joints.size() >= MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelSourceLoaderException("Model source exceeds the skeleton joint limit", skeleton.FileName, MODEL_ANIMATION_RIG_MAX_JOINTS);
    }

    const string node_name = ConvertModelSourceString(node->name, ModelSourceValidationContext {.FileName = skeleton.FileName, .NodeName = "<hierarchy>", .FieldName = "node_name", .ElementIndex = skeleton.Joints.size()});
    vector<string> hierarchy = parent_hierarchy;
    hierarchy.emplace_back(node_name);
    const mat44 rest_transform = ConvertModelSourceMatrix(node->node_to_parent, ModelSourceValidationContext {.FileName = skeleton.FileName, .NodeName = node_name, .FieldName = "node_to_parent", .ElementIndex = skeleton.Joints.size()});
    skeleton.Joints.emplace_back(ModelSkeletonJoint {.Name = node_name, .Hierarchy = hierarchy, .RestLocalTransform = rest_transform});

    for (size_t i = 0; i < node->children.count; i++) {
        auto child = make_nptr(node->children[i]);
        FO_VERIFY_AND_THROW(child, "Model source hierarchy contains a null child node", skeleton.FileName, node_name, i);
        AppendModelSourceSkeletonJoint(child, hierarchy, skeleton);
    }
}

static auto ExtractModelSourceAnimations(ptr<const ufbx_scene> scene, string_view path) -> vector<ModelAnimationSource>
{
    FO_STACK_TRACE_ENTRY();

    if (scene->anim_stacks.count > MODEL_ANIMATION_RIG_MAX_CLIPS) {
        throw ModelSourceLoaderException("Model source has too many animation stacks", path, scene->anim_stacks.count, MODEL_ANIMATION_RIG_MAX_CLIPS);
    }

    vector<ModelAnimationSource> result;
    result.reserve(scene->anim_stacks.count);

    for (ptr<const ufbx_anim_stack> anim_stack : scene->anim_stacks) {
        result.emplace_back(ExtractModelSourceAnimation(scene, anim_stack, path));
    }

    return result;
}

static auto ExtractModelSourceAnimation(ptr<const ufbx_scene> scene, ptr<const ufbx_anim_stack> anim_stack, string_view path) -> ModelAnimationSource
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(anim_stack->anim, "Model source animation stack has no animation", path);
    ufbx_bake_opts bake_opts = {};
    bake_opts.trim_start_time = true;
    ufbx_error bake_error = {};
    auto baked_anim = make_nptr(ufbx_bake_anim(scene.get(), anim_stack->anim, &bake_opts, &bake_error));
    const string clip_name = ConvertModelSourceString(anim_stack->name, ModelSourceValidationContext {.FileName = path, .FieldName = "animation_stack_name"});

    if (!baked_anim) {
        throw ModelSourceLoaderException("Unable to bake animation from model source", clip_name, path, bake_error.description.data);
    }

    auto baked_anim_holder = make_unique_del_ptr(baked_anim, [](ufbx_baked_anim* raw_anim) noexcept { ufbx_free_baked_anim(raw_anim); });

    if (baked_anim->nodes.count > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelSourceLoaderException("Animation from model source has too many baked nodes", clip_name, path, baked_anim->nodes.count, MODEL_ANIMATION_RIG_MAX_JOINTS);
    }

    ModelAnimationSource result;
    result.FileName = path;
    result.Name = clip_name;
    result.Duration = ConvertModelSourceFloat(baked_anim->playback_duration, ModelSourceValidationContext {.FileName = path, .ClipName = clip_name, .FieldName = "playback_duration"}, "time");
    result.Joints.reserve(baked_anim->nodes.count);

    for (const ufbx_baked_node& baked_node : baked_anim->nodes) {
        FO_VERIFY_AND_THROW(baked_node.typed_id < scene->nodes.count, "Baked model source animation node id is outside the scene node table", path, clip_name, baked_node.typed_id, scene->nodes.count);
        auto node = make_nptr(scene->nodes[baked_node.typed_id]);
        FO_VERIFY_AND_THROW(node, "Baked model source animation node is null", path, clip_name, baked_node.typed_id);
        const string node_name = ConvertModelSourceString(node->name, ModelSourceValidationContext {.FileName = path, .ClipName = clip_name, .FieldName = "node_name", .ElementIndex = baked_node.typed_id});
        ModelAnimationJointSource joint;
        joint.OutputName = node_name;
        joint.Hierarchy = BuildModelSourceHierarchy(node, path, clip_name);

        if (baked_node.translation_keys.count > std::numeric_limits<uint16_t>::max() || baked_node.rotation_keys.count > std::numeric_limits<uint16_t>::max() || baked_node.scale_keys.count > std::numeric_limits<uint16_t>::max()) {
            throw ModelSourceLoaderException("Animation node exceeds the per-track key limit", clip_name, path, node_name, std::numeric_limits<uint16_t>::max(), baked_node.translation_keys.count, baked_node.rotation_keys.count, baked_node.scale_keys.count);
        }

        joint.Translation.Times.reserve(baked_node.translation_keys.count);
        joint.Translation.Values.reserve(baked_node.translation_keys.count);
        joint.Rotation.Times.reserve(baked_node.rotation_keys.count);
        joint.Rotation.Values.reserve(baked_node.rotation_keys.count);
        joint.Scale.Times.reserve(baked_node.scale_keys.count);
        joint.Scale.Values.reserve(baked_node.scale_keys.count);

        for (size_t i = 0; i < baked_node.translation_keys.count; i++) {
            const ufbx_baked_vec3& key = baked_node.translation_keys[i];
            const ModelSourceValidationContext context {.FileName = path, .ClipName = clip_name, .NodeName = node_name, .FieldName = "translation", .ElementIndex = i};
            joint.Translation.Times.emplace_back(ConvertModelSourceFloat(key.time, context, "time"));
            joint.Translation.Values.emplace_back(ConvertModelSourceVec3(key.value, context));
        }
        for (size_t i = 0; i < baked_node.rotation_keys.count; i++) {
            const ufbx_baked_quat& key = baked_node.rotation_keys[i];
            const ModelSourceValidationContext context {.FileName = path, .ClipName = clip_name, .NodeName = node_name, .FieldName = "rotation", .ElementIndex = i};
            joint.Rotation.Times.emplace_back(ConvertModelSourceFloat(key.time, context, "time"));
            joint.Rotation.Values.emplace_back(ConvertModelSourceQuaternion(key.value, context));
        }
        for (size_t i = 0; i < baked_node.scale_keys.count; i++) {
            const ufbx_baked_vec3& key = baked_node.scale_keys[i];
            const ModelSourceValidationContext context {.FileName = path, .ClipName = clip_name, .NodeName = node_name, .FieldName = "scale", .ElementIndex = i};
            joint.Scale.Times.emplace_back(ConvertModelSourceFloat(key.time, context, "time"));
            joint.Scale.Values.emplace_back(ConvertModelSourceVec3(key.value, context));
        }

        result.Joints.emplace_back(std::move(joint));
    }

    return result;
}

static auto BuildModelSourceHierarchy(ptr<const ufbx_node> node, string_view path, string_view clip_name) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> result;
    nptr<const ufbx_node> hierarchy_node = node;

    while (hierarchy_node) {
        if (result.size() >= MODEL_MESH_MAX_HIERARCHY_DEPTH) {
            throw ModelSourceLoaderException("Animation has a hierarchy deeper than the maximum", clip_name, path, MODEL_MESH_MAX_HIERARCHY_DEPTH, node->name.data);
        }

        result.emplace_back(ConvertModelSourceString(hierarchy_node->name, ModelSourceValidationContext {.FileName = path, .ClipName = clip_name, .NodeName = node->name.data, .FieldName = "hierarchy", .ElementIndex = result.size()}));
        hierarchy_node = hierarchy_node->parent;
    }

    std::ranges::reverse(result);
    return result;
}

static void ValidateModelSourceName(string_view value, string_view context, bool allow_empty)
{
    FO_STACK_TRACE_ENTRY();

    if (!allow_empty && value.empty()) {
        throw ModelSourceLoaderException("Empty name", context);
    }
    if (value.find('\0') != string_view::npos) {
        throw ModelSourceLoaderException("Embedded NUL in name", context);
    }
    if (!strvex(value).is_valid_utf8()) {
        throw ModelSourceLoaderException("Invalid UTF-8 in name", context);
    }
}

static void ValidateModelSourceAnimation(const ModelAnimationSource& animation, const set<vector<string>>& skeleton_hierarchies)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelSourceName(animation.Name, strex("animation name in '{}'", animation.FileName), false);

    if (!std::isfinite(animation.Duration) || animation.Duration <= 0.0f || !std::isfinite(1.0f / animation.Duration)) {
        throw ModelSourceLoaderException("Animation has an invalid duration", animation.Name, animation.FileName, animation.Duration);
    }
    if (animation.Joints.size() > MODEL_ANIMATION_RIG_MAX_JOINTS) {
        throw ModelSourceLoaderException("Animation has too many joint outputs", animation.Name, animation.FileName, animation.Joints.size(), MODEL_ANIMATION_RIG_MAX_JOINTS);
    }

    set<vector<string>> seen_hierarchies;
    set<string> seen_outputs;

    for (size_t i = 0; i < animation.Joints.size(); i++) {
        const ModelAnimationJointSource& joint = animation.Joints[i];
        ValidateModelSourceName(joint.OutputName, strex("animation '{}#{}' output {}", animation.FileName, animation.Name, i), joint.Hierarchy.size() == 1);

        if (joint.Hierarchy.empty()) {
            throw ModelSourceLoaderException("Animation has an empty joint hierarchy at an output", animation.Name, animation.FileName, i);
        }
        if (joint.Hierarchy.size() > MODEL_MESH_MAX_HIERARCHY_DEPTH) {
            throw ModelSourceLoaderException("Animation joint hierarchy has too many names", animation.Name, animation.FileName, joint.Hierarchy.size(), i, MODEL_MESH_MAX_HIERARCHY_DEPTH);
        }

        for (size_t j = 0; j < joint.Hierarchy.size(); j++) {
            ValidateModelSourceName(joint.Hierarchy[j], strex("animation '{}#{}' hierarchy {}", animation.FileName, animation.Name, i), j == 0);
        }

        if (joint.Hierarchy.back() != joint.OutputName) {
            throw ModelSourceLoaderException("Animation output does not match its hierarchy leaf", animation.Name, animation.FileName, joint.OutputName, joint.Hierarchy.back(), i);
        }
        if (!seen_hierarchies.emplace(joint.Hierarchy).second) {
            throw ModelSourceLoaderException("Animation has a duplicate joint hierarchy at an output", animation.Name, animation.FileName, i);
        }
        if (!seen_outputs.emplace(joint.OutputName).second) {
            throw ModelSourceLoaderException("Animation has a duplicate output", animation.Name, animation.FileName, joint.OutputName);
        }
        if (skeleton_hierarchies.count(joint.Hierarchy) == 0) {
            throw ModelSourceLoaderException("Animation output references a hierarchy absent from its source skeleton", animation.Name, animation.FileName, joint.OutputName);
        }

        ValidateModelSourceVec3Track(joint.Translation, animation.FileName, animation.Name, joint.OutputName, "translation");
        ValidateModelSourceQuaternionTrack(joint.Rotation, animation.FileName, animation.Name, joint.OutputName);
        ValidateModelSourceVec3Track(joint.Scale, animation.FileName, animation.Name, joint.OutputName, "scale");
    }
}

static void ValidateModelSourceVec3Track(const ModelAnimationVec3Track& track, string_view source_file, string_view clip_name, string_view output_name, string_view track_name)
{
    FO_STACK_TRACE_ENTRY();

    if (track.Times.empty() || track.Times.size() != track.Values.size()) {
        throw ModelSourceLoaderException("Animation output has invalid track sizes", source_file, clip_name, output_name, track_name, track.Times.size(), track.Values.size());
    }
    if (track.Times.size() > std::numeric_limits<uint16_t>::max()) {
        throw ModelSourceLoaderException("Animation output has too many track keys", source_file, clip_name, output_name, track.Times.size(), track_name, std::numeric_limits<uint16_t>::max());
    }

    ValidateModelSourceTimes(track.Times, source_file, clip_name, output_name, track_name);

    for (size_t i = 0; i < track.Values.size(); i++) {
        const vec3& value = track.Values[i];

        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
            throw ModelSourceLoaderException("Animation output has a non-finite track value", source_file, clip_name, output_name, track_name, i);
        }
        if (float_abs(value.x) > MODEL_ANIMATION_HALF_MAX || float_abs(value.y) > MODEL_ANIMATION_HALF_MAX || float_abs(value.z) > MODEL_ANIMATION_HALF_MAX) {
            throw ModelSourceLoaderException("Animation output has a track value outside the ozz FP16 range", source_file, clip_name, output_name, track_name, MODEL_ANIMATION_HALF_MAX, i, value.x, value.y, value.z);
        }
    }
}

static void ValidateModelSourceQuaternionTrack(const ModelAnimationQuaternionTrack& track, string_view source_file, string_view clip_name, string_view output_name)
{
    FO_STACK_TRACE_ENTRY();

    if (track.Times.empty() || track.Times.size() != track.Values.size()) {
        throw ModelSourceLoaderException("Animation output has invalid rotation track sizes", source_file, clip_name, output_name, track.Times.size(), track.Values.size());
    }
    if (track.Times.size() > std::numeric_limits<uint16_t>::max()) {
        throw ModelSourceLoaderException("Animation output has too many rotation keys", source_file, clip_name, output_name, track.Times.size(), std::numeric_limits<uint16_t>::max());
    }

    ValidateModelSourceTimes(track.Times, source_file, clip_name, output_name, "rotation");

    for (size_t i = 0; i < track.Values.size(); i++) {
        const quaternion& value = track.Values[i];

        if (!std::isfinite(value.w) || !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
            throw ModelSourceLoaderException("Animation output has a non-finite rotation value", source_file, clip_name, output_name, i);
        }

        const float32_t norm_squared = glm::dot(value, value);

        if (!std::isfinite(norm_squared) || norm_squared <= std::numeric_limits<float32_t>::epsilon()) {
            throw ModelSourceLoaderException("Animation output has a zero rotation value", source_file, clip_name, output_name, i);
        }
        if (float_abs(norm_squared - 1.0f) > MODEL_ANIMATION_QUATERNION_NORM_TOLERANCE) {
            throw ModelSourceLoaderException("Animation output has a non-unit rotation value", source_file, clip_name, output_name, i, norm_squared);
        }
    }
}

static void ValidateModelSourceTimes(const vector<float32_t>& times, string_view source_file, string_view clip_name, string_view output_name, string_view track_name)
{
    FO_STACK_TRACE_ENTRY();

    for (size_t i = 0; i < times.size(); i++) {
        if (!std::isfinite(times[i])) {
            throw ModelSourceLoaderException("Animation output has a non-finite track time", source_file, clip_name, output_name, track_name, i);
        }
        if (i != 0 && times[i] <= times[i - 1]) {
            throw ModelSourceLoaderException("Animation output has non-ascending track times", source_file, clip_name, output_name, track_name, i - 1, i);
        }
        if (i != 0) {
            const float32_t delta = times[i] - times[i - 1];

            if (!std::isfinite(delta) || !std::isfinite(1.0f / delta)) {
                throw ModelSourceLoaderException("Animation output has an invalid track time interval", source_file, clip_name, output_name, track_name, i - 1, i, times[i - 1], times[i]);
            }
        }
    }
}

FO_END_NAMESPACE

#endif
