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

#include "ParticleBaker.h"
#include "EffekseerCompiler.h"
#include "SparkExtension.h"

#if FO_SPARK_PARTICLES || FO_EFFEKSEER_PARTICLES
FO_DISABLE_WARNINGS_PUSH()
#if FO_EFFEKSEER_PARTICLES
#include "Effekseer.h"
#endif
#if FO_SPARK_PARTICLES
#include "SPARK.h"
#endif
FO_DISABLE_WARNINGS_POP()
#endif

FO_BEGIN_NAMESPACE

#if FO_SPARK_PARTICLES
static void ValidateSparkTexturePaths(const File& particle_file, const SPK::Ref<SPK::System>& system)
{
    FO_STACK_TRACE_ENTRY();

    const std::filesystem::path particle_dir {fs_make_path(strex(particle_file.GetPath()).extract_dir().normalize_path_slashes())};

    for (size_t group_index = 0; group_index < system->getNbGroups(); group_index++) {
        const SPK::Ref<SPK::Group>& group = system->getGroup(group_index);
        const SPK::Ref<SPK::Renderer>& renderer = group->getRenderer();

        if (!renderer || !SPK::FO::IsSparkQuadRenderer(*renderer)) {
            continue;
        }

        const SPK::FO::SparkQuadRendererData renderer_data = SPK::FO::GetSparkQuadRendererData(*renderer);
        const string texture_path {renderer_data.TextureName};

        if (texture_path.empty()) {
            continue;
        }
        if (texture_path.find_first_of("\t\r\n") != string::npos) {
            throw ParticleBakerException("SPARK particle has an invalid texture path", particle_file.GetPath(), texture_path);
        }

        const string normalized_path = strex(texture_path).normalize_path_slashes();
        const bool has_drive_prefix = normalized_path.size() >= 2 && normalized_path[1] == ':' && ((normalized_path[0] >= 'A' && normalized_path[0] <= 'Z') || (normalized_path[0] >= 'a' && normalized_path[0] <= 'z'));
        const std::filesystem::path relative_path {fs_make_path(normalized_path)};

        if (normalized_path.starts_with('/') || has_drive_prefix || relative_path.is_absolute()) {
            throw ParticleBakerException("SPARK particle texture path must be relative", particle_file.GetPath(), texture_path);
        }

        const std::filesystem::path resolved_path = (particle_dir / relative_path).lexically_normal();
        const auto first_component = resolved_path.begin();

        if (resolved_path.empty() || resolved_path.is_absolute() || (first_component != resolved_path.end() && *first_component == "..")) {
            throw ParticleBakerException("SPARK particle texture path escapes its resource source", particle_file.GetPath(), texture_path, particle_file.GetDataSource()->GetPackName());
        }
    }
}
#endif

#if FO_EFFEKSEER_PARTICLES
static constexpr string_view EffekseerDependencyCacheHeader = "FONLINE_EFFEKSEER_DEPENDENCIES_V5\n";

static auto GetEffekseerDependencyCachePath(const BakingContext& context, string_view output_path) -> string
{
    FO_STACK_TRACE_ENTRY();

    if (context.Settings->BakeOutput.empty()) {
        return {};
    }

    return strex(context.Settings->BakeOutput).combine_path(BAKER_CACHE_DIR).combine_path("Effekseer").combine_path(context.PackName).combine_path(strex("{}.deps", output_path));
}

static auto ParseEffekseerDependencySnapshot(string_view snapshot) -> optional<vector<string>>
{
    FO_STACK_TRACE_ENTRY();

    if (!snapshot.starts_with(EffekseerDependencyCacheHeader)) {
        return std::nullopt;
    }

    vector<string> paths;
    const size_t project_line_end = snapshot.find('\n', EffekseerDependencyCacheHeader.size());

    if (project_line_end == string::npos) {
        return std::nullopt;
    }

    const string_view project_line = snapshot.substr(EffekseerDependencyCacheHeader.size(), project_line_end - EffekseerDependencyCacheHeader.size());
    const size_t project_first_tab = project_line.find('\t');
    const size_t project_second_tab = project_first_tab != string::npos ? project_line.find('\t', project_first_tab + 1) : string::npos;

    if (project_first_tab == 0 || project_second_tab == string::npos || project_line.find('\t', project_second_tab + 1) != string::npos) {
        return std::nullopt;
    }

    const string_view project_path = project_line.substr(0, project_first_tab);

    if (!std::filesystem::path {fs_make_path(project_path)}.is_absolute()) {
        return std::nullopt;
    }

    size_t line_start = project_line_end + 1;

    while (line_start < snapshot.size()) {
        const size_t line_end = snapshot.find('\n', line_start);
        const string_view line = snapshot.substr(line_start, line_end != string::npos ? line_end - line_start : snapshot.size() - line_start);

        if (!line.empty()) {
            const size_t first_tab = line.find('\t');
            const size_t second_tab = first_tab != string::npos ? line.find('\t', first_tab + 1) : string::npos;

            if (first_tab == 0 || second_tab == string::npos || line.find('\t', second_tab + 1) != string::npos) {
                return std::nullopt;
            }

            string path {line.substr(0, first_tab)};

            if (!std::filesystem::path {fs_make_path(path)}.is_absolute()) {
                return std::nullopt;
            }

            paths.emplace_back(std::move(path));
        }

        if (line_end == string::npos) {
            break;
        }

        line_start = line_end + 1;
    }

    return paths;
}

static auto BuildEffekseerDependencySnapshot(string_view project_path, size_t project_size, uint64_t project_write_time, const vector<string>& dependency_paths, uint64_t& max_write_time) -> string
{
    FO_STACK_TRACE_ENTRY();

    string snapshot {EffekseerDependencyCacheHeader};
    snapshot += strex("{}\t{}\t{}\n", project_path, project_size, project_write_time);
    max_write_time = project_write_time;

    for (const string& dependency_path : dependency_paths) {
        const optional<size_t> dependency_size = fs_file_size(dependency_path);
        const uint64_t dependency_write_time = fs_last_write_time(dependency_path);

        if (dependency_size && dependency_write_time != 0) {
            snapshot += strex("{}\t{}\t{}\n", dependency_path, *dependency_size, dependency_write_time);
            max_write_time = std::max(max_write_time, dependency_write_time);
        }
        else {
            snapshot += strex("{}\t-\t-\n", dependency_path);
        }
    }

    return snapshot;
}

static auto TryGetCachedEffekseerDependencyWriteTime(const BakingContext& context, const FileHeader& project_file, string_view output_path) -> optional<uint64_t>
{
    FO_STACK_TRACE_ENTRY();

    const uint64_t source_write_time = project_file.GetWriteTime();
    const string cache_path = GetEffekseerDependencyCachePath(context, output_path);

    if (cache_path.empty()) {
        return source_write_time;
    }
    if (!project_file.GetDataSource()->IsDiskDir()) {
        return source_write_time;
    }

    const string project_path = fs_resolve_path(project_file.GetDiskPath());
    const optional<string> cached_snapshot = fs_read_file(cache_path);
    const optional<vector<string>> dependency_paths = cached_snapshot ? ParseEffekseerDependencySnapshot(*cached_snapshot) : std::nullopt;
    uint64_t dependency_write_time = 0;
    const optional<string> current_snapshot = dependency_paths ? optional<string> {BuildEffekseerDependencySnapshot(project_path, project_file.GetSize(), source_write_time, *dependency_paths, dependency_write_time)} : std::nullopt;

    if (cached_snapshot && current_snapshot && *cached_snapshot == *current_snapshot) {
        return dependency_write_time;
    }

    return std::nullopt;
}

static auto ResolveEffekseerDependencyPaths(const File& project_file, const vector<string>& compiler_dependencies) -> vector<string>
{
    FO_STACK_TRACE_ENTRY();

    vector<string> resolved_paths;
    const string project_path = fs_resolve_path(project_file.GetDiskPath());
    const string source_root_path = fs_resolve_path(project_file.GetDataSource()->GetPackName());
    const std::filesystem::path project_dir = std::filesystem::path {fs_make_path(project_path)}.parent_path();
    const std::filesystem::path source_root = std::filesystem::path {fs_make_path(source_root_path)}.lexically_normal();

    for (string dependency_path : compiler_dependencies) {
        if (dependency_path.empty()) {
            continue;
        }
        if (dependency_path.find_first_of("\t\r\n") != string::npos) {
            throw ParticleBakerException("Effekseer compiler produced an invalid dependency path", project_path, dependency_path);
        }

        const std::filesystem::path relative_path {fs_make_path(strex(dependency_path).normalize_path_slashes())};

        if (relative_path.is_absolute()) {
            throw ParticleBakerException("Effekseer project dependency path must be relative", project_path, dependency_path);
        }

        const std::filesystem::path resolved_path = (project_dir / relative_path).lexically_normal();
        const std::filesystem::path source_relative_path = resolved_path.lexically_relative(source_root);
        const auto first_component = source_relative_path.begin();

        if (source_relative_path.empty() || source_relative_path.is_absolute() || (first_component != source_relative_path.end() && *first_component == "..")) {
            throw ParticleBakerException("Effekseer project dependency escapes its directory resource source", project_file.GetPath(), dependency_path, source_root_path);
        }

        resolved_paths.emplace_back(fs_path_to_string(resolved_path));
    }

    std::ranges::sort(resolved_paths);
    const auto unique_end = std::ranges::unique(resolved_paths).begin();
    resolved_paths.erase(unique_end, resolved_paths.end());
    return resolved_paths;
}

static auto RefreshEffekseerDependencySnapshot(const BakingContext& context, const File& project_file, string_view output_path) -> uint64_t
{
    FO_STACK_TRACE_ENTRY();

    const string project_path = fs_resolve_path(project_file.GetDiskPath());
    vector<string> compiler_dependencies;

    try {
        compiler_dependencies = GetEffekseerProjectDependencies(project_path, project_file.GetDataSpan());
    }
    catch (const EffekseerCompilerException& ex) {
        throw ParticleBakerException("Effekseer project dependency scan failed", project_file.GetPath(), ex.what());
    }

    const vector<string> dependency_paths = ResolveEffekseerDependencyPaths(project_file, compiler_dependencies);
    uint64_t dependency_write_time = 0;
    const string dependency_snapshot = BuildEffekseerDependencySnapshot(project_path, project_file.GetSize(), project_file.GetWriteTime(), dependency_paths, dependency_write_time);
    const string cache_path = GetEffekseerDependencyCachePath(context, output_path);

    if (!cache_path.empty() && !fs_write_file(cache_path, dependency_snapshot)) {
        throw ParticleBakerException("Failed to refresh Effekseer dependency cache", output_path, cache_path);
    }

    if (!context.Settings->BakeOutput.empty()) {
        const string baked_output_path = strex(context.Settings->BakeOutput).combine_path(context.PackName).combine_path(output_path).str();

        if (fs_exists(baked_output_path) && !fs_remove_file(baked_output_path)) {
            throw ParticleBakerException("Failed to invalidate stale Effekseer particle", output_path, baked_output_path);
        }
    }

    return dependency_write_time;
}

static void ValidateEffekseerRuntimeBinary(string_view path, const_span<uint8_t> file_data)
{
    FO_STACK_TRACE_ENTRY();

    constexpr size_t magic_size = 4;

    if (file_data.size() < magic_size) {
        throw ParticleBakerException("Effekseer compiler produced a truncated particle", path);
    }

    const string_view actual_magic {ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), magic_size};

    if (actual_magic != "SKFE") {
        throw ParticleBakerException("Effekseer compiler produced invalid particle magic", path, actual_magic);
    }
    if (file_data.size() > numeric_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        throw ParticleBakerException("Effekseer compiler produced an oversized particle", path, file_data.size());
    }

    const Effekseer::SettingRef setting = Effekseer::Setting::Create();
    const Effekseer::EffectRef effect = Effekseer::Effect::Create(setting, file_data.data(), numeric_cast<int32_t>(file_data.size()), 1.0f, u"");

    if (!effect) {
        throw ParticleBakerException("Effekseer core rejected compiled particle", path);
    }
}
#endif

ParticleBaker::ParticleBaker(shared_ptr<BakingContext> ctx) :
    BaseBaker(std::move(ctx), NAME)
#if FO_SPARK_PARTICLES
    ,
    _sparkContext {SafeAlloc::MakeUnique<SPK::SPKContext>()}
#endif
{
    FO_STACK_TRACE_ENTRY();

#if FO_SPARK_PARTICLES
    SPK::FO::EnsureSparkParticleObjectsRegistered(*_sparkContext);
#endif
}

ParticleBaker::~ParticleBaker()
{
    FO_STACK_TRACE_ENTRY();
}

void ParticleBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<File> spark_files;
    vector<File> effekseer_files;
    const string target_ext = target_path.empty() ? string {} : strex(target_path).get_file_extension();

    if (target_path.empty()) {
        for (const auto& file_header : files) {
            const string ext = strex(file_header.GetPath()).get_file_extension();

#if FO_SPARK_PARTICLES
            if (ext == "spk") {
                throw ParticleBakerException("Authored SPARK particles must use the text .spark format", file_header.GetPath());
            }

            if (ext == "spark") {
                const string output_path = strex(file_header.GetPath()).change_file_extension("spk");

                if (!_context->BakeChecker || _context->BakeChecker(output_path, file_header.GetWriteTime())) {
                    spark_files.emplace_back(File::Load(file_header));
                }

                continue;
            }
#endif

#if FO_EFFEKSEER_PARTICLES
            if (ext == "efk") {
                throw ParticleBakerException("Authored Effekseer particles must use the text .efkproj format", file_header.GetPath());
            }

            if (ext == "efkproj") {
                const string output_path = strex(file_header.GetPath()).change_file_extension("efk");
                const optional<uint64_t> dependency_write_time = TryGetCachedEffekseerDependencyWriteTime(*_context, file_header, output_path);

                if (dependency_write_time) {
                    if (!_context->BakeChecker || _context->BakeChecker(output_path, *dependency_write_time)) {
                        effekseer_files.emplace_back(File::Load(file_header));
                    }
                }
                else {
                    File file = File::Load(file_header);
                    const uint64_t fresh_dependency_write_time = RefreshEffekseerDependencySnapshot(*_context, file, output_path);

                    if (!_context->BakeChecker || _context->BakeChecker(output_path, fresh_dependency_write_time)) {
                        effekseer_files.emplace_back(std::move(file));
                    }
                }
            }
#endif
            ignore_unused(ext);
        }
    }
    else {
#if FO_SPARK_PARTICLES
        if (target_ext == "spk") {
            const string source_path = strex(target_path).change_file_extension("spark");

            if (files.FindFileByPath(target_path)) {
                throw ParticleBakerException("Authored SPARK particles must use the text .spark format", source_path);
            }

            auto file = files.FindFileByPath(source_path);

            if (file && (!_context->BakeChecker || _context->BakeChecker(target_path, file.GetWriteTime()))) {
                spark_files.emplace_back(std::move(file));
            }
        }
#endif
#if FO_EFFEKSEER_PARTICLES
        if (target_ext == "efk") {
            const string source_path = strex(target_path).change_file_extension("efkproj");
            const string output_path = strex(source_path).change_file_extension("efk");

            if (files.FindFileByPath(output_path)) {
                throw ParticleBakerException("Authored Effekseer particles must use the text .efkproj format", source_path);
            }

            auto file = files.FindFileByPath(source_path);

            if (file) {
                const optional<uint64_t> cached_dependency_write_time = TryGetCachedEffekseerDependencyWriteTime(*_context, file, output_path);
                const uint64_t dependency_write_time = cached_dependency_write_time ? *cached_dependency_write_time : RefreshEffekseerDependencySnapshot(*_context, file, output_path);

                if (!_context->BakeChecker || _context->BakeChecker(output_path, dependency_write_time)) {
                    effekseer_files.emplace_back(std::move(file));
                }
            }
        }
#endif
        ignore_unused(target_ext);
    }

#if FO_SPARK_PARTICLES
    for (const auto& file : spark_files) {
        BakeSparkFile(file);
    }
#else
    ignore_unused(spark_files);
#endif

#if FO_EFFEKSEER_PARTICLES
    if (!effekseer_files.empty()) {
        BakeEffekseerFiles(effekseer_files);
    }
#else
    ignore_unused(effekseer_files);
#endif
}

#if FO_SPARK_PARTICLES
void ParticleBaker::BakeSparkFile(const File& file) const
{
    FO_STACK_TRACE_ENTRY();

    const string_view source_path = file.GetPath();
    const string output_path = strex(source_path).change_file_extension("spk");

    WriteLog("Baking SPARK particle: {} -> {}", source_path, output_path);

    // Load SPARK XML
    const_span<uint8_t> file_data = file.GetDataSpan();
    auto system = _sparkContext->getIOManager().loadFromBuffer("xml", ptr<const uint8_t> {file_data.data()}.reinterpret_as<char>().get(), numeric_cast<unsigned>(file_data.size()));

    if (!system) {
        throw ParticleBakerException("Failed to load SPARK particle XML", source_path);
    }

    ValidateSparkTexturePaths(file, system);

    // Save to SPARK binary format
    std::ostringstream oss(std::ios::binary);

    if (!_sparkContext->getIOManager().save("spk", oss, system)) {
        throw ParticleBakerException("Failed to save SPARK particle binary", source_path);
    }

    const std::string str = oss.str();
    vector<uint8_t> binary(str.begin(), str.end());

    _context->WriteData(output_path, binary);
}
#endif

#if FO_EFFEKSEER_PARTICLES
void ParticleBaker::BakeEffekseerFiles(const_span<File> files) const
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(!files.empty(), "Effekseer compiler received an empty project list");

    for (const File& file : files) {
        if (!file.GetDataSource()->IsDiskDir()) {
            throw ParticleBakerException("Effekseer text projects can only be compiled from a directory resource source", file.GetPath(), file.GetDataSource()->GetPackName());
        }

        const string project_path = fs_resolve_path(file.GetDiskPath());
        const string output_path = strex(file.GetPath()).change_file_extension("efk");
        EffekseerCompilerOutput compiled;

        try {
            compiled = CompileEffekseerProject(project_path, file.GetDataSpan());
        }
        catch (const EffekseerCompilerException& ex) {
            throw ParticleBakerException("Effekseer project compiler failed", file.GetPath(), ex.what());
        }

        ValidateEffekseerRuntimeBinary(output_path, compiled.Binary);
        const vector<string> dependency_paths = ResolveEffekseerDependencyPaths(file, compiled.Dependencies);
        uint64_t dependency_write_time = 0;
        const string dependency_snapshot = BuildEffekseerDependencySnapshot(project_path, file.GetSize(), file.GetWriteTime(), dependency_paths, dependency_write_time);
        WriteLog("Baking Effekseer particle: {} -> {}", file.GetPath(), output_path);
        _context->WriteData(output_path, compiled.Binary);
        const string cache_path = GetEffekseerDependencyCachePath(*_context, output_path);

        if (!cache_path.empty() && !fs_write_file(cache_path, dependency_snapshot)) {
            throw ParticleBakerException("Failed to write Effekseer dependency cache", output_path, cache_path);
        }
    }
}
#endif

FO_END_NAMESPACE
