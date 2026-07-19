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
};

struct BakerModelDescriptionAnimEntry
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
    vector<BakerModelDescriptionAnimEntry> AnimEntries {};
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

static auto IsModelDescriptionTemplateFile(string_view path) -> bool;
static auto GetModelDescriptionMaxWriteTime(const FileCollection& files, const NameResolver& name_resolver, string_view fname) -> uint64_t;
static void CollectModelDescriptionLinkDependencies(const BakerModelDescriptionLink& link, unordered_set<string>& required_dependencies, unordered_set<string>& optional_dependencies);
static void UpdateModelDescriptionDependencyWriteTime(const FileCollection& files, string_view dependency, string_view owner, bool required, uint64_t& max_write_time);

static auto ValidateModelDescription(const FileCollection& source_files, const FileSystem& baked_files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> ValidatedModelDescription;
static auto ValidateModelDescriptionAnimations(const FileCollection& source_files, const NameResolver& name_resolver, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakerModelDescription& description, string_view fname) -> ValidatedModelAnimations;
static void ValidateModelDescriptionAnimationData(ModelSkeletonCompatibilityReport& compatibility_report, const vector<ModelAnimationSource>& animation_sources, string_view fname);
static void ValidateModelDescriptionAttachment(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& main_info, const BakerModelDescriptionLink& link, string_view fname);
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
static void BakeModelAnimInfo(const BakingContext& ctx, const FileCollection& files, const ModelSourceAssetCache& model_sources, string_view target_path);
static void CollectBakedModelMeshInfo(const ModelMeshBoneData& bone, BakedModelMeshInfo& info, const vector<string>& parent_hierarchy);

static auto TokenizeModelDescriptionLine(string_view line) -> vector<string>;
static auto ApplyModelDescriptionReplacements(string content, const vector<pair<string, string>>& replacements) -> string;
static auto TakeModelDescriptionToken(const vector<string>& tokens, size_t& index, string_view token, string_view fname, size_t line) -> string;
static auto ParseModelDescriptionFloat(string_view value, string_view token, string_view fname, size_t line) -> float32_t;
static auto ParseModelDescriptionInt(string_view value, const NameResolver& name_resolver, string_view token, string_view fname, size_t line) -> int32_t;
static void ApplyModelDescriptionAdd(float32_t& value, float32_t operand);
static void ApplyModelDescriptionMul(float32_t& value, float32_t operand);

ModelInfoBaker::ModelInfoBaker(shared_ptr<BakingContext> ctx, ModelSourceAssetCache::LoadCallback model_source_loader) :
    BaseBaker(std::move(ctx)),
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
    const ModelSourceAssetCache model_sources {files, _modelSourceLoader};

    BakeModelAnimInfo(*_context, files, model_sources, target_path);

    vector<File> filtered_files;

    if (target_path.empty()) {
        for (const FileHeader& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();

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
        const string ext = strex(target_path).get_file_extension();

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

    const BakerClientEngine dependency_engine(*_context->BakedFiles);
    vector<File> files_to_bake;

    for (File& file_ : filtered_files) {
        const uint64_t max_write_time = GetModelDescriptionMaxWriteTime(files, dependency_engine, file_.GetPath());

        if (_context->BakeChecker && !_context->BakeChecker(file_.GetPath(), max_write_time)) {
            continue;
        }

        files_to_bake.emplace_back(std::move(file_));
    }

    vector<std::future<pair<string, ModelSkeletonCompatibilityReport>>> file_bakings;

    for (File& file_ : files_to_bake) {
        const auto task_name = strex("BakeModelInfo-{}", file_.GetPath()).str();
        file_bakings.emplace_back(run_async(GetAsyncMode(), task_name, [this, &files, &model_sources, file = std::move(file_)]() FO_DEFERRED -> pair<string, ModelSkeletonCompatibilityReport> {
            const BakerClientEngine client_engine(*_context->BakedFiles);

            if (_context->BakeChecker) {
                const uint64_t max_write_time = GetModelDescriptionMaxWriteTime(files, client_engine, file.GetPath());

                if (!_context->BakeChecker(file.GetPath(), max_write_time)) {
                    return pair<string, ModelSkeletonCompatibilityReport> {string {}, ModelSkeletonCompatibilityReport {}};
                }
            }

            ModelDescriptionParser parser(&files, &client_engine);
            auto [description, max_write_time] = parser.Parse(file.GetPath());
            ignore_unused(max_write_time);

            ValidatedModelDescription validated = ValidateModelDescription(files, *_context->BakedFiles, client_engine, model_sources, description, file.GetPath());

            vector<uint8_t> data;
            DataWriter writer(data);
            writer.WriteBytes({MODEL_DESCRIPTION_MAGIC.data(), MODEL_DESCRIPTION_MAGIC.size()});
            writer.Write<uint16_t>(MODEL_DESCRIPTION_SCHEMA_VERSION);
            writer.Write<uint16_t>(MODEL_DESCRIPTION_SUPPORTED_FLAGS);
            description.Save(writer);
            const vector<uint8_t> animation_rig_data = WriteModelAnimationRigData(validated.AnimationRigData, file.GetPath());
            writer.Write<uint64_t>(numeric_cast<uint64_t>(animation_rig_data.size()));
            writer.WriteBytes(animation_rig_data);
            _context->WriteData(file.GetPath(), data);
            return pair<string, ModelSkeletonCompatibilityReport> {string {file.GetPath()}, std::move(validated.CompatibilityReport)};
        }));
    }

    size_t errors = 0;
    vector<pair<string, ModelSkeletonCompatibilityReport>> compatibility_reports;

    for (std::future<pair<string, ModelSkeletonCompatibilityReport>>& file_baking : file_bakings) {
        try {
            auto report = file_baking.get();

            if (!report.first.empty()) {
                compatibility_reports.emplace_back(std::move(report));
            }
        }
        catch (const std::exception& ex) {
            WriteLog("Model description baking error: {}", ex.what());
            errors++;
        }
    }

    std::sort(compatibility_reports.begin(), compatibility_reports.end(), [](const auto& first, const auto& second) { return first.first < second.first; });

    for (const auto& [file_name, report] : compatibility_reports) {
        WriteLog("Model skeleton compatibility report for '{}': {}", file_name, FormatModelSkeletonCompatibilityReport(report));
    }

    if (errors != 0) {
        throw ModelInfoBakerException("Errors during model description baking");
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

    for (const BakerModelDescriptionAnimEntry& animation : description.AnimEntries) {
        if (!selected_animation_pairs.emplace(animation.StateAnim, animation.ActionAnim).second || animation.FileName == "ModelFile") {
            continue;
        }

        required_dependencies.emplace(strex(fname).extract_dir().combine_path(animation.FileName).str());
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

    const auto dependency_file = std::ranges::find_if(files, [&](const FileHeader& file) { return file.GetPath() == dependency; });

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

    const string content = ApplyModelDescriptionReplacements(file.GetStr(), replacements);
    ParseContent(file.GetPath(), content, description, state);

    _includeStack.pop_back();
}

void ModelDescriptionParser::ParseContent(string_view fname, const string& content, BakerModelDescription& description, ModelDescriptionParseState& state)
{
    FO_STACK_TRACE_ENTRY();

    auto istr = istringstream(content);
    string line_buf;
    size_t line = 0;

    while (std::getline(istr, line_buf)) {
        line++;

        const vector<string> tokens = TokenizeModelDescriptionLine(line_buf);
        size_t index = 0;

        while (index < tokens.size()) {
            const string token = tokens[index++];

            ParseToken(fname, line, token, tokens, index, description, state);
        }
    }
}

void ModelDescriptionParser::ParseToken(string_view fname, size_t line, string_view token, const vector<string>& tokens, size_t& index, BakerModelDescription& description, ModelDescriptionParseState& state)
{
    FO_STACK_TRACE_ENTRY();

    if (token == "Model") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.Model = strex(fname).extract_dir().combine_path(value);
    }
    else if (token == "Include") {
        if (index >= tokens.size()) {
            throw ModelInfoBakerException("Missing include path", fname, line);
        }

        const string include_name = tokens[index++];

        if ((tokens.size() - index) % 2 != 0) {
            throw ModelInfoBakerException("Include has unpaired template argument", include_name, fname, line);
        }

        vector<pair<string, string>> replacements;

        while (index < tokens.size()) {
            string name = tokens[index++];
            string value = tokens[index++];
            replacements.emplace_back(std::move(name), std::move(value));
        }

        const string include_path = strex(fname).extract_dir().combine_path(include_name);
        ParseFile(include_path, replacements, description, state);
        index = tokens.size();
    }
    else if (token == "Mesh") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        state.Mesh = value != "All" ? value : string {};
    }
    else if (token == "Subset") {
        (void)TakeModelDescriptionToken(tokens, index, token, fname, line);
        WriteLog("Tag 'Subset' obsolete, use 'Mesh' instead");
    }
    else if (token == "Layer" || token == "Value") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t parsed_value = ParseModelDescriptionInt(value, *_nameResolver, token, fname, line);

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
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

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
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        auto default_link = ModelDescriptionLinkPtr(description.DefaultLink);
        auto dummy_link = ModelDescriptionLinkPtr(state.DummyLink);

        if (state.Link != default_link && state.Link != dummy_link) {
            state.Link->LinkBone = value;
        }
    }
    else if (token == "Cut") {
        const string file_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string layers = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string shapes = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string unskin_bone1 = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string unskin_bone2 = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string unskin_shape = TakeModelDescriptionToken(tokens, index, token, fname, line);

        BakerModelDescriptionCut& cut = state.Link->CutInfo.emplace_back();
        cut.FileName = strex(fname).extract_dir().combine_path(file_name);

        for (const string& cut_layer_name : strex(layers).split('-')) {
            if (cut_layer_name != "All") {
                const int32_t cut_layer = ParseModelDescriptionInt(cut_layer_name, *_nameResolver, token, fname, line);
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
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token, ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Set);
    }
    else if (token == "Scale") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        state.Link->ScaleX = parsed_value;
        state.Link->ScaleY = parsed_value;
        state.Link->ScaleZ = parsed_value;
    }
    else if (token == "RotX+" || token == "RotY+" || token == "RotZ+" || token == "MoveX+" || token == "MoveY+" || token == "MoveZ+" || token == "ScaleX+" || token == "ScaleY+" || token == "ScaleZ+" || token == "Speed+") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token.substr(0, token.length() - 1), ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Add);
    }
    else if (token == "Scale+") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        ApplyModelDescriptionAdd(state.Link->ScaleX, parsed_value);
        ApplyModelDescriptionAdd(state.Link->ScaleY, parsed_value);
        ApplyModelDescriptionAdd(state.Link->ScaleZ, parsed_value);
    }
    else if (token == "RotX*" || token == "RotY*" || token == "RotZ*" || token == "MoveX*" || token == "MoveY*" || token == "MoveZ*" || token == "ScaleX*" || token == "ScaleY*" || token == "ScaleZ*" || token == "Speed*") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        ApplyFloatValue(*state.Link, token.substr(0, token.length() - 1), ParseModelDescriptionFloat(value, token, fname, line), AssignMode::Mul);
    }
    else if (token == "Scale*") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const float32_t parsed_value = ParseModelDescriptionFloat(value, token, fname, line);
        ApplyModelDescriptionMul(state.Link->ScaleX, parsed_value);
        ApplyModelDescriptionMul(state.Link->ScaleY, parsed_value);
        ApplyModelDescriptionMul(state.Link->ScaleZ, parsed_value);
    }
    else if (token == "DisableLayer") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        for (const string& disabled_layer_name : strex(value).split('-')) {
            const int32_t disabled_layer = ParseModelDescriptionInt(disabled_layer_name, *_nameResolver, token, fname, line);
            ValidateModelDescriptionLayer(disabled_layer, token, fname, line);
            state.Link->DisabledLayer.emplace_back(disabled_layer);
        }
    }
    else if (token == "DisableMesh") {
        const string value = TakeModelDescriptionToken(tokens, index, token, fname, line);

        for (const string& disabled_mesh_name : strex(value).split('-')) {
            state.Link->DisabledMesh.emplace_back(disabled_mesh_name != "All" ? disabled_mesh_name : string {});
        }
    }
    else if (token == "Texture") {
        const string index_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t texture_index = ParseModelDescriptionInt(index_value, *_nameResolver, token, fname, line);
        const string texture_name = TakeModelDescriptionToken(tokens, index, token, fname, line);

        if (texture_index < 0 || texture_index >= numeric_cast<int32_t>(MODEL_MAX_TEXTURES)) {
            throw ModelInfoBakerException("Texture index is out of range", texture_index, fname, line, MODEL_MAX_TEXTURES);
        }

        state.Link->TextureInfo.emplace_back(texture_name, state.Mesh, texture_index);
    }
    else if (token == "Effect") {
        const string effect_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        state.Link->EffectInfo.emplace_back(effect_name, state.Mesh);
    }
    else if (token == "Anim") {
        const string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        const string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        const string anim_file = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const string anim_name = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.AnimEntries.emplace_back(BakerModelDescriptionAnimEntry {.StateAnim = state_anim, .ActionAnim = action_anim, .FileName = anim_file, .Name = anim_name});
    }
    else if (token == "AllowAnimationGeometry") {
        const string anim_file = TakeModelDescriptionToken(tokens, index, token, fname, line);

        if (!description.AnimationGeometryExceptions.emplace(anim_file).second) {
            throw ModelInfoBakerException("Duplicate animation-geometry exception", anim_file, fname, line);
        }
    }
    else if (token == "AnimSpeed") {
        const string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        const string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        const string speed_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        description.AnimSpeed.emplace_back(std::make_pair(state_anim, action_anim), ParseModelDescriptionFloat(speed_value, token, fname, line));
    }
    else if (token == "AnimLayerValue") {
        const string state_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t state_anim = ParseModelDescriptionInt(state_anim_value, *_nameResolver, token, fname, line);
        const string action_anim_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t action_anim = ParseModelDescriptionInt(action_anim_value, *_nameResolver, token, fname, line);
        const string layer_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t layer = ParseModelDescriptionInt(layer_value, *_nameResolver, token, fname, line);
        const string anim_layer_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t value = ParseModelDescriptionInt(anim_layer_value, *_nameResolver, token, fname, line);
        ValidateModelDescriptionLayer(layer, token, fname, line);
        description.AnimLayerValues.emplace_back(BakerModelDescriptionAnimLayerValue {.StateAnim = state_anim, .ActionAnim = action_anim, .Layer = layer, .LayerValue = value});
    }
    else if (token == "FastTransitionBone") {
        description.FastTransitionBones.emplace_back(TakeModelDescriptionToken(tokens, index, token, fname, line));
    }
    else if (token == "StateAnimEqual") {
        const string from_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t from = ParseModelDescriptionInt(from_value, *_nameResolver, token, fname, line);
        const string to_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t to = ParseModelDescriptionInt(to_value, *_nameResolver, token, fname, line);
        description.StateAnimEquals.emplace_back(from, to);
    }
    else if (token == "ActionAnimEqual") {
        const string from_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t from = ParseModelDescriptionInt(from_value, *_nameResolver, token, fname, line);
        const string to_value = TakeModelDescriptionToken(tokens, index, token, fname, line);
        const int32_t to = ParseModelDescriptionInt(to_value, *_nameResolver, token, fname, line);
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

static auto ValidateModelDescription(const FileCollection& source_files, const FileSystem& baked_files, const NameResolver& name_resolver, const ModelSourceAssetCache& model_sources, const BakerModelDescription& description, string_view fname) -> ValidatedModelDescription
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
        ValidateModelDescriptionAttachment(source_files, baked_files, model_sources, mesh_cache, main_info, link, fname);
    }

    ValidatedModelAnimations animations = ValidateModelDescriptionAnimations(source_files, name_resolver, baked_files, model_sources, mesh_cache, description, fname);
    vector<ModelSkeletonClipSource> clip_sources;
    clip_sources.reserve(animations.Sources.size());

    for (const ModelAnimationSource& animation : animations.Sources) {
        const shared_ptr<const ModelSourceAsset> animation_model = GetModelSourceAsset(model_sources, animation.FileName, fname);
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
        const string resolved_exception = strex(fname).extract_dir().combine_path(exception).str();

        if (!geometry_exceptions.emplace(resolved_exception).second) {
            throw ModelInfoBakerException("Animation-geometry exceptions contain duplicate resolved target; keep exactly one AllowAnimationGeometry line", fname, resolved_exception);
        }
    }

    for (const BakerModelDescriptionAnimEntry& anim_entry : description.AnimEntries) {
        ValidateModelDescriptionAnimPair(name_resolver, anim_entry.StateAnim, anim_entry.ActionAnim, "Anim", fname);

        if (!anim_pairs.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
            continue;
        }

        const string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(fname).extract_dir().combine_path(anim_entry.FileName).str();
        const BakedModelMeshInfo& anim_info = GetBakedModelMeshInfo(baked_files, mesh_cache, anim_file);
        ValidateBakedModelMeshFreshness(source_files, anim_info, fname);
        const shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, fname);
        string anim_name = anim_entry.Name;

        if (anim_file != description.Model) {
            selected_external_animation_files.emplace(anim_file);

            if (anim_info.DrawBonesCount != 0) {
                if (geometry_exceptions.count(anim_file) == 0) {
                    const string first_draw_bone = anim_info.DrawBones.empty() ? string {"<unnamed>"} : *std::ranges::min_element(anim_info.DrawBones);
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

            const auto is_vec3_finite = [](const vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); };

            for (const vec3& value : joint.Scale.Values) {
                issue.NonFiniteScaleKeys += !is_vec3_finite(value) ? 1 : 0;
            }
            for (const quaternion& value : joint.Rotation.Values) {
                const bool finite = std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
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

static void ValidateModelDescriptionAttachment(const FileCollection& source_files, const FileSystem& baked_files, const ModelSourceAssetCache& model_sources, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& main_info, const BakerModelDescriptionLink& link, string_view fname)
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

    const string child_ext = strex(link.ChildName).get_file_extension();

    if (child_ext == "fo3d") {
        if (!baked_files.IsFileExists(link.ChildName) && !source_files.FindFileByPath(link.ChildName)) {
            throw ModelInfoBakerException("Attached model description referenced by file not found", link.ChildName, fname);
        }

        return;
    }

    const BakedModelMeshInfo& child_info = GetBakedModelMeshInfo(baked_files, mesh_cache, link.ChildName);
    ValidateBakedModelMeshFreshness(source_files, child_info, fname);
    const shared_ptr<const ModelSourceAsset> child_source = GetModelSourceAsset(model_sources, link.ChildName, fname);

    if (!child_source->Animations.empty()) {
        throw ModelInfoBakerException("Direct attached model contains embedded animation clips; animated child models require a .fo3d description", link.ChildName, fname, child_source->Animations.size());
    }

    ValidateModelDescriptionLinkData(source_files, baked_files, mesh_cache, child_info, &main_info, link, fname);
}

static void ValidateModelDescriptionLinkData(const FileCollection& source_files, const FileSystem& baked_files, unordered_map<string, BakedModelMeshInfo>& mesh_cache, const BakedModelMeshInfo& target_info, nptr<const BakedModelMeshInfo> parent_info, const BakerModelDescriptionLink& link, string_view fname)
{
    FO_STACK_TRACE_ENTRY();

    if (link.SpeedAjust < 0.0f) {
        throw ModelInfoBakerException("Negative Speed value for model", fname, target_info.FileName);
    }

    for (const int32_t disabled_layer : link.DisabledLayer) {
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

    for (const int32_t layer : cut.Layers) {
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

    const string texture_path = strex(model_info.FileName).extract_dir().combine_path(texture_name);
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

    const string key {path};

    if (const auto it = cache.find(key); it != cache.end()) {
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

    const File file = baked_files.ReadFile(path);

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
    }
    catch (const std::exception& ex) {
        throw ModelInfoBakerException("Invalid baked model mesh", path, ex.what());
    }

    return info;
}

static void ValidateBakedModelMeshFreshness(const FileCollection& source_files, const BakedModelMeshInfo& info, string_view owner)
{
    FO_STACK_TRACE_ENTRY();

    const File source_file = source_files.FindFileByPath(info.FileName);

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

struct ModelAnimInfoBakingStats
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

static void BakeModelAnimInfo(const BakingContext& ctx, const FileCollection& files, const ModelSourceAssetCache& model_sources, string_view target_path)
{
    FO_STACK_TRACE_ENTRY();

    constexpr string_view output_path = "ModelAnimInfo.foinfo";

    if (!target_path.empty() && target_path != output_path) {
        return;
    }

    // Collect all (non-template) model descriptions and the newest write time across them and their includes.
    vector<File> fo3d_files;
    uint64_t max_write_time = 0;
    const BakerClientEngine client_engine(*ctx.BakedFiles);

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

    // Deterministic section order.
    std::sort(fo3d_files.begin(), fo3d_files.end(), [](const File& a, const File& b) { return a.GetPath() < b.GetPath(); });

    unordered_map<string, optional<ModelBounds3D>> animation_bounds_cache;
    ModelAnimInfoBakingStats stats;
    string config_text;

    for (const File& file : fo3d_files) {
        ModelDescriptionParser parser(&files, &client_engine);
        auto [description, parsed_write_time] = parser.Parse(file.GetPath());
        ignore_unused(parsed_write_time);

        set<pair<int32_t, int32_t>> seen;
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

        if (description.Model.empty()) {
            throw ModelInfoBakerException("Model description has no Model section", file.GetPath());
        }

        const File model_file = ctx.BakedFiles->ReadFile(description.Model);
        FO_VERIFY_AND_THROW(model_file, "Baked model data for bounds is not readable", description.Model, file.GetPath());
        auto model_reader = DataReader(model_file.GetDataSpan());
        const ModelMeshData model_mesh = ReadModelMeshData(model_reader, description.Model);
        optional<ModelBounds3D> model_bounds;
        optional<ModelBounds3D> view_bounds;
        int32_t view_bounds_priority = -1;

        for (const auto& [from, to] : description.StateAnimEquals) {
            state_anim_equals.try_emplace(from, to);
        }
        for (const auto& [from, to] : description.ActionAnimEquals) {
            action_anim_equals.try_emplace(from, to);
        }

        for (const BakerModelDescriptionAnimEntry& anim_entry : description.AnimEntries) {
            if (!seen.emplace(anim_entry.StateAnim, anim_entry.ActionAnim).second) {
                continue;
            }

            const string anim_file = anim_entry.FileName == "ModelFile" ? description.Model : strex(file.GetPath()).extract_dir().combine_path(anim_entry.FileName).str();
            const shared_ptr<const ModelSourceAsset> anim_source = GetModelSourceAsset(model_sources, anim_file, file.GetPath());

            string anim_name = anim_entry.Name;
            const bool reversed = !anim_name.empty() && anim_name.front() == '~';

            if (reversed) {
                anim_name.erase(anim_name.begin());
            }

            const float32_t clip_duration = GetModelSourceAnimationDuration(*anim_source, anim_name);

            if (clip_duration <= 0.0f) {
                continue;
            }

            // Authored playback speed scales the cycle (faster speed -> shorter real cycle). The runtime
            // moving-speed factor is applied separately at play time and must not be baked in here.
            float32_t speed = 1.0f;

            for (const auto& [anim_pair, anim_speed] : description.AnimSpeed) {
                if (anim_pair.first == anim_entry.StateAnim && anim_pair.second == anim_entry.ActionAnim) {
                    speed = anim_speed;
                    break;
                }
            }

            if (speed <= 0.0f || !std::isfinite(1.0f / speed)) {
                throw ModelInfoBakerException("Animation speed must be positive with a finite reciprocal", anim_entry.StateAnim, anim_entry.ActionAnim, file.GetPath());
            }

            const double duration_milliseconds = static_cast<double>(clip_duration) / static_cast<double>(speed) * 1000.0;

            if (!std::isfinite(duration_milliseconds) || duration_milliseconds <= 0.0 || duration_milliseconds > static_cast<double>(std::numeric_limits<int32_t>::max())) {
                throw ModelInfoBakerException("Animation duration is outside the millisecond output range", anim_entry.StateAnim, anim_entry.ActionAnim, file.GetPath(), clip_duration, speed);
            }

            const int32_t duration_ms = iround<int32_t>(duration_milliseconds);

            // The runtime model-anim-info load rejects a non-positive duration, so a sub-millisecond effective
            // cycle that rounds down to zero must fail here rather than bake a manifest the client cannot load.
            if (duration_ms <= 0) {
                throw ModelInfoBakerException("Animation duration rounds to a non-positive millisecond value", anim_entry.StateAnim, anim_entry.ActionAnim, file.GetPath(), clip_duration, speed);
            }

            raw_durations.emplace_back(anim_entry.StateAnim, anim_entry.ActionAnim, duration_ms);
            stats.AnimationEntries++;

            const string cache_key = strex("{}\n{}\n{}\n{}", file.GetPath(), anim_file, anim_name, reversed ? 1 : 0);
            auto bounds_it = animation_bounds_cache.find(cache_key);

            if (bounds_it == animation_bounds_cache.end()) {
                stats.BoundsCalculations++;
                optional<ModelBounds3D> calculated_bounds;

                try {
                    const ModelAnimationSource& animation = GetModelSourceAnimation(*anim_source, anim_name);
                    calculated_bounds = CalculateModelAnimationBounds(model_mesh, animation, reversed, description.DefaultLink.DisabledMesh);

                    if (!calculated_bounds && !description.DefaultLink.DisabledMesh.empty()) {
                        stats.BoundsCalculations++;
                        calculated_bounds = CalculateModelAnimationBounds(model_mesh, animation, reversed);
                    }
                }
                catch (const ModelBoundsException& ex) {
                    throw ModelInfoBakerException("Failed to calculate animation bounds", anim_entry.StateAnim, anim_entry.ActionAnim, file.GetPath(), ex.what());
                }

                bounds_it = animation_bounds_cache.emplace(cache_key, calculated_bounds).first;
            }
            else {
                stats.BoundsCacheHits++;
            }

            const optional<ModelBounds3D>& bounds = bounds_it->second;
            FO_VERIFY_AND_THROW(bounds, "Animation bounds could not be calculated", anim_entry.StateAnim, anim_entry.ActionAnim, file.GetPath());
            FO_VERIFY_AND_THROW(IncludeModelBounds(model_bounds, *bounds), "Calculated model animation bounds are invalid", file.GetPath(), anim_entry.StateAnim, anim_entry.ActionAnim);

            const bool idle = anim_entry.ActionAnim == static_cast<int32_t>(CritterActionAnim::Idle);
            const bool unarmed_idle = idle && anim_entry.StateAnim == static_cast<int32_t>(CritterStateAnim::Unarmed);
            const int32_t view_priority = unarmed_idle ? 2 : idle ? 1 : 0;

            if (view_priority > view_bounds_priority) {
                view_bounds = *bounds;
                view_bounds_priority = view_priority;
            }

            stats.AnimationBounds++;
            bounds_states += strex(" {}", anim_entry.StateAnim);
            bounds_actions += strex(" {}", anim_entry.ActionAnim);
            bounds_min_x += strex(" {}", bounds->Min.x);
            bounds_min_y += strex(" {}", bounds->Min.y);
            bounds_min_z += strex(" {}", bounds->Min.z);
            bounds_max_x += strex(" {}", bounds->Max.x);
            bounds_max_y += strex(" {}", bounds->Max.y);
            bounds_max_z += strex(" {}", bounds->Max.z);

            const float32_t max_extent = std::max({bounds->Max.x - bounds->Min.x, bounds->Max.y - bounds->Min.y, bounds->Max.z - bounds->Min.z});
            const size_t bucket = max_extent < 1.0f ? 0 : max_extent < 2.0f ? 1 : max_extent < 3.0f ? 2 : max_extent < 5.0f ? 3 : max_extent < 10.0f ? 4 : 5;
            stats.AnimationBoundsMaxExtent[bucket]++;
        }

        // Match ModelInformation::GetAnimationIndexEx: both alias maps are applied once before the
        // animation lookup, and an alias has priority over an exact entry with the same source key.
        // Materialize every input pair that resolves to a baked entry so common runtimes do not need
        // the client-only model description to answer the typed duration query.
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
                const auto it = state_anim_equals.find(from);

                if (it != state_anim_equals.end() && it->second == to && to == state_anim && seen_state_inputs.emplace(from).second) {
                    resolved_state_inputs.emplace_back(from);
                }
            }

            if (action_anim_equals.count(action_anim) == 0) {
                resolved_action_inputs.emplace_back(action_anim);
                seen_action_inputs.emplace(action_anim);
            }
            for (const auto& [from, to] : description.ActionAnimEquals) {
                const auto it = action_anim_equals.find(from);

                if (it != action_anim_equals.end() && it->second == to && to == action_anim && seen_action_inputs.emplace(from).second) {
                    resolved_action_inputs.emplace_back(from);
                }
            }

            for (const int32_t resolved_state : resolved_state_inputs) {
                for (const int32_t resolved_action : resolved_action_inputs) {
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
            try {
                model_bounds = CalculateModelStaticBounds(model_mesh, description.DefaultLink.DisabledMesh);

                if (!model_bounds && !description.DefaultLink.DisabledMesh.empty()) {
                    model_bounds = CalculateModelStaticBounds(model_mesh);
                }
            }
            catch (const ModelBoundsException& ex) {
                throw ModelInfoBakerException("Failed to calculate static model bounds", file.GetPath(), ex.what());
            }

            FO_VERIFY_AND_THROW(model_bounds, "Static model bounds could not be calculated", file.GetPath());
            view_bounds = model_bounds;
        }

        FO_VERIFY_AND_THROW(view_bounds, "Model view bounds were not selected", file.GetPath());
        stats.ModelSections++;
        stats.ModelBounds++;

        if (view_bounds_priority >= 1) {
            stats.ViewBoundsIdle++;
        }
        else {
            stats.ViewBoundsFallback++;
        }

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
            config_text += strex("StateAnims ={}\n", states);
            config_text += strex("ActionAnims ={}\n", actions);
            config_text += strex("DurationsMs ={}\n", durations);
        }
        if (!bounds_states.empty()) {
            config_text += strex("BoundsStateAnims ={}\n", bounds_states);
            config_text += strex("BoundsActionAnims ={}\n", bounds_actions);
            config_text += strex("BoundsMinX ={}\n", bounds_min_x);
            config_text += strex("BoundsMinY ={}\n", bounds_min_y);
            config_text += strex("BoundsMinZ ={}\n", bounds_min_z);
            config_text += strex("BoundsMaxX ={}\n", bounds_max_x);
            config_text += strex("BoundsMaxY ={}\n", bounds_max_y);
            config_text += strex("BoundsMaxZ ={}\n", bounds_max_z);
        }

        config_text += "\n";
    }

    const auto data = vector<uint8_t>(config_text.begin(), config_text.end());
    ctx.WriteData(output_path, data);

    if (ctx.Report) {
        const shared_ptr<BakingReport> report = ctx.Report;
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
    // Reserved legacy sprite-size fields. Runtime dimensions are calculated from baked model bounds.
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
    writer.Write<uint32_t>(numeric_cast<uint32_t>(AnimEntries.size()));
    for (const BakerModelDescriptionAnimEntry& anim_entry : AnimEntries) {
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

void BakerModelDescriptionAnimEntry::Save(DataWriter& writer) const
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
    const size_t semicolon_pos = line.find(';');

    if (semicolon_pos != string_view::npos) {
        comment_pos = comment_pos != string_view::npos ? std::min(comment_pos, semicolon_pos) : semicolon_pos;
    }

    const string clean_line = string(comment_pos != string_view::npos ? line.substr(0, comment_pos) : line);
    auto istr = istringstream(clean_line);
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

    const float32_t parsed_value = strvex(value).to_float32();

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
    const int32_t enum_value = name_resolver.ResolveEnumValue(value, &failed);

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
