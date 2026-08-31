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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <aka.cvet@gmail.com>
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

#include "ModelInfoBaker.h"

#if FO_ENABLE_3D

#include "Application.h"
#include "ModelAnimationConverter.h"
#include "ModelBoundsCalculator.h"
#include "ModelMeshData.h"

FO_BEGIN_NAMESPACE

struct BakerModelDescriptionCut
{
    void Save(DataWriter& writer) const;

    string FileName {};
    vector<int32_t> Layers {};
    vector<string> Shapes {};
    string UnskinBone1 {};
    string UnskinBone2 {};
    string UnskinShape {};
    bool RevertUnskinShape {};
};

struct BakerModelDescriptionLink
{
    void Save(DataWriter& writer) const;

    int32_t Layer {};
    int32_t LayerValue {};
    string LinkBone {};
    string ChildName {};
    bool IsParticles {};
    float32_t RotX {};
    float32_t RotY {};
    float32_t RotZ {};
    float32_t MoveX {};
    float32_t MoveY {};
    float32_t MoveZ {};
    float32_t ScaleX {};
    float32_t ScaleY {};
    float32_t ScaleZ {};
    float32_t SpeedAjust {};
    vector<int32_t> DisabledLayer {};
    vector<string> DisabledMesh {};
    vector<tuple<string, string, int32_t>> TextureInfo {};
    vector<pair<string, string>> EffectInfo {};
    vector<BakerModelDescriptionCut> CutInfo {};
    optional<ModelBounds3D> Bounds {};
};

struct BakerModelDescriptionAnimationEntry
{
    void Save(DataWriter& writer) const;

    int32_t StateAnim {};
    int32_t ActionAnim {};
    string FileName {};
    string Name {};
};

struct BakerModelDescriptionAnimLayerValue
{
    void Save(DataWriter& writer) const;

    int32_t StateAnim {};
    int32_t ActionAnim {};
    int32_t Layer {};
    int32_t LayerValue {};
};

struct BakerModelDescription
{
    void Save(DataWriter& writer) const;

    string Model {};
    bool DisableAnimationInterpolation {};
    bool DisableBackwardAnim {};
    bool ShadowDisabled {};
    string RotationBone {};
    BakerModelDescriptionLink DefaultLink {};
    vector<BakerModelDescriptionLink> Links {};
    vector<BakerModelDescriptionAnimationEntry> AnimationEntries {};
    unordered_set<string> AnimationGeometryExceptions {};
    vector<pair<pair<int32_t, int32_t>, float32_t>> AnimSpeed {};
    vector<BakerModelDescriptionAnimLayerValue> AnimLayerValues {};
    vector<string> FastTransitionBones {};
    vector<pair<int32_t, int32_t>> StateAnimEquals {};
    vector<pair<int32_t, int32_t>> ActionAnimEquals {};
};

struct ModelDescriptionParseState
{
    string Mesh {};
    int32_t Layer {-1};
    int32_t LayerValue {};
    BakerModelDescriptionLink DummyLink {};
    nptr<BakerModelDescriptionLink> Link {};
};

static auto ModelDescriptionLinkPtr(BakerModelDescriptionLink& link) noexcept -> ptr<BakerModelDescriptionLink>
{
    FO_NO_STACK_TRACE_ENTRY();

    return &link;
}

class ModelDescriptionParser final
{
public:
    ModelDescriptionParser(ptr<const FileCollection> files, ptr<const NameResolver> name_resolver);

    auto Parse(string_view fname) -> pair<BakerModelDescription, uint64_t>;

private:
    enum class AssignMode : uint8_t
    {
        Set,
        Add,
        Mul,
    };

    void ParseFile(string_view fname, const vector<pair<string, string>>& replacements, BakerModelDescription& description, ModelDescriptionParseState& state);
    void ParseContent(string_view fname, const string& content, BakerModelDescription& description, ModelDescriptionParseState& state);
    void ParseToken(string_view fname, size_t line, string_view token, const vector<string>& tokens, size_t& index, BakerModelDescription& description, ModelDescriptionParseState& state);
    static void ApplyFloatValue(BakerModelDescriptionLink& link, string_view field, float32_t value, AssignMode mode);

    ptr<const FileCollection> _files;
    ptr<const NameResolver> _nameResolver;
    vector<string> _includeStack {};
    uint64_t _maxWriteTime {};
};

struct BakedModelMeshInfo
{
    string FileName {};
    uint64_t WriteTime {};
    unordered_set<string> Bones {};
    unordered_set<string> DrawBones {};
    size_t DrawBonesCount {};
    vector<string> DiffuseTextures {};
    vector<string> SkinBoneRefs {};
    ModelSkeletonSource Skeleton {};
    optional<ModelBounds3D> StaticBounds {};
};

struct ValidatedModelAnimations
{
    vector<ModelAnimationSource> Sources {};
    vector<ModelAnimationRigBindingSource> Bindings {};
};

struct ValidatedModelDescription
{
    ModelSkeletonCompatibilityReport CompatibilityReport {};
    ModelAnimationRigData AnimationRigData {};
};

struct ModelAnimationInfoBakingStats
{
    uint64_t ModelSections {};
    uint64_t ModelBounds {};
    uint64_t AnimationEntries {};
    uint64_t AnimationBounds {};
    uint64_t BoundsCalculations {};
    uint64_t BoundsCacheHits {};
    uint64_t ViewBoundsIdle {};
    uint64_t ViewBoundsFallback {};
    array<uint64_t, 6> AnimationBoundsMaxExtent {};
};

// The ModelAnimationInfo record of one model, produced independently of the other models and concatenated
// in sorted file order so the manifest stays deterministic
struct ModelAnimationInfoSection
{
    string ConfigText {};
    ModelAnimationInfoBakingStats Stats {};
};

// One emitted animation pair with the clip that poses it. The owning asset is held so the animation
// reference stays alive for the whole pass
struct ModelAnimationInfoEntry
{
    int32_t StateAnim {};
    int32_t ActionAnim {};
    int32_t DurationMs {};
    size_t ClipIndex {};
    shared_ptr<const ModelSourceAsset> Source {};
    nptr<const ModelAnimationSource> Animation {};
    bool Reversed {};
};

// One clip a description plays, resolved once and measured by every attachment of that description.
// The owning asset is held so the animation reference stays alive for the whole pass
struct ModelDescriptionClip
{
    shared_ptr<const ModelSourceAsset> Source {};
    nptr<const ModelAnimationSource> Animation {};
    bool Reversed {};
};

// Bounds view of a description's own model. A clip whose visible geometry is entirely disabled falls back to the
// unfiltered mesh, so both selections are prepared once and reused across every clip of the description
class DescriptionBoundsSampler final
{
public:
    DescriptionBoundsSampler(const ModelMeshData& model_mesh, const BakerModelDescription& description, ModelBoundsMeasurement measurement);
    DescriptionBoundsSampler(const DescriptionBoundsSampler&) = delete;
    DescriptionBoundsSampler(DescriptionBoundsSampler&&) noexcept = delete;
    auto operator=(const DescriptionBoundsSampler&) -> DescriptionBoundsSampler& = delete;
    auto operator=(DescriptionBoundsSampler&&) noexcept -> DescriptionBoundsSampler& = delete;
    ~DescriptionBoundsSampler() = default;

    // The retry counter lets the full bake keep its calculation statistics while the targeted bake ignores them
    auto CalculateAnimationBounds(const ModelAnimationSource& animation, bool reversed, int32_t state_anim, int32_t action_anim, string_view fname, uint64_t& disabled_mesh_retries) const -> optional<ModelBounds3D>;
    auto CalculateStaticBounds(string_view fname) const -> ModelBounds3D;

private:
    ModelBoundsSampler _filtered;
    optional<ModelBoundsSampler> _unfiltered {};
};

static auto IsModelDescriptionTemplateFile(string_view path) -> bool;
static auto GetModelDescriptionMaxWriteTime(const FileCollection& files, const NameResolver& name_resolver, string_view fname) -> uint64_t;
static void CollectModelDescriptionLinkDependencies(const BakerModelDescriptionLink& link, unordered_set<string>& required_dependencies, unordered_set<string>& optional_dependencies);
static void UpdateModelDescriptionDependencyWriteTime(const FileCollection& files, string_view dependency, string_view owner, bool required, uint64_t& max_write_time);

static auto ValidateModelDescription(const BakingSettings& settings, const FileCollection& source_files, const FileSystem& baked_files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> ValidatedModelDescription;
static auto ValidateModelDescriptionAnimations(const FileCollection& source_files, const NameResolver& name_resolver, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakerModelDescription& description, string_view fname) -> ValidatedModelAnimations;
static void ValidateModelDescriptionAnimationData(ModelSkeletonCompatibilityReport& compatibility_report, const vector<ModelAnimationSource>& animation_sources, string_view fname);
static void ValidateModelDescriptionAttachment(const BakingSettings& settings, const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& main_info, const BakerModelDescriptionLink& link, string_view fname);
static void ValidateDirectAttachmentSize(const BakingSettings& settings, const BakedModelMeshInfo& child_info, string_view child_name, string_view fname);
static void ValidateFo3dAggregateModelBounds(const BakingSettings& settings, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname);
static void ValidateAggregateModelBoundsExtent(const BakingSettings& settings, const ModelBounds3D& bounds, string_view fname);
static void ValidateModelWorldExtent(const BakingSettings& settings, const ModelBounds3D& bounds, string_view too_large_message, string_view too_small_message, string_view what, string_view fname);
static auto GetModelBoundsMaxAbsExtent(const ModelBounds3D& bounds) -> float32_t;
static auto CalculateFo3dAggregateModelBounds(const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, const BakerModelDescription& description, string_view fname) -> ModelBounds3D;
static auto ReadBakedModelMeshForBounds(const FileSystem& baked_files, const BakerModelDescription& description, string_view fname) -> ModelMeshData;
static auto ReadBakedModelDescriptionForBounds(const FileSystem& baked_files, string_view fname) -> BakerModelDescription;
static auto MakeModelDescriptionLinkTransform(const BakerModelDescriptionLink& child_default_link, const BakerModelDescriptionLink& outer_link) -> mat44;
static void CalculateModelDescriptionLinkBounds(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, async_launch_mode async_mode, const ModelMeshData& parent_model_mesh, BakerModelDescription& description, string_view fname);
static auto CollectModelDescriptionClips(const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> vector<ModelDescriptionClip>;
static auto SampleModelDescriptionLinkBoneTracks(const ModelBoundsSampler& parent_sampler, const vector<ModelDescriptionClip>& clips, const unordered_set<string>& link_bones, async_launch_mode async_mode) -> unordered_map<string, vector<optional<vector<mat44>>>>;
static auto CalculateModelDescriptionLinkBoundsEntry(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, const vector<ModelDescriptionClip>& clips, const unordered_map<string, vector<optional<vector<mat44>>>>& link_bone_tracks, nptr<const ModelBoundsSampler> parent_sampler, const BakerModelDescriptionLink& link, string_view fname) -> optional<ModelBounds3D>;
static auto GetModelBoundsMeasurement(const BakingSettings& settings) -> ModelBoundsMeasurement;
static void ValidateModelDescriptionLinkData(const FileCollection& source_files, const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& target_info, nptr<const BakedModelMeshInfo> parent_info, const BakerModelDescriptionLink& link, string_view fname);
static void ValidateModelDescriptionCut(const FileCollection& source_files, const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& target_info, const BakerModelDescriptionCut& cut, string_view fname);
static void ValidateModelDescriptionTexture(const FileSystem& baked_files, const BakedModelMeshInfo& model_info, string_view texture_name, string_view token, string_view fname);
static void ValidateModelDescriptionEffect(const FileSystem& baked_files, string_view effect_name, string_view token, string_view fname);
static void ValidateModelDescriptionBakedFileExists(const FileSystem& baked_files, string_view path, string_view kind, string_view fname);
static void ValidateModelDescriptionDrawBoneReference(const BakedModelMeshInfo& info, string_view bone_name, string_view token, string_view fname);
static void ValidateModelDescriptionBoneReference(const BakedModelMeshInfo& info, string_view bone_name, string_view token, string_view fname);
static void ValidateModelDescriptionMeshReference(const BakedModelMeshInfo& info, string_view mesh_name, string_view token, string_view fname);
static void ValidateModelDescriptionAnimPair(const NameResolver& name_resolver, int32_t state_anim, int32_t action_anim, string_view token, string_view fname);
static void ValidateModelDescriptionEnumValue(const NameResolver& name_resolver, string_view enum_name, int32_t value, string_view token, string_view fname);
static void ValidateModelDescriptionLayer(int32_t layer, string_view token, string_view fname, size_t line);

static auto GetBakedModelMeshInfo(const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& cache, string_view path) -> const BakedModelMeshInfo&;
static auto ReadBakedModelMeshInfo(const FileSystem& baked_files, string_view path) -> BakedModelMeshInfo;
static void ValidateBakedModelMeshFreshness(const FileCollection& source_files, const BakedModelMeshInfo& info, string_view owner);
static auto GetModelSourceAsset(const ModelSourceAssetCache& model_sources, string_view path, string_view owner) -> shared_ptr<const ModelSourceAsset>;
static auto ModelSourceAssetHasAnimation(const ModelSourceAsset& asset, string_view anim_name) -> bool;
static auto GetModelSourceAnimation(const ModelSourceAsset& asset, string_view anim_name) -> const ModelAnimationSource&;
static auto GetModelSourceAnimationDuration(const ModelSourceAsset& asset, string_view anim_name) -> float32_t;
static void BakeModelAnimationInfo(const BakingContext& ctx, const FileCollection& files, const ModelSourceAssetCache& model_sources, string_view target_path);
static auto BakeModelAnimationInfoSection(const BakingContext& ctx, const FileCollection& files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, async_launch_mode async_mode, const File& file) -> ModelAnimationInfoSection;
static auto CollectModelAnimationInfoEntries(const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname, ModelAnimationInfoBakingStats& stats) -> vector<ModelAnimationInfoEntry>;
static auto SampleModelAnimationInfoClipBounds(const DescriptionBoundsSampler& sampler, const vector<ModelAnimationInfoEntry>& entries, async_launch_mode async_mode, string_view fname, ModelAnimationInfoBakingStats& stats) -> vector<optional<ModelBounds3D>>;
static void CollectBakedModelMeshInfo(const ModelMeshBoneData& bone, BakedModelMeshInfo& info, const vector<string>& parent_hierarchy);

static auto TokenizeModelDescriptionLine(string_view line) -> vector<string>;
static auto ApplyModelDescriptionReplacements(string content, const vector<pair<string, string>>& replacements) -> string;
static auto TakeModelDescriptionToken(const vector<string>& tokens, size_t& index, string_view token, string_view fname, size_t line) -> string;
static auto ParseModelDescriptionFloat(string_view value, string_view token, string_view fname, size_t line) -> float32_t;
static auto ParseModelDescriptionInt(string_view value, const NameResolver& name_resolver, string_view token, string_view fname, size_t line) -> int32_t;
static void ApplyModelDescriptionAdd(float32_t& value, float32_t operand);
static void ApplyModelDescriptionMul(float32_t& value, float32_t operand);

ModelInfoBaker::ModelInfoBaker(shared_ptr<BakingContext> ctx, ModelSourceAssetCache::LoadCallback model_source_loader) :
    BaseBaker(std::move(ctx), NAME),
    _modelSourceLoader {std::move(model_source_loader)}
{
    FO_STACK_TRACE_ENTRY();
}

ModelInfoBaker::~ModelInfoBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ModelInfoBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(_context->BakedFiles, "Baker context has no baked file registry");
    FO_VERIFY_AND_THROW(_context->Settings, "Baker context has no baking settings");
    ModelSourceAssetCache model_sources {files, _modelSourceLoader};

    if (target_path == "ModelAnimationInfo.foinfo") {
        BakeModelAnimationInfo(*_context, files, model_sources, target_path);
        return;
    }

    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const FileHeader& file_header : files) {
            string ext = strex(file_header.GetPath()).get_file_extension();

            if (ext != "fo3d") {
                continue;
            }
            if (IsModelDescriptionTemplateFile(file_header.GetPath())) {
                continue;
            }

            filtered_files.emplace_back(File::Load(file_header));
        }
    }
    else {
        string ext = strex(target_path).get_file_extension();

        if (ext != "fo3d") {
            return;
        }
        if (IsModelDescriptionTemplateFile(target_path)) {
            return;
        }

        File file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }

        filtered_files.emplace_back(std::move(file));
    }

    if (filtered_files.empty()) {
        return;
    }

    BakerClientEngine dependency_engine(*_context->BakedFiles);
    vector<File> files_to_bake;

    for (File& file_ : filtered_files) {
        uint64_t max_write_time = GetModelDescriptionMaxWriteTime(files, dependency_engine, file_.GetPath());

        if (_context->BakeChecker && !_context->BakeChecker(file_.GetPath(), max_write_time)) {
            continue;
        }

        files_to_bake.emplace_back(std::move(file_));
    }

    vector<std::future<void>> file_bakings;
    bool validate_aggregate_bounds = !target_path.empty();

    for (File& file_ : files_to_bake) {
        string task_name = strex("BakeModelInfo-{}", file_.GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, &files, &model_sources, validate_aggregate_bounds, file = std::move(file_)]() FO_DEFERRED {
            BakerClientEngine client_engine(*_context->BakedFiles);

            if (_context->BakeChecker) {
                uint64_t max_write_time = GetModelDescriptionMaxWriteTime(files, client_engine, file.GetPath());

                if (!_context->BakeChecker(file.GetPath(), max_write_time)) {
                    return;
                }
            }

            ModelDescriptionParser parser(&files, &client_engine);
            auto [description, max_write_time] = parser.Parse(file.GetPath());
            ignore_unused(max_write_time);

            ValidatedModelDescription validated = ValidateModelDescription(*_context->Settings, files, *_context->BakedFiles, client_engine, model_sources, description, file.GetPath());

            ModelMeshData model_mesh = ReadBakedModelMeshForBounds(*_context->BakedFiles, description, file.GetPath());
            CalculateModelDescriptionLinkBounds(files, *_context->BakedFiles, model_sources, GetModelBoundsMeasurement(*_context->Settings), GetAsyncMode(), model_mesh, description, file.GetPath());

            // A targeted .fo3d bake does not run BakeModelAnimationInfo, so the lighting envelope is checked here
            if (validate_aggregate_bounds) {
                ValidateFo3dAggregateModelBounds(*_context->Settings, *_context->BakedFiles, model_sources, description, file.GetPath());
            }

            vector<uint8_t> data;
            DataWriter writer(data);
            writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
            writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
            writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
            description.Save(writer);
            vector<uint8_t> animation_rig_data = WriteModelAnimationRigData(validated.AnimationRigData, file.GetPath());
            writer.Write<uint64_t>(numeric_cast<uint64_t>(animation_rig_data.size()));
            writer.WriteBytes(animation_rig_data);
            _context->WriteData(file.GetPath(), data);
        }));
    }

    size_t errors = 0;

    for (std::future<void>& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Model description baking error: {}", ex.what());
            errors++;
        }
    }

    if (errors != 0) {
        throw ModelInfoBakerException("Errors during model description baking");
    }

    if (target_path.empty()) {
        BakeModelAnimationInfo(*_context, files, model_sources, target_path);
    }
}

static auto IsModelDescriptionTemplateFile(string_view path) -> bool
{
    FO_STACK_TRACE_ENTRY();

    return strex(path).extract_file_name().starts_with("TEMPLATE_");
}

static auto GetModelDescriptionMaxWriteTime(const FileCollection& files, const NameResolver& name_resolver, string_view fname) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    ModelDescriptionParser parser {&files, &name_resolver};
    auto [description, max_write_time] = parser.Parse(fname);
    unordered_set<string> required_dependencies;
    unordered_set<string> optional_dependencies;

    if (!description.Model.empty()) {
        required_dependencies.emplace(description.Model);
    }

    set<pair<int32_t, int32_t>> selected_animation_pairs;

    for (const BakerModelDescriptionAnimationEntry& anim : description.AnimationEntries) {
        if (!selected_animation_pairs.emplace(anim.StateAnim, anim.ActionAnim).second || anim.FileName == "ModelFile") {
            continue;
        }

        required_dependencies.emplace(strex(fname).extract_dir().combine_path(anim.FileName).str());
    }

    CollectModelDescriptionLinkDependencies(description.DefaultLink, required_dependencies, optional_dependencies);

    for (const BakerModelDescriptionLink& link : description.Links) {
        CollectModelDescriptionLinkDependencies(link, required_dependencies, optional_dependencies);
    }

    for (const string& dependency : required_dependencies) {
        UpdateModelDescriptionDependencyWriteTime(files, dependency, fname, true, max_write_time);
    }
    for (const string& dependency : optional_dependencies) {
        if (required_dependencies.count(dependency) == 0) {
            UpdateModelDescriptionDependencyWriteTime(files, dependency, fname, false, max_write_time);
        }
    }

    return max_write_time;
}

static void CollectModelDescriptionLinkDependencies(const BakerModelDescriptionLink& link, unordered_set<string>& required_dependencies, unordered_set<string>& optional_dependencies)
{
    FO_STACK_TRACE_ENTRY();

    if (!link.IsParticles && !link.ChildName.empty()) {
        if (strex(link.ChildName).get_file_extension() == "fo3d") {
            optional_dependencies.emplace(link.ChildName);
        }
        else {
            required_dependencies.emplace(link.ChildName);
        }
    }

    for (const BakerModelDescriptionCut& cut : link.CutInfo) {
        optional_dependencies.emplace(cut.FileName);
    }
}

static void UpdateModelDescriptionDependencyWriteTime(const FileCollection& files, string_view dependency, string_view owner, bool required, uint64_t& max_write_time)
{
    FO_STACK_TRACE_ENTRY();

    auto dependency_file = std::ranges::find_if(files, [&](const FileHeader& file) { return file.GetPath() == dependency; });

    if (dependency_file == files.end()) {
        if (required) {
            throw ModelInfoBakerException("Model source dependency referenced by owner was not found", dependency, owner);
        }

        return;
    }

    max_write_time = std::max(max_write_time, dependency_file->GetWriteTime());
}

ModelDescriptionParser::ModelDescriptionParser(ptr<const FileCollection> files, ptr<const NameResolver> name_resolver) :
    _files {files},
    _nameResolver {name_resolver}
{
    FO_STACK_TRACE_ENTRY();
}

auto ModelDescriptionParser::Parse(string_view fname) -> pair<BakerModelDescription, uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    BakerModelDescription description;
    ModelDescriptionParseState state;
    state.Link = ModelDescriptionLinkPtr(description.DefaultLink);
    ParseFile(fname, {}, description, state);

    return {std::move(description), _maxWriteTime};
}

void ModelDescriptionParser::ParseFile(string_view fname, const vector<pair<string, string>>& replacements, BakerModelDescription& description, ModelDescriptionParseState& state)
{
    FO_STACK_TRACE_ENTRY();

    if (std::ranges::find(_includeStack, fname) != _includeStack.end()) {
        throw ModelInfoBakerException("Recursive model description include", fname);
    }

    File file = _files->FindFileByPath(fname);

    if (!file) {
        throw ModelInfoBakerException("Model description file not found", fname);
    }

    _maxWriteTime = std::max(_maxWriteTime, file.GetWriteTime());
    _includeStack.emplace_back(fname);

    string content = ApplyModelDescriptionReplacements(file.GetStr(), replacements);
    ParseContent(file.GetPath(), content, description, state);

    _includeStack.pop_back();
}

void ModelDescriptionParser::ParseContent(string_view fname, const string& content, BakerModelDescription& description, ModelDescriptionParseState& state)
{
    FO_STACK_TRACE_ENTRY();

    istringstream istr = istringstream(make_stream_string(content));
    string line_buf;
    size_t line = 0;

    while (getline(istr, line_buf)) {
        line++;

        vector<string> tokens = TokenizeModelDescriptionLine(line_buf);
        size_t index = 0;

        while (index < tokens.size()) {
            string token = tokens[index++];

            ParseToken(fname, line, token, tokens, index, description, state);
        }
    }
}

void ModelDescriptionParser::ParseToken(string_view fname, size_t line, string_view token, const vector<string>& tokens, size_t& index, BakerModelDescription& description, ModelDescriptionParseState& state)
{
    FO_STACK_TRACE_ENTRY();

    if (token == "Model") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.Model = strex(fname).extract_dir().combine_path(value);
    }
    else if (token == "Include") {
        if (index >= tokens.size()) {
            throw ModelInfoBakerException("Missing include path", fname, line);
        }

        string include_name = tokens[index++];

        if ((tokens.size() - index) % 2 != 0) {
            throw ModelInfoBakerException("Include has unpaired template argument", include_name, fname, line);
        }

        vector<pair<string, string>> replacements;

        while (index < tokens.size()) {
            string name = tokens[index++];
            string value = tokens[index++];
            replacements.emplace_back(std::move(name), std::move(value));
        }

        string include_path = strex(fname).extract_dir().combine_path(include_name);
        ParseFile(include_path, replacements, description, state);
        index = tokens.size();
    }
    else if (token == "Mesh") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        state.Mesh = value != "All" ? value : string {};
    }
    else if (token == "Subset") {
        (void)TakeModelDescriptionToken(tokens, index, token, fname, line);
        WriteLog("Tag 'Subset' obsolete, use 'Mesh' instead");
    }
    else if (token == "Layer" || token == "Value") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t parsed_value = ParseModelDescriptionInt(value, *_nameResolver, token, fname, line);

        if (token == "Layer") {
            ValidateModelDescriptionLayer(parsed_value, token, fname, line);
            state.Layer = parsed_value;
        }
        else {
            state.LayerValue = parsed_value;
        }

        state.Link = ModelDescriptionLinkPtr(state.DummyLink);
        state.Mesh.clear();
    }
    else if (token == "Root") {
        if (state.Layer == -1) {
            state.Link = ModelDescriptionLinkPtr(description.DefaultLink);
        }
        else if (state.LayerValue == 0) {
            throw ModelInfoBakerException("Wrong zero value for layer", state.Layer, fname, line);
        }
        else {
            state.Link = ModelDescriptionLinkPtr(description.Links.emplace_back());
            state.Link->Layer = state.Layer;
            state.Link->LayerValue = state.LayerValue;
        }

        state.Mesh.clear();
    }
    else if (token == "Attach" || token == "AttachParticles") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        if (state.Layer < 0 || state.LayerValue == 0) {
            throw ModelInfoBakerException("Token requires non-zero layer value", token, fname, line);
        }

        state.Link = ModelDescriptionLinkPtr(description.Links.emplace_back());
        state.Link->Layer = state.Layer;
        state.Link->LayerValue = state.LayerValue;
        state.Link->ChildName = token == "Attach" ? strex(fname).extract_dir().combine_path(value).str() : value;
        state.Link->IsParticles = token == "AttachParticles";
        state.Mesh.clear();
    }
    else if (token == "Link") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        auto default_link = ModelDescriptionLinkPtr(description.DefaultLink);
        auto dummy_link = ModelDescriptionLinkPtr(state.DummyLink);

        if (state.Link != default_link && state.Link != dummy_link) {
            state.Link->LinkBone = value;
        }
    }
    else if (token == "Cut") {
        string file_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string layers = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string shapes = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string unskin_bone1 = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string unskin_bone2 = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string unskin_shape = TakeModelDescriptionToken(tokens, index, token, fname, line);

        BakerModelDescriptionCut& cut = state.Link->CutInfo.emplace_back();
        cut.FileName = strex(fname).extract_dir().combine_path(file_name);

        for (const string& cut_layer_name : strex(layers).split('-')) {
            if (cut_layer_name != "All") {
                int32_t cut_layer = ParseModelDescriptionInt(cut_layer_name, *_nameResolver, token, fname, line);
                ValidateModelDescriptionLayer(cut_layer, token, fname, line);
                cut.Layers.emplace_back(cut_layer);
            }
            else {
                for (int32_t i = 0; i < numeric_cast<int32_t>(MODEL_LAYERS_COUNT); i++) {
                    if (i != state.Layer) {
                        cut.Layers.emplace_back(i);
                    }
                }
            }
        }

        for (const string& shape_name : strex(shapes).split('-')) {
            cut.Shapes.emplace_back(shape_name != "All" ? shape_name : string {});
        }

        cut.UnskinBone1 = unskin_bone1 != "-" ? unskin_bone1 : string {};
        cut.UnskinBone2 = unskin_bone2 != "-" ? unskin_bone2 : string {};
        cut.RevertUnskinShape = !unskin_shape.empty() && unskin_shape[0] == '~';
        cut.UnskinShape = unskin_shape != "-" ? (cut.RevertUnskinShape ? unskin_shape.substr(1) : unskin_shape) : string {};
    }
    else if (token == "RotX" || token == "RotY" || token == "RotZ" || token == "MoveX" || token == "MoveY" || token == "MoveZ" || token == "ScaleX" || token == "ScaleY" || token == "ScaleZ" || token == "Speed") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token, ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Set);
    }
    else if (token == "Scale") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        state.Link->ScaleX = parsed_value;
        state.Link->ScaleY = parsed_value;
        state.Link->ScaleZ = parsed_value;
    }
    else if (token == "RotX+" || token == "RotY+" || token == "RotZ+" || token == "MoveX+" || token == "MoveY+" || token == "MoveZ+" || token == "ScaleX+" || token == "ScaleY+" || token == "ScaleZ+" || token == "Speed+") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token.substr(0, token.length() - 1), ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Add);
    }
    else if (token == "Scale+") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        ApplyModelDescriptionAdd(state.Link->ScaleX, parsed_value);
        ApplyModelDescriptionAdd(state.Link->ScaleY, parsed_value);
        ApplyModelDescriptionAdd(state.Link->ScaleZ, parsed_value);
    }
    else if (token == "RotX*" || token == "RotY*" || token == "RotZ*" || token == "MoveX*" || token == "MoveY*" || token == "MoveZ*" || token == "ScaleX*" || token == "ScaleY*" || token == "ScaleZ*" || token == "Speed*") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token.substr(0, token.length() - 1), ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Mul);
    }
    else if (token == "Scale*") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        ApplyModelDescriptionMul(state.Link->ScaleX, parsed_value);
        ApplyModelDescriptionMul(state.Link->ScaleY, parsed_value);
        ApplyModelDescriptionMul(state.Link->ScaleZ, parsed_value);
    }
    else if (token == "DisableLayer") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        for (const string& disabled_layer_name : strex(value).split('-')) {
            int32_t disabled_layer = ParseModelDescriptionInt(disabled_layer_name, *_nameResolver, token, fname, line);
            ValidateModelDescriptionLayer(disabled_layer, token, fname, line);
            state.Link->DisabledLayer.emplace_back(disabled_layer);
        }
    }
    else if (token == "DisableMesh") {
        string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        for (const string& disabled_mesh_name : strex(value).split('-')) {
            state.Link->DisabledMesh.emplace_back(disabled_mesh_name != "All" ? disabled_mesh_name : string {});
        }
    }
    else if (token == "Texture") {
        string index_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t texture_index = ParseModelDescriptionInt(index_value, *_nameResolver, token, fname, line);
        string texture_name = TakeModelDescriptionToken(tokens, index, token, fname, line);

        if (texture_index < 0 || texture_index >= numeric_cast<int32_t>(MODEL_MAX_TEXTURES)) {
            throw ModelInfoBakerException("Texture index is out of range", texture_index, fname, line, MODEL_MAX_TEXTURES);
        }

        state.Link->TextureInfo.emplace_back(texture_name, state.Mesh, texture_index);
    }
    else if (token == "Effect") {
        string effect_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        state.Link->EffectInfo.emplace_back(effect_name, state.Mesh);
    }
    else if (token == "Anim") {
        string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        string anim_file = TakeModelDescriptionToken(tokens, index, token, fname, line);
        string anim_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.AnimationEntries.emplace_back(BakerModelDescriptionAnimationEntry {.StateAnim = state_anim, .ActionAnim = action_anim, .FileName = anim_file, .Name = anim_name});
    }
    else if (token == "AllowAnimationGeometry") {
        string anim_file = TakeModelDescriptionToken(tokens, index, token, fname, line);

        if (!description.AnimationGeometryExceptions.emplace(anim_file).second) {
            throw ModelInfoBakerException("Duplicate animation-geometry exception", anim_file, fname, line);
        }
    }
    else if (token == "AnimSpeed") {
        string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        string speed_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.AnimSpeed.emplace_back(std::make_pair(state_anim, action_anim), ParseModelDescriptionFloat(speed_value, token, fname, line));
    }
    else if (token == "AnimLayerValue") {
        string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        string layer_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t layer = ParseModelDescriptionInt(layer_value, *_nameResolver, token, fname, line);
        string anim_layer_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t value = ParseModelDescriptionInt(anim_layer_value, *_nameResolver, token, fname, line);
        ValidateModelDescriptionLayer(layer, token, fname, line);
        description.AnimLayerValues.emplace_back(BakerModelDescriptionAnimLayerValue {.StateAnim = state_anim, .ActionAnim = action_anim, .Layer = layer, .LayerValue = value});
    }
    else if (token == "FastTransitionBone") {
        description.FastTransitionBones.emplace_back(TakeModelDescriptionToken(tokens, index, token, fname, line));
    }
    else if (token == "StateAnimEqual") {
        string from_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t from = ParseModelDescriptionInt(from_value, *_nameResolver, token, fname, line);
        string to_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t to = ParseModelDescriptionInt(to_value, *_nameResolver, token, fname, line);
        description.StateAnimEquals.emplace_back(from, to);
    }
    else if (token == "ActionAnimEqual") {
        string from_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t from = ParseModelDescriptionInt(from_value, *_nameResolver, token, fname, line);
        string to_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        int32_t to = ParseModelDescriptionInt(to_value, *_nameResolver, token, fname, line);
        description.ActionAnimEquals.emplace_back(from, to);
    }
    else if (token == "DisableShadow") {
        description.ShadowDisabled = true;
    }
    else if (token == "DisableAnimationInterpolation") {
        description.DisableAnimationInterpolation = true;
    }
    else if (token == "DisableBackwardAnim") {
        description.DisableBackwardAnim = true;
    }
    else if (token == "RotationBone") {
        description.RotationBone = TakeModelDescriptionToken(tokens, index, token, fname, line);
    }
    else {
        throw ModelInfoBakerException("Unknown token in file", token, fname, line);
    }
}

void ModelDescriptionParser::ApplyFloatValue(BakerModelDescriptionLink& link, string_view field, float32_t value, AssignMode mode)
{
    FO_STACK_TRACE_ENTRY();

    nptr<float32_t> target = nullptr;

    if (field == "RotX") {
        target = &link.RotX;
    }
    else if (field == "RotY") {
        target = &link.RotY;
    }
    else if (field == "RotZ") {
        target = &link.RotZ;
    }
    else if (field == "MoveX") {
        target = &link.MoveX;
    }
    else if (field == "MoveY") {
        target = &link.MoveY;
    }
    else if (field == "MoveZ") {
        target = &link.MoveZ;
    }
    else if (field == "ScaleX") {
        target = &link.ScaleX;
    }
    else if (field == "ScaleY") {
        target = &link.ScaleY;
    }
    else if (field == "ScaleZ") {
        target = &link.ScaleZ;
    }
    else if (field == "Speed") {
        target = &link.SpeedAjust;
    }
    else {
        FO_UNREACHABLE_PLACE();
    }

    FO_VERIFY_AND_THROW(target, "Model description field did not resolve to a target value", field);

    switch (mode) {
    case AssignMode::Set:
        *target = value;
        break;
    case AssignMode::Add:
        ApplyModelDescriptionAdd(*target, value);
        break;
    case AssignMode::Mul:
        ApplyModelDescriptionMul(*target, value);
        break;
    default:
        FO_UNREACHABLE_PLACE();
    }
}

static auto ValidateModelDescription(const BakingSettings& settings, const FileCollection& source_files, const FileSystem& baked_files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> ValidatedModelDescription
{
    FO_STACK_TRACE_ENTRY();

    if (description.Model.empty()) {
        throw ModelInfoBakerException("'Model' section not found in file", fname);
    }

    unordered_map<string, BakedModelMeshInfo> mesh_cache;
    const BakedModelMeshInfo& main_info = GetBakedModelMeshInfo(baked_files, mesh_cache, description.Model);
    ValidateBakedModelMeshFreshness(source_files, main_info, fname);

    if (main_info.DrawBonesCount == 0) {
        throw ModelInfoBakerException("Model referenced by file has no drawable meshes", description.Model, fname);
    }

    ValidateModelDescriptionBoneReference(main_info, description.RotationBone, "RotationBone", fname);

    for (const string& bone_name : description.FastTransitionBones) {
        ValidateModelDescriptionBoneReference(main_info, bone_name, "FastTransitionBone", fname);
    }

    for (const string& diffuse_texture : main_info.DiffuseTextures) {
        ValidateModelDescriptionTexture(baked_files, main_info, diffuse_texture, "Model", fname);
    }

    ValidateModelDescriptionLinkData(source_files, baked_files, mesh_cache, main_info, nullptr, description.DefaultLink, fname);

    for (const BakerModelDescriptionLink& link : description.Links) {
        ValidateModelDescriptionAttachment(settings, source_files, baked_files, model_sources, mesh_cache, main_info, link, fname);
    }

    ValidatedModelAnimations animations = ValidateModelDescriptionAnimations(source_files, name_resolver, baked_files, model_sources, mesh_cache, description, fname);
    vector<ModelSkeletonClipSource> clip_sources;
    clip_sources.reserve(animations.Sources.size());

    for (const ModelAnimationSource& animation : animations.Sources) {
        shared_ptr<const ModelSourceAsset> animation_model = GetModelSourceAsset(model_sources, animation.FileName, fname);
        ModelSkeletonClipSource& clip_source = clip_sources.emplace_back();
        clip_source.FileName = animation.FileName;
        clip_source.ClipName = animation.Name;
        clip_source.Joints = animation_model->Skeleton.Joints;
        clip_source.AnimatedJointHierarchies.reserve(animation.Joints.size());

        for (const ModelAnimationJointSource& joint : animation.Joints) {
            clip_source.AnimatedJointHierarchies.emplace_back(joint.Hierarchy);
        }
    }

    ValidatedModelDescription result;

    try {
        result.CompatibilityReport = BuildModelSkeletonCompatibilityReport(main_info.Skeleton, clip_sources);
    }
    catch (const ModelSkeletonCompatibilityException& ex) {
        throw ModelInfoBakerException("Skeleton compatibility validation failed", fname, ex.what());
    }

    ValidateModelDescriptionAnimationData(result.CompatibilityReport, animations.Sources, fname);

    try {
        ModelAnimationRigArtifacts artifacts = BuildModelAnimationRigArtifacts(fname, main_info.Skeleton, result.CompatibilityReport, animations.Sources, description.DisableAnimationInterpolation);
        result.AnimationRigData = BuildModelAnimationRigData(std::move(artifacts), animations.Bindings);
    }
    catch (const ModelAnimationConverterException& ex) {
        throw ModelInfoBakerException("Canonical animation conversion failed", fname, ex.what());
    }
    catch (const ModelAnimationArchiveException& ex) {
        throw ModelInfoBakerException("Canonical animation archive validation failed", fname, ex.what());
    }
    catch (const ModelAnimationRigDataException& ex) {
        throw ModelInfoBakerException("Canonical animation data validation failed", fname, ex.what());
    }

    return result;
}

static auto ValidateModelDescriptionAnimations(const FileCollection& source_files, const NameResolver& name_resolver, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakerModelDescription& description, string_view fname) -> ValidatedModelAnimations
{
    FO_STACK_TRACE_ENTRY();

    set<pair<int32_t, int32_t>> anim_pairs;
    set<pair<string, string>> animation_identities;
    set<string> geometry_exceptions;
    set<string> selected_external_animation_files;
    set<string> used_geometry_exceptions;
    ValidatedModelAnimations result;

    for (const string& exception : description.AnimationGeometryExceptions) {
        string resolved_exception = strex(fname).extract_dir().combine_path(exception).str();

        if (!geometry_exceptions.emplace(resolved_exception).second) {
            throw ModelInfoBakerException("Animation-geometry exceptions contain duplicate resolved target; keep exactly one AllowAnimationGeometry line", fname, resolved_exception);
        }
    }

    for (const BakerModelDescriptionAnimationEntry& anim_entry : description.AnimationEntries) {
        ValidateModelDescriptionAnimPair(name_resolver, anim_entry.StateAnim, anim_entry.ActionAnim, "Anim", fname);

        if (!anim_pairs.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
            continue;
        }

        string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(fname).extract_dir().combine_path(anim_entry.FileName).str();
        const BakedModelMeshInfo& anim_info = GetBakedModelMeshInfo(baked_files, mesh_cache, anim_file);
        ValidateBakedModelMeshFreshness(source_files, anim_info, fname);
        shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, fname);
        string anim_name = anim_entry.Name;

        if (anim_file != description.Model) {
            selected_external_animation_files.emplace(anim_file);

            if (anim_info.DrawBonesCount != 0) {
                if (geometry_exceptions.count(anim_file) == 0) {
                    string first_draw_bone = anim_info.DrawBones.empty() ? string {"<unnamed>"} : *std::ranges::min_element(anim_info.DrawBones);
                    throw ModelInfoBakerException("External animation model contains drawable mesh nodes; remove the geometry or temporarily add an AllowAnimationGeometry line while repairing the source", anim_file, fname, anim_info.DrawBonesCount, first_draw_bone, anim_entry.FileName);
                }

                used_geometry_exceptions.emplace(anim_file);
            }
        }

        if (!anim_name.empty() && anim_name.front() == '~') {
            anim_name.erase(anim_name.begin());
        }

        if (!ModelSourceAssetHasAnimation(*anim_source, anim_name)) {
            throw ModelInfoBakerException("Animation for state/action pair not found in animation file", anim_entry.Name, anim_entry.StateAnim, anim_entry.ActionAnim, fname, anim_file);
        }

        const ModelAnimationSource& selected_animation = GetModelSourceAnimation(*anim_source, anim_name);

        result.Bindings.emplace_back(ModelAnimationRigBindingSource {
            anim_entry.StateAnim,
            anim_entry.ActionAnim,
            selected_animation.FileName,
            selected_animation.Name,
            !anim_entry.Name.empty() && anim_entry.Name.front() == '~',
        });

        if (animation_identities.emplace(selected_animation.FileName, selected_animation.Name).second) {
            result.Sources.emplace_back(selected_animation);
        }
    }

    for (const string& exception : geometry_exceptions) {
        if (selected_external_animation_files.count(exception) == 0) {
            throw ModelInfoBakerException("Animation-geometry exception does not match a selected external Anim source; remove the AllowAnimationGeometry line or select that exact file", exception, fname);
        }
        if (used_geometry_exceptions.count(exception) == 0) {
            throw ModelInfoBakerException("Animation-geometry exception is stale because the selected external animation no longer contains drawable meshes; remove the AllowAnimationGeometry line", exception, fname);
        }
    }

    for (const auto& [anim_pair, speed] : description.AnimSpeed) {
        ValidateModelDescriptionAnimPair(name_resolver, anim_pair.first, anim_pair.second, "AnimSpeed", fname);

        if (speed <= 0.0f || !std::isfinite(1.0f / speed)) {
            throw ModelInfoBakerException("Animation speed for state/action pair must be positive with a finite reciprocal", anim_pair.first, anim_pair.second, fname);
        }
    }

    for (const BakerModelDescriptionAnimLayerValue& value : description.AnimLayerValues) {
        ValidateModelDescriptionAnimPair(name_resolver, value.StateAnim, value.ActionAnim, "AnimLayerValue", fname);
        ValidateModelDescriptionLayer(value.Layer, "AnimLayerValue", fname, 0);
    }

    for (const auto& [from, to] : description.StateAnimEquals) {
        ValidateModelDescriptionEnumValue(name_resolver, "CritterStateAnim", from, "StateAnimEqual", fname);
        ValidateModelDescriptionEnumValue(name_resolver, "CritterStateAnim", to, "StateAnimEqual", fname);
    }

    for (const auto& [from, to] : description.ActionAnimEquals) {
        ValidateModelDescriptionEnumValue(name_resolver, "CritterActionAnim", from, "ActionAnimEqual", fname);
        ValidateModelDescriptionEnumValue(name_resolver, "CritterActionAnim", to, "ActionAnimEqual", fname);
    }

    return result;
}

static void ValidateModelDescriptionAnimationData(ModelSkeletonCompatibilityReport& compatibility_report, const vector<ModelAnimationSource>& animation_sources, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    for (const ModelAnimationSource& animation : animation_sources) {
        for (const ModelAnimationJointSource& joint : animation.Joints) {
            ModelSkeletonAnimationDataIssue issue;
            issue.FileName = animation.FileName;
            issue.ClipName = animation.Name;
            issue.JointName = joint.OutputName;
            issue.Hierarchy = joint.Hierarchy;

            auto is_vec3_finite = [](const vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); };

            for (const vec3& value : joint.Scale.Values) {
                issue.NonFiniteScaleKeys += !is_vec3_finite(value) ? 1 : 0;
            }
            for (const quaternion& value : joint.Rotation.Values) {
                bool finite = std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
                issue.NonFiniteRotationKeys += !finite ? 1 : 0;
                issue.ZeroRotationKeys += finite && value.w == 0.0f && value.x == 0.0f && value.y == 0.0f && value.z == 0.0f ? 1 : 0;
            }
            for (const vec3& value : joint.Translation.Values) {
                issue.NonFiniteTranslationKeys += !is_vec3_finite(value) ? 1 : 0;
            }

            if (issue.NonFiniteScaleKeys == 0 && issue.NonFiniteRotationKeys == 0 && issue.ZeroRotationKeys == 0 && issue.NonFiniteTranslationKeys == 0) {
                continue;
            }

            throw ModelInfoBakerException("Animation has invalid output with non-finite scale/rotation/translation keys or zero rotation keys", animation.Name, animation.FileName, fname, joint.OutputName, issue.NonFiniteScaleKeys, issue.NonFiniteRotationKeys, issue.NonFiniteTranslationKeys, issue.ZeroRotationKeys);
        }
    }

    compatibility_report.AnimationDataIssues.clear();
}

static void ValidateModelDescriptionAttachment(const BakingSettings& settings, const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& main_info, const BakerModelDescriptionLink& link, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelDescriptionBoneReference(main_info, link.LinkBone, link.IsParticles ? "AttachParticles" : "Attach", fname);

    if (link.IsParticles) {
        ValidateModelDescriptionBakedFileExists(baked_files, link.ChildName, "Particle", fname);
        return;
    }

    if (link.ChildName.empty()) {
        ValidateModelDescriptionLinkData(source_files, baked_files, mesh_cache, main_info, nullptr, link, fname);
        return;
    }

    string child_ext = strex(link.ChildName).get_file_extension();

    if (child_ext == "fo3d") {
        if (!baked_files.IsFileExists(link.ChildName) && !source_files.FindFileByPath(link.ChildName)) {
            throw ModelInfoBakerException("Attached model description referenced by file not found", link.ChildName, fname);
        }

        return;
    }

    const BakedModelMeshInfo& child_info = GetBakedModelMeshInfo(baked_files, mesh_cache, link.ChildName);
    ValidateBakedModelMeshFreshness(source_files, child_info, fname);
    shared_ptr<const ModelSourceAsset> child_source = GetModelSourceAsset(model_sources, link.ChildName, fname);

    if (!child_source->Animations.empty()) {
        throw ModelInfoBakerException("Direct attached model contains embedded animation clips; animated child models require a .fo3d description", link.ChildName, fname, child_source->Animations.size());
    }

    ValidateDirectAttachmentSize(settings, child_info, link.ChildName, fname);

    ValidateModelDescriptionLinkData(source_files, baked_files, mesh_cache, child_info, &main_info, link, fname);
}

static auto GetModelBoundsMaxAbsExtent(const ModelBounds3D& bounds) -> float32_t
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::max({std::abs(bounds.Min.x), std::abs(bounds.Min.y), std::abs(bounds.Min.z), std::abs(bounds.Max.x), std::abs(bounds.Max.y), std::abs(bounds.Max.z)});
}

static void ValidateModelWorldExtent(const BakingSettings& settings, const ModelBounds3D& bounds, string_view too_large_message, string_view too_small_message, string_view what, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    float32_t extent = GetModelBoundsMaxAbsExtent(bounds);

    if (extent > settings.ModelAttachmentMaxExtent) {
        throw ModelInfoBakerException(too_large_message, what, fname, extent, settings.ModelAttachmentMaxExtent);
    }
    if (extent < settings.ModelAttachmentMinExtent) {
        throw ModelInfoBakerException(too_small_message, what, fname, extent, settings.ModelAttachmentMinExtent);
    }
}

// An attachment renders on the parent skeleton, so a foreign unit space shows up not in the render but in the sprite
// frame the client sizes from these bounds. See Engine/Docs/BakingPipeline.md
static void ValidateDirectAttachmentSize(const BakingSettings& settings, const BakedModelMeshInfo& child_info, string_view child_name, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (!child_info.StaticBounds) {
        return;
    }

    ValidateModelWorldExtent(settings, *child_info.StaticBounds, "Direct attached model reaches beyond the authored world extent; export it in the same units as the other models", "Direct attached model stays below the authored world extent; export it in the same units as the other models", child_name, fname);
}

static auto ReadBakedModelMeshForBounds(const FileSystem& baked_files, const BakerModelDescription& description, string_view fname) -> ModelMeshData
{
    FO_STACK_TRACE_ENTRY();

    if (description.Model.empty()) {
        throw ModelInfoBakerException("Model description has no Model section", fname);
    }

    File model_file = baked_files.ReadFile(description.Model);

    if (!model_file) {
        throw ModelInfoBakerException("Baked model data for bounds is not readable", description.Model, fname);
    }

    try {
        auto model_reader = DataReader(model_file.GetDataSpan());
        return ReadModelMeshData(model_reader, description.Model);
    }
    catch (const std::exception& ex) {
        throw ModelInfoBakerException("Invalid baked model mesh while calculating bounds", description.Model, fname, ex.what());
    }
}

DescriptionBoundsSampler::DescriptionBoundsSampler(const ModelMeshData& model_mesh, const BakerModelDescription& description, ModelBoundsMeasurement measurement) :
    _filtered {model_mesh, description.DefaultLink.DisabledMesh, measurement}
{
    FO_STACK_TRACE_ENTRY();

    if (!description.DefaultLink.DisabledMesh.empty()) {
        _unfiltered.emplace(model_mesh, vector<string> {}, measurement);
    }
}

auto DescriptionBoundsSampler::CalculateAnimationBounds(const ModelAnimationSource& animation, bool reversed, int32_t state_anim, int32_t action_anim, string_view fname, uint64_t& disabled_mesh_retries) const -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    try {
        optional<ModelBounds3D> calculated_bounds = _filtered.CalculateAnimationBounds(animation, reversed);

        if (!calculated_bounds && _unfiltered) {
            disabled_mesh_retries++;
            calculated_bounds = _unfiltered->CalculateAnimationBounds(animation, reversed);
        }

        return calculated_bounds;
    }
    catch (const ModelBoundsException& ex) {
        throw ModelInfoBakerException("Failed to calculate animation bounds", state_anim, action_anim, fname, ex.what());
    }
}

auto DescriptionBoundsSampler::CalculateStaticBounds(string_view fname) const -> ModelBounds3D
{
    FO_STACK_TRACE_ENTRY();

    optional<ModelBounds3D> model_bounds;

    try {
        model_bounds = _filtered.CalculateStaticBounds();

        if (!model_bounds && _unfiltered) {
            model_bounds = _unfiltered->CalculateStaticBounds();
        }
    }
    catch (const ModelBoundsException& ex) {
        throw ModelInfoBakerException("Failed to calculate static model bounds", fname, ex.what());
    }

    if (!model_bounds) {
        throw ModelInfoBakerException("Static model bounds could not be calculated", fname);
    }

    return *model_bounds;
}

static auto ReadBakedModelDescriptionCutForBounds(DataReader& reader) -> BakerModelDescriptionCut
{
    FO_STACK_TRACE_ENTRY();

    BakerModelDescriptionCut cut;
    cut.FileName = reader.ReadString();
    cut.Layers = reader.ReadSizedObjectVector<int32_t>();
    cut.Shapes = reader.ReadStringVector();
    cut.UnskinBone1 = reader.ReadString();
    cut.UnskinBone2 = reader.ReadString();
    cut.UnskinShape = reader.ReadString();
    cut.RevertUnskinShape = reader.Read<uint8_t>() != 0;
    return cut;
}

static auto ReadBakedModelDescriptionLinkForBounds(DataReader& reader) -> BakerModelDescriptionLink
{
    FO_STACK_TRACE_ENTRY();

    BakerModelDescriptionLink link;
    link.Layer = reader.Read<int32_t>();
    link.LayerValue = reader.Read<int32_t>();
    link.LinkBone = reader.ReadString();
    link.ChildName = reader.ReadString();
    link.IsParticles = reader.Read<uint8_t>() != 0;
    link.RotX = reader.Read<float32_t>();
    link.RotY = reader.Read<float32_t>();
    link.RotZ = reader.Read<float32_t>();
    link.MoveX = reader.Read<float32_t>();
    link.MoveY = reader.Read<float32_t>();
    link.MoveZ = reader.Read<float32_t>();
    link.ScaleX = reader.Read<float32_t>();
    link.ScaleY = reader.Read<float32_t>();
    link.ScaleZ = reader.Read<float32_t>();
    link.SpeedAjust = reader.Read<float32_t>();
    link.DisabledLayer = reader.ReadSizedObjectVector<int32_t>();
    link.DisabledMesh = reader.ReadStringVector();

    uint32_t texture_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < texture_count; i++) {
        string texture_name = reader.ReadString();
        string mesh_name = reader.ReadString();
        int32_t texture_index = reader.Read<int32_t>();
        link.TextureInfo.emplace_back(std::move(texture_name), std::move(mesh_name), texture_index);
    }

    uint32_t effect_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < effect_count; i++) {
        string effect_name = reader.ReadString();
        string mesh_name = reader.ReadString();
        link.EffectInfo.emplace_back(std::move(effect_name), std::move(mesh_name));
    }

    uint32_t cut_count = reader.Read<uint32_t>();

    for (uint32_t i = 0; i < cut_count; i++) {
        link.CutInfo.emplace_back(ReadBakedModelDescriptionCutForBounds(reader));
    }

    uint8_t has_geometry_value = reader.Read<uint8_t>();
    FO_VERIFY_AND_THROW(has_geometry_value <= uint8_t {1}, "Baked model link geometry flag is not 0 or 1", link.ChildName, has_geometry_value);
    bool has_geometry = has_geometry_value != 0;
    bool expected_geometry = !link.ChildName.empty() && !link.IsParticles;
    FO_VERIFY_AND_THROW(has_geometry == expected_geometry, "Baked model link geometry flag does not match its child type", link.ChildName, link.IsParticles, has_geometry);

    if (has_geometry) {
        link.Bounds = ModelBounds3D {
            .Min = reader.Read<vec3>(),
            .Max = reader.Read<vec3>(),
        };
    }

    return link;
}

static auto ReadBakedModelDescriptionForBounds(const FileSystem& baked_files, string_view fname) -> BakerModelDescription
{
    FO_STACK_TRACE_ENTRY();

    File file = baked_files.ReadFile(fname);

    if (!file) {
        throw ModelInfoBakerException("Baked model description for bounds is not readable", fname);
    }

    try {
        DataReader reader {file.GetDataSpan()};
        const_span<uint8_t> magic = reader.ReadBytes(MODEL_DESCRIPTION_MAGIC.size());

        if (!std::equal(magic.begin(), magic.end(), MODEL_DESCRIPTION_MAGIC.begin())) {
            throw ModelInfoBakerException("Invalid baked model description magic while calculating bounds", fname);
        }

        uint16_t schema = reader.Read<uint16_t>();
        uint16_t flags = reader.Read<uint16_t>();

        if (schema != MODEL_DESCRIPTION_SCHEMA_VERSION || (flags & ~MODEL_DESCRIPTION_SUPPORTED_FLAGS) != 0) {
            throw ModelInfoBakerException("Unsupported baked model description while calculating bounds", fname, schema, flags);
        }

        BakerModelDescription description;
        description.Model = reader.ReadString();
        description.DisableAnimationInterpolation = reader.Read<uint8_t>() != 0;
        description.DisableBackwardAnim = reader.Read<uint8_t>() != 0;
        description.ShadowDisabled = reader.Read<uint8_t>() != 0;
        (void)reader.Read<int32_t>();
        (void)reader.Read<int32_t>();
        (void)reader.Read<int32_t>();
        (void)reader.Read<int32_t>();
        description.RotationBone = reader.ReadString();
        description.DefaultLink = ReadBakedModelDescriptionLinkForBounds(reader);

        uint32_t link_count = reader.Read<uint32_t>();

        for (uint32_t i = 0; i < link_count; i++) {
            description.Links.emplace_back(ReadBakedModelDescriptionLinkForBounds(reader));
        }

        uint32_t animation_count = reader.Read<uint32_t>();

        for (uint32_t i = 0; i < animation_count; i++) {
            description.AnimationEntries.emplace_back(BakerModelDescriptionAnimationEntry {
                .StateAnim = reader.Read<int32_t>(),
                .ActionAnim = reader.Read<int32_t>(),
                .FileName = reader.ReadString(),
                .Name = reader.ReadString(),
            });
        }

        return description;
    }
    catch (const DataReadingException& ex) {
        throw ModelInfoBakerException("Invalid baked model description while calculating bounds", fname, ex.what());
    }
}

static auto MakeModelDescriptionLinkTransform(const BakerModelDescriptionLink& child_default_link, const BakerModelDescriptionLink& outer_link) -> mat44
{
    FO_STACK_TRACE_ENTRY();

    mat44 scale {1.0f};
    mat44 rotation {1.0f};
    mat44 translation {1.0f};

    auto apply_link = [&scale, &rotation, &translation](const BakerModelDescriptionLink& link) {
        if (link.ScaleX != 0.0f) {
            scale *= glm::scale(mat44 {1.0f}, vec3 {link.ScaleX, 1.0f, 1.0f});
        }
        if (link.ScaleY != 0.0f) {
            scale *= glm::scale(mat44 {1.0f}, vec3 {1.0f, link.ScaleY, 1.0f});
        }
        if (link.ScaleZ != 0.0f) {
            scale *= glm::scale(mat44 {1.0f}, vec3 {1.0f, 1.0f, link.ScaleZ});
        }
        if (link.RotX != 0.0f) {
            rotation *= glm::rotate(mat44 {1.0f}, -link.RotX * DEG_TO_RAD_FLOAT, vec3 {1.0f, 0.0f, 0.0f});
        }
        if (link.RotY != 0.0f) {
            rotation *= glm::rotate(mat44 {1.0f}, link.RotY * DEG_TO_RAD_FLOAT, vec3 {0.0f, 1.0f, 0.0f});
        }
        if (link.RotZ != 0.0f) {
            rotation *= glm::rotate(mat44 {1.0f}, link.RotZ * DEG_TO_RAD_FLOAT, vec3 {0.0f, 0.0f, 1.0f});
        }
        if (link.MoveX != 0.0f) {
            translation *= glm::translate(mat44 {1.0f}, vec3 {link.MoveX, 0.0f, 0.0f});
        }
        if (link.MoveY != 0.0f) {
            translation *= glm::translate(mat44 {1.0f}, vec3 {0.0f, link.MoveY, 0.0f});
        }
        if (link.MoveZ != 0.0f) {
            translation *= glm::translate(mat44 {1.0f}, vec3 {0.0f, 0.0f, -link.MoveZ});
        }
    };

    // Runtime applies the child description's default link first and then the outer attachment link, while keeping
    // translation, rotation, and scale in their separate accumulators. Reproduce that component order exactly
    apply_link(child_default_link);
    apply_link(outer_link);

    return translation * rotation * scale;
}

// Every attachment of one description is measured against the same clip set, so the enumeration, the parent
// hierarchy and each rigid link bone track are prepared once instead of once per attachment
static void CalculateModelDescriptionLinkBounds(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, async_launch_mode async_mode, const ModelMeshData& parent_model_mesh, BakerModelDescription& description, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    vector<size_t> link_indices;
    unordered_set<string> link_bones;

    for (size_t link_index = 0; link_index < description.Links.size(); link_index++) {
        const BakerModelDescriptionLink& link = description.Links[link_index];

        if (link.ChildName.empty() || link.IsParticles) {
            continue;
        }

        link_indices.emplace_back(link_index);

        if (!link.LinkBone.empty()) {
            link_bones.emplace(link.LinkBone);
        }
    }

    if (link_indices.empty()) {
        return;
    }

    vector<ModelDescriptionClip> clips = CollectModelDescriptionClips(model_sources, description, fname);
    optional<ModelBoundsSampler> parent_sampler;
    unordered_map<string, vector<optional<vector<mat44>>>> link_bone_tracks;

    if (!link_bones.empty()) {
        parent_sampler.emplace(parent_model_mesh, vector<string> {}, measurement);
        link_bone_tracks = SampleModelDescriptionLinkBoneTracks(*parent_sampler, clips, link_bones, async_mode);
    }

    vector<std::future<optional<ModelBounds3D>>> link_bakings;
    link_bakings.reserve(link_indices.size());

    nptr<const ModelBoundsSampler> parent = parent_sampler ? &*parent_sampler : nullptr;

    for (size_t link_index : link_indices) {
        string task_name = strex("BakeModelInfoBounds-{}-{}", fname, link_index);
        ptr<const BakerModelDescriptionLink> link = &description.Links[link_index];
        link_bakings.emplace_back(run_async(async_mode, task_name, [&source_files, &baked_files, &model_sources, measurement, &clips, &link_bone_tracks, parent, link, fname]() FO_DEFERRED { return CalculateModelDescriptionLinkBoundsEntry(source_files, baked_files, model_sources, measurement, clips, link_bone_tracks, parent, *link, fname); }));
    }

    // Every task is awaited before the first failure is rethrown, so no link keeps running past this scope
    std::exception_ptr link_error;
    vector<optional<ModelBounds3D>> link_bounds(link_indices.size());

    for (size_t i = 0; i < link_bakings.size(); i++) {
        try {
            link_bounds[i] = link_bakings[i].get();
        }
        catch (const std::exception&) {
            if (!link_error) {
                link_error = std::current_exception();
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (link_error) {
        std::rethrow_exception(link_error);
    }

    for (size_t i = 0; i < link_indices.size(); i++) {
        BakerModelDescriptionLink& link = description.Links[link_indices[i]];
        link.Bounds = link_bounds[i];

        if (!link.Bounds) {
            throw ModelInfoBakerException("Model geometry link bounds could not be calculated", link.ChildName, fname);
        }
    }
}

// Deduplicated by clip rather than by animation pair: two pairs playing the same clip pose the model identically
static auto CollectModelDescriptionClips(const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> vector<ModelDescriptionClip>
{
    FO_STACK_TRACE_ENTRY();

    vector<ModelDescriptionClip> result;
    set<pair<int32_t, int32_t>> selected_pairs;
    unordered_set<string> sampled_animations;

    for (const BakerModelDescriptionAnimationEntry& anim_entry : description.AnimationEntries) {
        if (!selected_pairs.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
            continue;
        }

        string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(fname).extract_dir().combine_path(anim_entry.FileName).str();
        string anim_name = anim_entry.Name;
        bool reversed = !anim_name.empty() && anim_name.front() == '~';

        if (reversed) {
            anim_name.erase(anim_name.begin());
        }

        string animation_key = strex("{}\n{}\n{}", anim_file, anim_name, reversed ? 1 : 0);

        if (!sampled_animations.emplace(animation_key).second) {
            continue;
        }

        shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, fname);
        const ModelAnimationSource& animation = GetModelSourceAnimation(*anim_source, anim_name);
        result.emplace_back(ModelDescriptionClip {.Source = std::move(anim_source), .Animation = &animation, .Reversed = reversed});
    }

    return result;
}

// A rigid attachment reads only where its bone travels, so one track answers every attachment on that bone
static auto SampleModelDescriptionLinkBoneTracks(const ModelBoundsSampler& parent_sampler, const vector<ModelDescriptionClip>& clips, const unordered_set<string>& link_bones, async_launch_mode async_mode) -> unordered_map<string, vector<optional<vector<mat44>>>>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> bones(link_bones.begin(), link_bones.end());
    std::ranges::sort(bones);

    // One flat slot per bone and clip, so each task owns its slot and no writer needs synchronization
    vector<optional<vector<mat44>>> tracks(bones.size() * clips.size());
    vector<std::future<void>> track_bakings;

    for (size_t bone_index = 0; bone_index < bones.size(); bone_index++) {
        for (size_t clip_index = 0; clip_index < clips.size(); clip_index++) {
            size_t track_index = bone_index * clips.size() + clip_index;
            string task_name = strex("BakeModelInfoBoneTrack-{}-{}", bones[bone_index], clip_index);
            track_bakings.emplace_back(run_async(async_mode, task_name, [&parent_sampler, &clips, &bones, &tracks, bone_index, clip_index, track_index]() FO_DEFERRED {
                const ModelDescriptionClip& clip = clips[clip_index];
                tracks[track_index] = parent_sampler.SampleBoneTransforms(*clip.Animation, clip.Reversed, bones[bone_index]);
            }));
        }
    }

    std::exception_ptr track_error;

    for (std::future<void>& track_baking : track_bakings) {
        try {
            track_baking.get();
        }
        catch (const std::exception&) {
            if (!track_error) {
                track_error = std::current_exception();
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (track_error) {
        std::rethrow_exception(track_error);
    }

    unordered_map<string, vector<optional<vector<mat44>>>> result;

    for (size_t bone_index = 0; bone_index < bones.size(); bone_index++) {
        auto first = tracks.begin() + numeric_cast<ptrdiff_t>(bone_index * clips.size());
        result.emplace(bones[bone_index], vector<optional<vector<mat44>>>(std::make_move_iterator(first), std::make_move_iterator(first + numeric_cast<ptrdiff_t>(clips.size()))));
    }

    return result;
}

static auto CalculateModelDescriptionLinkBoundsEntry(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, const vector<ModelDescriptionClip>& clips, const unordered_map<string, vector<optional<vector<mat44>>>>& link_bone_tracks, nptr<const ModelBoundsSampler> parent_sampler, const BakerModelDescriptionLink& link, string_view fname) -> optional<ModelBounds3D>
{
    FO_STACK_TRACE_ENTRY();

    BakerModelDescription child_description;
    bool child_is_description = strex(link.ChildName).get_file_extension() == "fo3d";

    if (child_is_description) {
        if (source_files.FindFileByPath(link.ChildName)) {
            BakerClientEngine link_engine(baked_files);
            ModelDescriptionParser parser(&source_files, &link_engine);
            auto [parsed_description, parsed_write_time] = parser.Parse(link.ChildName);
            ignore_unused(parsed_write_time);
            child_description = std::move(parsed_description);
        }
        else {
            child_description = ReadBakedModelDescriptionForBounds(baked_files, link.ChildName);
        }
    }
    else {
        child_description.Model = link.ChildName;
    }

    ModelMeshData child_model_mesh = ReadBakedModelMeshForBounds(baked_files, child_description, link.ChildName);
    vector<string> disabled_meshes = child_description.DefaultLink.DisabledMesh;
    disabled_meshes.insert(disabled_meshes.end(), link.DisabledMesh.begin(), link.DisabledMesh.end());
    child_description.DefaultLink.DisabledMesh = disabled_meshes;
    mat44 link_transform = MakeModelDescriptionLinkTransform(child_description.DefaultLink, link);
    optional<ModelBounds3D> result;

    try {
        if (link.LinkBone.empty()) {
            ModelBoundsSampler child_sampler {child_model_mesh, disabled_meshes, measurement};
            optional<ModelBounds3D> static_bounds = child_sampler.CalculateStaticBounds();

            if (static_bounds && !IncludeTransformedModelBounds(result, *static_bounds, link_transform)) {
                return std::nullopt;
            }

            for (const ModelDescriptionClip& clip : clips) {
                optional<ModelBounds3D> animation_bounds = child_sampler.CalculateAnimationBounds(*clip.Animation, clip.Reversed);

                if (!animation_bounds || !IncludeTransformedModelBounds(result, *animation_bounds, link_transform)) {
                    return std::nullopt;
                }
            }
        }
        else {
            FO_VERIFY_AND_THROW(parent_sampler, "Rigid attachment has no prepared parent hierarchy", link.ChildName, fname);
            optional<ModelBounds3D> child_bounds;

            if (child_is_description) {
                child_bounds = CalculateFo3dAggregateModelBounds(baked_files, model_sources, measurement, child_description, link.ChildName);
            }
            else {
                child_bounds = ModelBoundsSampler {child_model_mesh, disabled_meshes, measurement}.CalculateStaticBounds();
            }

            if (!child_bounds) {
                return std::nullopt;
            }

            optional<mat44> bind_pose_transform = parent_sampler->GetBindPoseBoneTransform(link.LinkBone);

            if (!bind_pose_transform) {
                return std::nullopt;
            }

            optional<ModelBounds3D> static_bounds = CalculateRigidAttachmentBounds({&*bind_pose_transform, 1}, *child_bounds, link_transform);

            if (!static_bounds || !IncludeModelBounds(result, *static_bounds)) {
                return std::nullopt;
            }

            const vector<optional<vector<mat44>>>& tracks = link_bone_tracks.at(link.LinkBone);

            for (size_t clip_index = 0; clip_index < clips.size(); clip_index++) {
                if (!tracks[clip_index]) {
                    return std::nullopt;
                }

                optional<ModelBounds3D> animation_bounds = CalculateRigidAttachmentBounds(*tracks[clip_index], *child_bounds, link_transform);

                if (!animation_bounds || !IncludeModelBounds(result, *animation_bounds)) {
                    return std::nullopt;
                }
            }
        }
    }
    catch (const ModelBoundsException& ex) {
        throw ModelInfoBakerException("Failed to calculate model link bounds", link.ChildName, fname, ex.what());
    }

    if (!result) {
        throw ModelInfoBakerException("Model link bounds could not be calculated", link.ChildName, fname);
    }

    return CalculateGuardedModelBounds(*result);
}

static auto CalculateFo3dAggregateModelBounds(const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, const BakerModelDescription& description, string_view fname) -> ModelBounds3D
{
    FO_STACK_TRACE_ENTRY();

    ModelMeshData model_mesh = ReadBakedModelMeshForBounds(baked_files, description, fname);
    DescriptionBoundsSampler sampler {model_mesh, description, measurement};
    optional<ModelBounds3D> model_bounds;
    set<pair<int32_t, int32_t>> seen;
    uint64_t disabled_mesh_retries = 0;

    for (const BakerModelDescriptionAnimationEntry& anim_entry : description.AnimationEntries) {
        if (!seen.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
            continue;
        }

        string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(fname).extract_dir().combine_path(anim_entry.FileName).str();
        shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, fname);
        string anim_name = anim_entry.Name;
        bool reversed = !anim_name.empty() && anim_name.front() == '~';

        if (reversed) {
            anim_name.erase(anim_name.begin());
        }

        float32_t clip_duration = GetModelSourceAnimationDuration(*anim_source, anim_name);

        if (clip_duration <= 0.0f) {
            continue;
        }

        const ModelAnimationSource& animation = GetModelSourceAnimation(*anim_source, anim_name);
        optional<ModelBounds3D> calculated_bounds = sampler.CalculateAnimationBounds(animation, reversed, anim_entry.StateAnim, anim_entry.ActionAnim, fname, disabled_mesh_retries);

        if (!calculated_bounds) {
            throw ModelInfoBakerException("Animation bounds could not be calculated", anim_entry.StateAnim, anim_entry.ActionAnim, fname);
        }

        FO_VERIFY_AND_THROW(IncludeModelBounds(model_bounds, *calculated_bounds), "Calculated model animation bounds are invalid", fname, anim_entry.StateAnim, anim_entry.ActionAnim);
    }

    if (!model_bounds) {
        model_bounds = sampler.CalculateStaticBounds(fname);
    }

    return *model_bounds;
}

static void ValidateFo3dAggregateModelBounds(const BakingSettings& settings, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    ValidateAggregateModelBoundsExtent(settings, CalculateFo3dAggregateModelBounds(baked_files, model_sources, GetModelBoundsMeasurement(settings), description, fname), fname);
}

static auto GetModelBoundsMeasurement(const BakingSettings& settings) -> ModelBoundsMeasurement
{
    FO_STACK_TRACE_ENTRY();

    return settings.PreciseModelBounds ? ModelBoundsMeasurement::PerVertex : ModelBoundsMeasurement::PerBoneEnvelope;
}

// The client sizes its lighting frame from this envelope, so a centimetre-space aggregate must fail the bake
static void ValidateAggregateModelBoundsExtent(const BakingSettings& settings, const ModelBounds3D& bounds, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelWorldExtent(settings, bounds, "Model bounds reach beyond the authored world extent; export it in the same units as the other models", "Model bounds stay below the authored world extent; export it in the same units as the other models", "aggregate ModelBounds", fname);
}

static void ValidateModelDescriptionLinkData(const FileCollection& source_files, const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& target_info, nptr<const BakedModelMeshInfo> parent_info, const BakerModelDescriptionLink& link, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (link.SpeedAjust < 0.0f) {
        throw ModelInfoBakerException("Negative Speed value for model", fname, target_info.FileName);
    }

    for (int32_t disabled_layer : link.DisabledLayer) {
        if (disabled_layer < 0 || disabled_layer >= numeric_cast<int32_t>(MODEL_LAYERS_COUNT)) {
            throw ModelInfoBakerException("Disabled layer is out of range", disabled_layer, fname, MODEL_LAYERS_COUNT);
        }
    }

    for (const auto& [texture_name, mesh_name, texture_index] : link.TextureInfo) {
        if (texture_index < 0 || texture_index >= numeric_cast<int32_t>(MODEL_MAX_TEXTURES)) {
            throw ModelInfoBakerException("Texture index is out of range", texture_index, fname, MODEL_MAX_TEXTURES);
        }

        ValidateModelDescriptionMeshReference(target_info, mesh_name, "Texture", fname);

        if (strex(texture_name).starts_with("Parent")) {
            if (!parent_info) {
                throw ModelInfoBakerException("Parent texture is used without parent model context", texture_name, fname);
            }

            string parent_mesh = string(texture_name.substr(6));

            if (parent_mesh.starts_with("_")) {
                parent_mesh.erase(parent_mesh.begin());
            }

            ValidateModelDescriptionMeshReference(*parent_info, parent_mesh, "Texture", fname);
        }
        else {
            ValidateModelDescriptionTexture(baked_files, target_info, texture_name, "Texture", fname);
        }
    }

    for (const auto& [effect_name, mesh_name] : link.EffectInfo) {
        ValidateModelDescriptionMeshReference(target_info, mesh_name, "Effect", fname);

        if (strex(effect_name).starts_with("Parent")) {
            if (!parent_info) {
                throw ModelInfoBakerException("Parent effect is used without parent model context", effect_name, fname);
            }

            string parent_mesh = string(effect_name.substr(6));

            if (parent_mesh.starts_with("_")) {
                parent_mesh.erase(parent_mesh.begin());
            }

            ValidateModelDescriptionMeshReference(*parent_info, parent_mesh, "Effect", fname);
        }
        else {
            ValidateModelDescriptionEffect(baked_files, effect_name, "Effect", fname);
        }
    }

    for (const BakerModelDescriptionCut& cut : link.CutInfo) {
        ValidateModelDescriptionCut(source_files, baked_files, mesh_cache, target_info, cut, fname);
    }
}

static void ValidateModelDescriptionCut(const FileCollection& source_files, const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& target_info, const BakerModelDescriptionCut& cut, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    const BakedModelMeshInfo& cut_info = GetBakedModelMeshInfo(baked_files, mesh_cache, cut.FileName);
    if (source_files.FindFileByPath(cut.FileName)) {
        ValidateBakedModelMeshFreshness(source_files, cut_info, fname);
    }

    if (cut.Shapes.empty()) {
        throw ModelInfoBakerException("Cut has no shapes", cut.FileName, fname);
    }

    if (cut.Layers.empty()) {
        throw ModelInfoBakerException("Cut has no layers", cut.FileName, fname);
    }

    for (int32_t layer : cut.Layers) {
        if (layer < 0 || layer >= numeric_cast<int32_t>(MODEL_LAYERS_COUNT)) {
            throw ModelInfoBakerException("Cut has out of range layer", cut.FileName, fname, layer);
        }
    }

    for (const string& shape : cut.Shapes) {
        ValidateModelDescriptionDrawBoneReference(cut_info, shape, "Cut", fname);
    }

    if (cut.UnskinBone1.empty() != cut.UnskinBone2.empty()) {
        throw ModelInfoBakerException("Cut must specify both unskin bones or none", cut.FileName, fname);
    }
    if (!cut.UnskinShape.empty() && (cut.UnskinBone1.empty() || cut.UnskinBone2.empty())) {
        throw ModelInfoBakerException("Cut specifies unskin shape without both unskin bones", cut.FileName, fname);
    }

    ValidateModelDescriptionBoneReference(target_info, cut.UnskinBone1, "Cut", fname);
    ValidateModelDescriptionBoneReference(target_info, cut.UnskinBone2, "Cut", fname);
    ValidateModelDescriptionDrawBoneReference(cut_info, cut.UnskinShape, "Cut", fname);
}

static void ValidateModelDescriptionTexture(const FileSystem& baked_files, const BakedModelMeshInfo& model_info, string_view texture_name, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (texture_name.empty() || strex(texture_name).starts_with("Parent")) {
        return;
    }

    string texture_path = strex(model_info.FileName).extract_dir().combine_path(texture_name);
    ValidateModelDescriptionBakedFileExists(baked_files, texture_path, "Texture", fname);
    ignore_unused(token);
}

static void ValidateModelDescriptionEffect(const FileSystem& baked_files, string_view effect_name, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (effect_name.empty() || strex(effect_name).starts_with("Parent")) {
        return;
    }

    ValidateModelDescriptionBakedFileExists(baked_files, effect_name, "Effect", fname);
    ignore_unused(token);
}

static void ValidateModelDescriptionBakedFileExists(const FileSystem& baked_files, string_view path, string_view kind, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (!baked_files.IsFileExists(path)) {
        throw ModelInfoBakerException("Referenced file not found in baked resources", kind, path, fname);
    }
}

static void ValidateModelDescriptionDrawBoneReference(const BakedModelMeshInfo& info, string_view bone_name, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (!bone_name.empty() && info.DrawBones.count(string(bone_name)) == 0) {
        throw ModelInfoBakerException("Draw bone for token not found in model", bone_name, token, fname, info.FileName);
    }
}

static void ValidateModelDescriptionBoneReference(const BakedModelMeshInfo& info, string_view bone_name, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (!bone_name.empty() && info.Bones.count(string(bone_name)) == 0) {
        throw ModelInfoBakerException("Bone for token not found in model", bone_name, token, fname, info.FileName);
    }
}

static void ValidateModelDescriptionMeshReference(const BakedModelMeshInfo& info, string_view mesh_name, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (!mesh_name.empty() && info.DrawBones.count(string(mesh_name)) == 0) {
        throw ModelInfoBakerException("Mesh for token not found in model", mesh_name, token, fname, info.FileName);
    }
}

static void ValidateModelDescriptionAnimPair(const NameResolver& name_resolver, int32_t state_anim, int32_t action_anim, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    ValidateModelDescriptionEnumValue(name_resolver, "CritterStateAnim", state_anim, token, fname);
    ValidateModelDescriptionEnumValue(name_resolver, "CritterActionAnim", action_anim, token, fname);
}

static void ValidateModelDescriptionEnumValue(const NameResolver& name_resolver, string_view enum_name, int32_t value, string_view token, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    bool metadata_missing = false;
    (void)name_resolver.ResolveEnumValueName(enum_name, 0, &metadata_missing);

    if (metadata_missing) {
        return;
    }

    bool failed = false;
    (void)name_resolver.ResolveEnumValueName(enum_name, value, &failed);

    if (failed) {
        throw ModelInfoBakerException("Invalid enum value for token", enum_name, value, token, fname);
    }
}

static auto GetBakedModelMeshInfo(const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& cache, string_view path) -> const BakedModelMeshInfo&
{
    FO_STACK_TRACE_ENTRY();

    string key {path};

    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }

    auto [it, inserted] = cache.emplace(key, ReadBakedModelMeshInfo(baked_files, path));
    ignore_unused(inserted);
    return it->second;
}

static auto ReadBakedModelMeshInfo(const FileSystem& baked_files, string_view path) -> BakedModelMeshInfo
{
    FO_STACK_TRACE_ENTRY();

    if (!baked_files.IsFileExists(path)) {
        throw ModelInfoBakerException("Baked model mesh not found", path);
    }

    File file = baked_files.ReadFile(path);

    if (!file) {
        throw ModelInfoBakerException("Baked model mesh not readable", path);
    }

    BakedModelMeshInfo info;
    info.FileName = path;
    info.WriteTime = file.GetWriteTime();
    info.Skeleton.FileName = path;

    try {
        auto reader = DataReader(file.GetDataSpan());
        ModelMeshData mesh_data = ReadModelMeshData(reader, path);
        FO_VERIFY_AND_THROW(mesh_data.RootBone, "Decoded model mesh has no root bone", path);
        CollectBakedModelMeshInfo(*mesh_data.RootBone, info, {});
        info.StaticBounds = ModelBoundsSampler {mesh_data, {}, ModelBoundsMeasurement::PerVertex}.CalculateStaticBounds();
    }
    catch (const std::exception& ex) {
        throw ModelInfoBakerException("Invalid baked model mesh", path, ex.what());
    }

    return info;
}

static void ValidateBakedModelMeshFreshness(const FileCollection& source_files, const BakedModelMeshInfo& info, string_view owner)
{
    FO_STACK_TRACE_ENTRY();

    File source_file = source_files.FindFileByPath(info.FileName);

    if (!source_file) {
        throw ModelInfoBakerException("Model source referenced by owner was not found", info.FileName, owner);
    }
    if (source_file.GetWriteTime() > info.WriteTime) {
        throw ModelInfoBakerException("Baked model mesh referenced by owner is older than its source; run ModelMesh before ModelInfo", info.FileName, owner);
    }
}

static auto GetModelSourceAsset(const ModelSourceAssetCache& model_sources, string_view path, string_view owner) -> shared_ptr<const ModelSourceAsset>
{
    FO_STACK_TRACE_ENTRY();

    try {
        return model_sources.Get(path);
    }
    catch (const std::exception& ex) {
        throw ModelInfoBakerException("Unable to load model source referenced by owner", path, owner, ex.what());
    }
}

static auto ModelSourceAssetHasAnimation(const ModelSourceAsset& asset, string_view anim_name) -> bool
{
    FO_STACK_TRACE_ENTRY();

    if (anim_name == "Base") {
        return !asset.Animations.empty();
    }

    for (const ModelAnimationSource& animation : asset.Animations) {
        if (strex(animation.Name).compare_ignore_case(anim_name)) {
            return true;
        }
    }

    return false;
}

static auto GetModelSourceAnimation(const ModelSourceAsset& asset, string_view anim_name) -> const ModelAnimationSource&
{
    FO_STACK_TRACE_ENTRY();

    if (anim_name == "Base") {
        FO_VERIFY_AND_THROW(!asset.Animations.empty(), "Base animation requested from a model source without animations", asset.FileName);
        return asset.Animations.front();
    }

    for (const ModelAnimationSource& animation : asset.Animations) {
        if (strex(animation.Name).compare_ignore_case(anim_name)) {
            return animation;
        }
    }

    throw ModelInfoBakerException("Animation not found in model source", anim_name, asset.FileName);
}

static auto GetModelSourceAnimationDuration(const ModelSourceAsset& asset, string_view anim_name) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    return ModelSourceAssetHasAnimation(asset, anim_name) ? GetModelSourceAnimation(asset, anim_name).Duration : 0.0f;
}

static void BakeModelAnimationInfo(const BakingContext& ctx, const FileCollection& files, const ModelSourceAssetCache& model_sources, string_view target_path)
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view output_path = "ModelAnimationInfo.foinfo";

    if (!target_path.empty() && target_path != output_path) {
        return;
    }

    // Collect all (non-template) model descriptions and the newest write time across them and their includes
    vector<File> fo3d_files;
    uint64_t max_write_time = 0;
    BakerClientEngine client_engine(*ctx.BakedFiles);

    for (const FileHeader& file_header : files) {
        if (strex(file_header.GetPath()).get_file_extension() != "fo3d") {
            continue;
        }
        if (IsModelDescriptionTemplateFile(file_header.GetPath())) {
            continue;
        }

        fo3d_files.emplace_back(File::Load(file_header));
        max_write_time = std::max(max_write_time, GetModelDescriptionMaxWriteTime(files, client_engine, file_header.GetPath()));
    }

    if (fo3d_files.empty()) {
        return;
    }
    if (ctx.BakeChecker && !ctx.BakeChecker(output_path, max_write_time)) {
        return;
    }

    // Deterministic section order
    std::sort(fo3d_files.begin(), fo3d_files.end(), [](const File& a, const File& b) { return a.GetPath() < b.GetPath(); });

    async_launch_mode async_mode = ctx.ForceSyncMode.value_or(ctx.Settings->SingleThreadBaking) ? launch_deferred_only : launch_async_and_deferred;
    ModelBoundsMeasurement measurement = GetModelBoundsMeasurement(*ctx.Settings);
    vector<std::future<ModelAnimationInfoSection>> section_bakings;
    section_bakings.reserve(fo3d_files.size());

    for (const File& file : fo3d_files) {
        string task_name = strex("BakeModelAnimationInfo-{}", file.GetPath());
        section_bakings.emplace_back(run_async(async_mode, task_name, [&ctx, &files, &model_sources, measurement, async_mode, &file]() FO_DEFERRED {
            BakerClientEngine section_engine(*ctx.BakedFiles);
            return BakeModelAnimationInfoSection(ctx, files, section_engine, model_sources, measurement, async_mode, file);
        }));
    }

    // Every section is awaited before the first failure is rethrown, so no model keeps running past this scope
    vector<ModelAnimationInfoSection> sections(section_bakings.size());
    std::exception_ptr section_error;

    for (size_t i = 0; i < section_bakings.size(); i++) {
        try {
            sections[i] = section_bakings[i].get();
        }
        catch (const std::exception&) {
            if (!section_error) {
                section_error = std::current_exception();
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (section_error) {
        std::rethrow_exception(section_error);
    }

    ModelAnimationInfoBakingStats stats;
    string config_text;

    for (const ModelAnimationInfoSection& section : sections) {
        config_text += section.ConfigText;
        stats.ModelSections += section.Stats.ModelSections;
        stats.ModelBounds += section.Stats.ModelBounds;
        stats.AnimationEntries += section.Stats.AnimationEntries;
        stats.AnimationBounds += section.Stats.AnimationBounds;
        stats.BoundsCalculations += section.Stats.BoundsCalculations;
        stats.BoundsCacheHits += section.Stats.BoundsCacheHits;
        stats.ViewBoundsIdle += section.Stats.ViewBoundsIdle;
        stats.ViewBoundsFallback += section.Stats.ViewBoundsFallback;

        for (size_t i = 0; i < stats.AnimationBoundsMaxExtent.size(); i++) {
            stats.AnimationBoundsMaxExtent[i] += section.Stats.AnimationBoundsMaxExtent[i];
        }
    }

    auto data = vector<uint8_t>(config_text.begin(), config_text.end());
    ctx.WriteData(output_path, data);

    if (ctx.Report) {
        shared_ptr<BakingReport> report = ctx.Report;
        report->AddCounter(ctx.PackName, ctx.BakerName, "modelSections", stats.ModelSections);
        report->AddCounter(ctx.PackName, ctx.BakerName, "modelBounds", stats.ModelBounds);
        report->AddCounter(ctx.PackName, ctx.BakerName, "animationEntries", stats.AnimationEntries);
        report->AddCounter(ctx.PackName, ctx.BakerName, "animationBounds", stats.AnimationBounds);
        report->AddCounter(ctx.PackName, ctx.BakerName, "boundsCalculations", stats.BoundsCalculations);
        report->AddCounter(ctx.PackName, ctx.BakerName, "boundsCacheHits", stats.BoundsCacheHits);
        report->AddCounter(ctx.PackName, ctx.BakerName, "viewBoundsIdle", stats.ViewBoundsIdle);
        report->AddCounter(ctx.PackName, ctx.BakerName, "viewBoundsFallback", stats.ViewBoundsFallback);

        constexpr array<string_view, 6> extent_buckets {"<1", "1-2", "2-3", "3-5", "5-10", "10+"};

        for (size_t i = 0; i < extent_buckets.size(); i++) {
            report->AddHistogramValue(ctx.PackName, ctx.BakerName, "animationBoundsMaxExtent", extent_buckets[i], stats.AnimationBoundsMaxExtent[i]);
        }
    }
}

static auto BakeModelAnimationInfoSection(const BakingContext& ctx, const FileCollection& files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, ModelBoundsMeasurement measurement, async_launch_mode async_mode, const File& file) -> ModelAnimationInfoSection
{
    FO_STACK_TRACE_ENTRY();

    ModelDescriptionParser parser(&files, &name_resolver);
    auto [description, parsed_write_time] = parser.Parse(file.GetPath());
    ignore_unused(parsed_write_time);

    ModelAnimationInfoSection section;
    ModelAnimationInfoBakingStats& stats = section.Stats;
    unordered_map<int32_t, int32_t> state_anim_equals;
    unordered_map<int32_t, int32_t> action_anim_equals;
    vector<tuple<int32_t, int32_t, int32_t>> raw_durations;
    string states;
    string actions;
    string durations;
    string bounds_states;
    string bounds_actions;
    string bounds_min_x;
    string bounds_min_y;
    string bounds_min_z;
    string bounds_max_x;
    string bounds_max_y;
    string bounds_max_z;

    ModelMeshData model_mesh = ReadBakedModelMeshForBounds(*ctx.BakedFiles, description, file.GetPath());
    DescriptionBoundsSampler sampler {model_mesh, description, measurement};
    optional<ModelBounds3D> model_bounds;
    optional<ModelBounds3D> view_bounds;
    int32_t view_bounds_priority = -1;

    for (const auto& [from, to] : description.StateAnimEquals) {
        state_anim_equals.try_emplace(from, to);
    }
    for (const auto& [from, to] : description.ActionAnimEquals) {
        action_anim_equals.try_emplace(from, to);
    }

    vector<ModelAnimationInfoEntry> entries = CollectModelAnimationInfoEntries(model_sources, description, file.GetPath(), stats);
    vector<optional<ModelBounds3D>> clip_bounds = SampleModelAnimationInfoClipBounds(sampler, entries, async_mode, file.GetPath(), stats);

    for (const ModelAnimationInfoEntry& entry : entries) {
        raw_durations.emplace_back(entry.StateAnim, entry.ActionAnim, entry.DurationMs);

        const optional<ModelBounds3D>& bounds = clip_bounds[entry.ClipIndex];

        if (!bounds) {
            throw ModelInfoBakerException("Animation bounds could not be calculated", entry.StateAnim, entry.ActionAnim, file.GetPath());
        }

        FO_VERIFY_AND_THROW(IncludeModelBounds(model_bounds, *bounds), "Calculated model animation bounds are invalid", file.GetPath(), entry.StateAnim, entry.ActionAnim);

        bool idle = entry.ActionAnim == static_cast<int32_t>(CritterActionAnim::Idle);
        bool unarmed_idle = idle && entry.StateAnim == static_cast<int32_t>(CritterStateAnim::Unarmed);
        int32_t view_priority = unarmed_idle ? 2 : idle ? 1 : 0;

        if (view_priority > view_bounds_priority) {
            view_bounds = *bounds;
            view_bounds_priority = view_priority;
        }

        stats.AnimationBounds++;
        bounds_states += strex(" {}", entry.StateAnim);
        bounds_actions += strex(" {}", entry.ActionAnim);
        bounds_min_x += strex(" {}", bounds->Min.x);
        bounds_min_y += strex(" {}", bounds->Min.y);
        bounds_min_z += strex(" {}", bounds->Min.z);
        bounds_max_x += strex(" {}", bounds->Max.x);
        bounds_max_y += strex(" {}", bounds->Max.y);
        bounds_max_z += strex(" {}", bounds->Max.z);

        float32_t max_extent = std::max({bounds->Max.x - bounds->Min.x, bounds->Max.y - bounds->Min.y, bounds->Max.z - bounds->Min.z});
        size_t bucket = max_extent < 1.0f ? 0 : max_extent < 2.0f ? 1 : max_extent < 3.0f ? 2 : max_extent < 5.0f ? 3 : max_extent < 10.0f ? 4 : 5;
        stats.AnimationBoundsMaxExtent[bucket]++;
    }

    // Mirrors ModelInformation::GetAnimationIndexEx, where an alias outranks an exact entry, and
    // materializes every resolvable pair so common runtimes answer without the client-only description
    set<pair<int32_t, int32_t>> output_pairs;

    for (const auto& [state_anim, action_anim, duration_ms] : raw_durations) {
        vector<int32_t> resolved_state_inputs;
        vector<int32_t> resolved_action_inputs;
        set<int32_t> seen_state_inputs;
        set<int32_t> seen_action_inputs;

        if (state_anim_equals.count(state_anim) == 0) {
            resolved_state_inputs.emplace_back(state_anim);
            seen_state_inputs.emplace(state_anim);
        }
        for (const auto& [from, to] : description.StateAnimEquals) {
            auto it = state_anim_equals.find(from);

            if (it != state_anim_equals.end() && it->second == to && to == state_anim && seen_state_inputs.emplace(from).second) {
                resolved_state_inputs.emplace_back(from);
            }
        }

        if (action_anim_equals.count(action_anim) == 0) {
            resolved_action_inputs.emplace_back(action_anim);
            seen_action_inputs.emplace(action_anim);
        }
        for (const auto& [from, to] : description.ActionAnimEquals) {
            auto it = action_anim_equals.find(from);

            if (it != action_anim_equals.end() && it->second == to && to == action_anim && seen_action_inputs.emplace(from).second) {
                resolved_action_inputs.emplace_back(from);
            }
        }

        for (int32_t resolved_state : resolved_state_inputs) {
            for (int32_t resolved_action : resolved_action_inputs) {
                const auto [it, inserted] = output_pairs.emplace(resolved_state, resolved_action);
                ignore_unused(it);
                FO_VERIFY_AND_THROW(inserted, "Model animation aliases resolve to duplicate output entry", file.GetPath(), resolved_state, resolved_action);

                states += strex(" {}", resolved_state);
                actions += strex(" {}", resolved_action);
                durations += strex(" {}", duration_ms);
            }
        }
    }

    if (!model_bounds) {
        model_bounds = sampler.CalculateStaticBounds(file.GetPath());
        view_bounds = model_bounds;
    }

    if (!view_bounds) {
        throw ModelInfoBakerException("Model view bounds were not selected", file.GetPath());
    }

    ValidateAggregateModelBoundsExtent(*ctx.Settings, *model_bounds, file.GetPath());

    stats.ModelSections++;
    stats.ModelBounds++;

    if (view_bounds_priority >= 1) {
        stats.ViewBoundsIdle++;
    }
    else {
        stats.ViewBoundsFallback++;
    }

    string& config_text = section.ConfigText;
    config_text += strex("[{}]\n", file.GetPath());
    config_text += strex("BoundsVersion = {}\n", MODEL_BOUNDS_VERSION);
    config_text += strex("ModelBoundsMinX = {}\n", model_bounds->Min.x);
    config_text += strex("ModelBoundsMinY = {}\n", model_bounds->Min.y);
    config_text += strex("ModelBoundsMinZ = {}\n", model_bounds->Min.z);
    config_text += strex("ModelBoundsMaxX = {}\n", model_bounds->Max.x);
    config_text += strex("ModelBoundsMaxY = {}\n", model_bounds->Max.y);
    config_text += strex("ModelBoundsMaxZ = {}\n", model_bounds->Max.z);
    config_text += strex("ViewBoundsMinX = {}\n", view_bounds->Min.x);
    config_text += strex("ViewBoundsMinY = {}\n", view_bounds->Min.y);
    config_text += strex("ViewBoundsMinZ = {}\n", view_bounds->Min.z);
    config_text += strex("ViewBoundsMaxX = {}\n", view_bounds->Max.x);
    config_text += strex("ViewBoundsMaxY = {}\n", view_bounds->Max.y);
    config_text += strex("ViewBoundsMaxZ = {}\n", view_bounds->Max.z);

    if (!states.empty()) {
        config_text += strex("StateAnimations ={}\n", states);
        config_text += strex("ActionAnimations ={}\n", actions);
        config_text += strex("DurationsMs ={}\n", durations);
    }
    if (!bounds_states.empty()) {
        config_text += strex("BoundsStateAnimations ={}\n", bounds_states);
        config_text += strex("BoundsActionAnimations ={}\n", bounds_actions);
        config_text += strex("BoundsMinX ={}\n", bounds_min_x);
        config_text += strex("BoundsMinY ={}\n", bounds_min_y);
        config_text += strex("BoundsMinZ ={}\n", bounds_min_z);
        config_text += strex("BoundsMaxX ={}\n", bounds_max_x);
        config_text += strex("BoundsMaxY ={}\n", bounds_max_y);
        config_text += strex("BoundsMaxZ ={}\n", bounds_max_z);
    }

    config_text += "\n";
    return section;
}

// Resolves the emitted duration of every animation pair and points it at the clip that poses it. Two pairs
// playing one clip share a bounds calculation, which is what the cache-hit counter reports
static auto CollectModelAnimationInfoEntries(const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname, ModelAnimationInfoBakingStats& stats) -> vector<ModelAnimationInfoEntry>
{
    FO_STACK_TRACE_ENTRY();

    vector<ModelAnimationInfoEntry> result;
    set<pair<int32_t, int32_t>> seen;
    unordered_map<string, size_t> clip_indices;

    for (const BakerModelDescriptionAnimationEntry& anim_entry : description.AnimationEntries) {
        if (!seen.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
            continue;
        }

        string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(fname).extract_dir().combine_path(anim_entry.FileName).str();
        shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, fname);

        string anim_name = anim_entry.Name;
        bool reversed = !anim_name.empty() && anim_name.front() == '~';

        if (reversed) {
            anim_name.erase(anim_name.begin());
        }

        float32_t clip_duration = GetModelSourceAnimationDuration(*anim_source, anim_name);

        if (clip_duration <= 0.0f) {
            continue;
        }

        // Authored playback speed scales the cycle (faster speed -> shorter real cycle). The runtime
        // moving-speed factor is applied separately at play time and must not be baked in here
        float32_t speed = 1.0f;

        for (const auto& [anim_pair, anim_speed] : description.AnimSpeed) {
            if (anim_pair.first == anim_entry.StateAnim && anim_pair.second == anim_entry.ActionAnim) {
                speed = anim_speed;
                break;
            }
        }

        if (speed <= 0.0f || !std::isfinite(1.0f / speed)) {
            throw ModelInfoBakerException("Animation speed must be positive with a finite reciprocal", anim_entry.StateAnim, anim_entry.ActionAnim, fname);
        }

        double duration_milliseconds = static_cast<double>(clip_duration) / static_cast<double>(speed) * 1000.0;

        if (!std::isfinite(duration_milliseconds) || duration_milliseconds <= 0.0 || duration_milliseconds > static_cast<double>(std::numeric_limits<int32_t>::max())) {
            throw ModelInfoBakerException("Animation duration is outside the millisecond output range", anim_entry.StateAnim, anim_entry.ActionAnim, fname, clip_duration, speed);
        }

        int32_t duration_ms = iround<int32_t>(duration_milliseconds);

        // The runtime model-anim-info load rejects a non-positive duration, so a sub-millisecond effective
        // cycle that rounds down to zero must fail here rather than bake a manifest the client cannot load
        if (duration_ms <= 0) {
            throw ModelInfoBakerException("Animation duration rounds to a non-positive millisecond value", anim_entry.StateAnim, anim_entry.ActionAnim, fname, clip_duration, speed);
        }

        stats.AnimationEntries++;

        string clip_key = strex("{}\n{}\n{}", anim_file, anim_name, reversed ? 1 : 0);
        auto [clip_it, inserted] = clip_indices.emplace(clip_key, clip_indices.size());

        if (inserted) {
            stats.BoundsCalculations++;
        }
        else {
            stats.BoundsCacheHits++;
        }

        const ModelAnimationSource& animation = GetModelSourceAnimation(*anim_source, anim_name);
        result.emplace_back(ModelAnimationInfoEntry {
            .StateAnim = anim_entry.StateAnim,
            .ActionAnim = anim_entry.ActionAnim,
            .DurationMs = duration_ms,
            .ClipIndex = clip_it->second,
            .Source = std::move(anim_source),
            .Animation = &animation,
            .Reversed = reversed,
        });
    }

    return result;
}

// A model with hundreds of clips is the whole cost of this baker, so the clips of one model are measured in
// parallel and the failure of the first entry that needs a clip is what the caller reports
static auto SampleModelAnimationInfoClipBounds(const DescriptionBoundsSampler& sampler, const vector<ModelAnimationInfoEntry>& entries, async_launch_mode async_mode, string_view fname, ModelAnimationInfoBakingStats& stats) -> vector<optional<ModelBounds3D>>
{
    FO_STACK_TRACE_ENTRY();

    size_t clip_count = 0;

    for (const ModelAnimationInfoEntry& entry : entries) {
        clip_count = std::max(clip_count, entry.ClipIndex + 1);
    }

    vector<optional<ModelBounds3D>> result(clip_count);
    vector<uint64_t> retries(clip_count);
    vector<std::future<void>> clip_bakings;
    vector<bool> scheduled(clip_count);

    // One task owns one clip slot, so the shared result and retry buffers need no further synchronization
    for (const ModelAnimationInfoEntry& entry_ : entries) {
        if (scheduled[entry_.ClipIndex]) {
            continue;
        }

        scheduled[entry_.ClipIndex] = true;
        string task_name = strex("BakeModelAnimationInfoBounds-{}-{}", fname, entry_.ClipIndex);
        ptr<const ModelAnimationInfoEntry> entry = &entry_;
        clip_bakings.emplace_back(run_async(async_mode, task_name, [&sampler, &result, &retries, entry, fname]() FO_DEFERRED {
            uint64_t clip_retries = 0;
            result[entry->ClipIndex] = sampler.CalculateAnimationBounds(*entry->Animation, entry->Reversed, entry->StateAnim, entry->ActionAnim, fname, clip_retries);
            retries[entry->ClipIndex] = clip_retries;
        }));
    }

    // Every clip is awaited before the first failure is rethrown, so no clip keeps running past this scope
    std::exception_ptr clip_error;

    for (std::future<void>& clip_baking : clip_bakings) {
        try {
            clip_baking.get();
        }
        catch (const std::exception&) {
            if (!clip_error) {
                clip_error = std::current_exception();
            }
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (clip_error) {
        std::rethrow_exception(clip_error);
    }

    for (uint64_t clip_retries : retries) {
        stats.BoundsCalculations += clip_retries;
    }

    return result;
}

static void CollectBakedModelMeshInfo(const ModelMeshBoneData& bone, BakedModelMeshInfo& info, const vector<string>& parent_hierarchy)
{
    FO_STACK_TRACE_ENTRY();

    vector<string> hierarchy = parent_hierarchy;
    hierarchy.emplace_back(bone.Name);

    if (!bone.Name.empty()) {
        info.Bones.emplace(bone.Name);
    }

    info.Skeleton.Joints.emplace_back(ModelSkeletonJoint {bone.Name, hierarchy, bone.TransformationMatrix});

    if (bone.AttachedMesh) {
        info.DrawBonesCount++;

        if (!bone.Name.empty()) {
            info.DrawBones.emplace(bone.Name);
        }

        if (!bone.AttachedMesh->DiffuseTexture.empty()) {
            info.DiffuseTextures.emplace_back(bone.AttachedMesh->DiffuseTexture);
        }

        for (const string& skin_bone : bone.AttachedMesh->SkinBoneNames) {
            if (!skin_bone.empty()) {
                info.SkinBoneRefs.emplace_back(skin_bone);
            }
            else if (!bone.Name.empty()) {
                info.SkinBoneRefs.emplace_back(bone.Name);
            }
        }
    }

    for (const auto& child : bone.Children) {
        CollectBakedModelMeshInfo(*child, info, hierarchy);
    }
}

void BakerModelDescription::Save(DataWriter& writer) const
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteString(Model);
    writer.Write<uint8_t>(DisableAnimationInterpolation ? uint8_t {1} : uint8_t {0});
    writer.Write<uint8_t>(DisableBackwardAnim ? uint8_t {1} : uint8_t {0});
    writer.Write<uint8_t>(ShadowDisabled ? uint8_t {1} : uint8_t {0});
    // Reserved legacy sprite-size fields. Runtime dimensions are calculated from baked model bounds
    writer.Write<int32_t>(0);
    writer.Write<int32_t>(0);
    writer.Write<int32_t>(0);
    writer.Write<int32_t>(0);
    writer.WriteString(RotationBone);
    DefaultLink.Save(writer);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(Links.size()));
    for (const BakerModelDescriptionLink& link : Links) {
        link.Save(writer);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(AnimationEntries.size()));
    for (const BakerModelDescriptionAnimationEntry& anim_entry : AnimationEntries) {
        anim_entry.Save(writer);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(AnimSpeed.size()));
    for (const auto& [anim_pair, speed] : AnimSpeed) {
        writer.Write<int32_t>(anim_pair.first);
        writer.Write<int32_t>(anim_pair.second);
        writer.Write<float32_t>(speed);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(AnimLayerValues.size()));
    for (const BakerModelDescriptionAnimLayerValue& value : AnimLayerValues) {
        value.Save(writer);
    }
    writer.WriteStringVector(FastTransitionBones);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(StateAnimEquals.size()));
    for (const auto& [from, to] : StateAnimEquals) {
        writer.Write<int32_t>(from);
        writer.Write<int32_t>(to);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(ActionAnimEquals.size()));
    for (const auto& [from, to] : ActionAnimEquals) {
        writer.Write<int32_t>(from);
        writer.Write<int32_t>(to);
    }
}

void BakerModelDescriptionLink::Save(DataWriter& writer) const
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<int32_t>(Layer);
    writer.Write<int32_t>(LayerValue);
    writer.WriteString(LinkBone);
    writer.WriteString(ChildName);
    writer.Write<uint8_t>(IsParticles ? uint8_t {1} : uint8_t {0});
    writer.Write<float32_t>(RotX);
    writer.Write<float32_t>(RotY);
    writer.Write<float32_t>(RotZ);
    writer.Write<float32_t>(MoveX);
    writer.Write<float32_t>(MoveY);
    writer.Write<float32_t>(MoveZ);
    writer.Write<float32_t>(ScaleX);
    writer.Write<float32_t>(ScaleY);
    writer.Write<float32_t>(ScaleZ);
    writer.Write<float32_t>(SpeedAjust);
    writer.WriteSizedObjectVector(DisabledLayer);
    writer.WriteStringVector(DisabledMesh);
    writer.Write<uint32_t>(numeric_cast<uint32_t>(TextureInfo.size()));
    for (const auto& [texture_name, mesh_name, texture_index] : TextureInfo) {
        writer.WriteString(texture_name);
        writer.WriteString(mesh_name);
        writer.Write<int32_t>(texture_index);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(EffectInfo.size()));
    for (const auto& [effect_name, mesh_name] : EffectInfo) {
        writer.WriteString(effect_name);
        writer.WriteString(mesh_name);
    }
    writer.Write<uint32_t>(numeric_cast<uint32_t>(CutInfo.size()));
    for (const BakerModelDescriptionCut& cut : CutInfo) {
        cut.Save(writer);
    }
    bool has_geometry = !ChildName.empty() && !IsParticles;
    FO_VERIFY_AND_THROW(Bounds.has_value() == has_geometry, "Model description link geometry and bounds do not match", ChildName, IsParticles, Bounds.has_value());
    writer.Write<uint8_t>(has_geometry ? uint8_t {1} : uint8_t {0});

    if (has_geometry) {
        writer.Write<vec3>(Bounds->Min);
        writer.Write<vec3>(Bounds->Max);
    }
}

void BakerModelDescriptionCut::Save(DataWriter& writer) const
{
    FO_STACK_TRACE_ENTRY();

    writer.WriteString(FileName);
    writer.WriteSizedObjectVector(Layers);
    writer.WriteStringVector(Shapes);
    writer.WriteString(UnskinBone1);
    writer.WriteString(UnskinBone2);
    writer.WriteString(UnskinShape);
    writer.Write<uint8_t>(RevertUnskinShape ? uint8_t {1} : uint8_t {0});
}

void BakerModelDescriptionAnimationEntry::Save(DataWriter& writer) const
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<int32_t>(StateAnim);
    writer.Write<int32_t>(ActionAnim);
    writer.WriteString(FileName);
    writer.WriteString(Name);
}

void BakerModelDescriptionAnimLayerValue::Save(DataWriter& writer) const
{
    FO_STACK_TRACE_ENTRY();

    writer.Write<int32_t>(StateAnim);
    writer.Write<int32_t>(ActionAnim);
    writer.Write<int32_t>(Layer);
    writer.Write<int32_t>(LayerValue);
}

static auto TokenizeModelDescriptionLine(string_view line) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    size_t comment_pos = line.find('#');
    size_t semicolon_pos = line.find(';');

    if (semicolon_pos != string_view::npos) {
        comment_pos = comment_pos != string_view::npos ? std::min(comment_pos, semicolon_pos) : semicolon_pos;
    }

    string clean_line = string(comment_pos != string_view::npos ? line.substr(0, comment_pos) : line);
    istringstream istr = istringstream(make_stream_string(clean_line));
    vector<string> tokens;
    string token;

    while (istr >> token) {
        tokens.emplace_back(std::move(token));
    }

    return tokens;
}

static auto ApplyModelDescriptionReplacements(string content, const vector<pair<string, string>>& replacements) -> string
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& [name, value] : replacements) {
        content = strex(content).replace(strex("%{}%", name), value);
    }

    return content;
}

static auto TakeModelDescriptionToken(const vector<string>& tokens, size_t& index, string_view token, string_view fname, size_t line) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (index >= tokens.size()) {
        throw ModelInfoBakerException("Missing argument for token", token, fname, line);
    }

    return tokens[index++];
}

static auto ParseModelDescriptionFloat(string_view value, string_view token, string_view fname, size_t line) -> float32_t
{
    FO_STACK_TRACE_ENTRY();

    if (!strvex(value).is_number()) {
        throw ModelInfoBakerException("Invalid float value for token", value, token, fname, line);
    }

    float32_t parsed_value = strvex(value).to_float32();

    if (!std::isfinite(parsed_value)) {
        throw ModelInfoBakerException("Invalid non-finite float value for token", value, token, fname, line);
    }

    return parsed_value;
}

static auto ParseModelDescriptionInt(string_view value, const NameResolver& name_resolver, string_view token, string_view fname, size_t line) -> int32_t
{
    FO_STACK_TRACE_ENTRY();

    if (strvex(value).is_explicit_bool()) {
        return strvex(value).to_bool() ? 1 : 0;
    }
    if (strvex(value).is_number()) {
        return numeric_cast<int32_t>(strvex(value).to_int64());
    }

    bool failed = false;
    int32_t enum_value = name_resolver.ResolveEnumValue(value, &failed);

    if (failed) {
        throw ModelInfoBakerException("Invalid enum value for token", value, token, fname, line);
    }

    return enum_value;
}

static void ValidateModelDescriptionLayer(int32_t layer, string_view token, string_view fname, size_t line)
{
    FO_STACK_TRACE_ENTRY();

    if (layer < 0 || layer >= numeric_cast<int32_t>(MODEL_LAYERS_COUNT)) {
        throw ModelInfoBakerException("Layer value for token is out of range", layer, token, fname, line, MODEL_LAYERS_COUNT);
    }
}

static void ApplyModelDescriptionAdd(float32_t& value, float32_t operand)
{
    FO_NO_STACK_TRACE_ENTRY();

    value = value == 0.0f ? operand : value + operand;
}

static void ApplyModelDescriptionMul(float32_t& value, float32_t operand)
{
    FO_NO_STACK_TRACE_ENTRY();

    value = value == 0.0f ? operand : value * operand;
}

FO_END_NAMESPACE

#endif
