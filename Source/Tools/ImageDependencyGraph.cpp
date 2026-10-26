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

#include "ImageDependencyGraph.h"

#include "ImageRecipe.h"
#include "SpriteResource.h"

FO_BEGIN_NAMESPACE

static auto HashImageBytes(const_span<uint8_t> bytes) -> string
{
    auto digest = crypto::hash_data({bytes});
    string result;
    result.reserve(digest.size() * 2);

    for (uint8_t byte : digest) {
        std::format_to(std::back_inserter(result), "{:02x}", byte);
    }

    return result;
}

static void AppendFingerprintPart(string& target, string_view value)
{
    std::format_to(std::back_inserter(target), "{}:", value.size());
    target.append(value);
}

auto MakeImageBakingFingerprint(const BakingSettings& settings) -> string
{
    string options = strex("ImageContractV8|{}|{}|{}|{}|{}|{}|{}|{}", FO_BUILD_HASH, FO_BAKING_CONFIG_HASH, SPRITE_RESOURCE_VERSION, SPRITE_INFO_VERSION, settings.SpriteMesh.Enabled, settings.SpriteMesh.AlphaThreshold, settings.SpriteMesh.MaxTriangles, settings.SpriteMesh.AreaSavingsWeight).str();

    return HashImageBytes(make_const_span(options));
}

void ImageSourceCatalog::AddFiles(const FileCollection& files)
{
    for (const FileHeader& file : files) {
        _files.insert_or_assign(string {file.GetPath()}, file.Copy());
    }
}

auto ImageSourceCatalog::Find(string_view path) const -> nptr<const FileHeader>
{
    auto it = _files.find(string {path});

    return it != _files.end() ? make_nptr(&it->second) : nullptr;
}

auto ImageSourceCatalog::Snapshot() const -> FileCollection
{
    vector<FileHeader> files;
    files.reserve(_files.size());

    for (const auto& [path, file] : _files) {
        ignore_unused(path);
        files.emplace_back(file.Copy());
    }

    return FileCollection {std::move(files)};
}

auto ImageSourceCatalog::Identify(string_view path) const -> ImageSourceId
{
    auto source = Find(path);
    FO_VERIFY_AND_THROW(source, "Image dependency source is not mounted", path);

    return {string {source->GetDataSource()->GetPackName()}, string {source->GetPath()}};
}

auto GetDependencies(const ImageDependencyGraph& graph, const ImageSourceId& source) -> const vector<ImageSourceId>&
{
    return graph.Dependencies.at(source.Path);
}

auto GetOutputFingerprint(const ImageDependencyGraph& graph, const ImageOutputCandidate& candidate) -> string
{
    string value;
    AppendFingerprintPart(value, graph.BakingFingerprint);
    AppendFingerprintPart(value, candidate.Pack);
    AppendFingerprintPart(value, candidate.ResourcePath);
    AppendFingerprintPart(value, graph.SourceFingerprints.at(candidate.Source.Path));

    return HashImageBytes(make_const_span(value));
}

auto GetPackFingerprint(const ImageDependencyGraph& graph, string_view pack) -> string
{
    string value;
    AppendFingerprintPart(value, graph.BakingFingerprint);
    AppendFingerprintPart(value, pack);
    map<string, string> outputs;

    for (const auto& output : GetRootOutputs(graph.Outputs, pack)) {
        outputs.emplace(output.ResourcePath, GetOutputFingerprint(graph, output));
    }

    for (const auto& [path, fingerprint] : outputs) {
        AppendFingerprintPart(value, path);
        AppendFingerprintPart(value, fingerprint);
    }

    return HashImageBytes(make_const_span(value));
}

auto GetRootOutputs(const ImageOutputPlan& plan, string_view pack) -> const vector<ImageOutputCandidate>&
{
    auto outputs = plan.Roots.find(string {pack});

    return outputs != plan.Roots.end() ? outputs->second : plan.EmptyRoots;
}

auto ImageReferencePath(string_view descriptor_path, string_view reference) -> string
{
    string normalized = strex(reference).normalize_path_slashes().str();
    FO_VERIFY_AND_THROW(!normalized.empty() && !fs::is_absolute_path(normalized), "Image dependency path must be relative", descriptor_path, reference);

    string extension = strex(normalized).get_file_extension();
    string directory = strex(normalized).extract_dir().str();
    string_view stem = strvex(normalized).extract_file_name().erase_file_extension().substring_until('$');
    string without_options = strex(directory).combine_path(strex("{}.{}", stem, extension)).str();
    string path = strex(descriptor_path).extract_dir().combine_path(without_options).str();
    FO_VERIFY_AND_THROW(path != ".." && !path.starts_with("../"), "Image dependency path escapes mounted sources", descriptor_path, reference);

    return path;
}

auto BuildImageDependencyGraph(ImageSourceCatalog sources, const_span<ImageOutputCandidate> candidates, const unordered_map<string, ImageImport::Loader>& loaders, string baking_fingerprint) -> ImageDependencyGraph
{
    ImageDependencyGraph graph;
    graph.Files = sources.Snapshot();
    graph.Sources = std::move(sources);
    graph.BakingFingerprint = std::move(baking_fingerprint);
    graph.Candidates.assign(candidates.begin(), candidates.end());

    for (ImageOutputCandidate& candidate : graph.Candidates) {
        candidate.Source = graph.Sources.Identify(candidate.Source.Path);
    }

    vector<string> chain;
    map<string, size_t> heights;

    auto describe_chain = [&chain](string_view tail) {
        string text;

        for (const string& path : chain) {
            text += path;
            text += " -> ";
        }

        text += tail;

        return text;
    };

    auto visit = [&](auto&& self, const string& path, size_t recipe_depth) -> size_t {
        auto source = graph.Sources.Find(path);
        FO_VERIFY_AND_THROW(source, "Image dependency is missing", describe_chain(path));

        bool cyclic = std::ranges::find(chain, path) != chain.end();
        FO_VERIFY_AND_THROW(!cyclic, "Image dependency cycle", describe_chain(path));

        bool is_recipe = strex(path).get_file_extension() == "fofrm";

        if (auto loader = loaders.find(strex(path).get_file_extension()); loader != loaders.end()) {
            graph.ResourcePaths.try_emplace(path, loader->second.ResolveOutput ? loader->second.ResolveOutput(path) : path);
        }

        if (auto cached = heights.find(path); cached != heights.end()) {
            FO_VERIFY_AND_THROW(recipe_depth + cached->second <= ImageDependencyGraph::MAX_FOFRM_DEPTH, "Image dependency depth exceeds 64 FOFRM nodes", describe_chain(path));

            return cached->second;
        }

        FO_VERIFY_AND_THROW(!is_recipe || recipe_depth < ImageDependencyGraph::MAX_FOFRM_DEPTH, "Image dependency depth exceeds 64 FOFRM nodes", describe_chain(path));

        File input = File::Load(*source);
        string fingerprint_data;
        AppendFingerprintPart(fingerprint_data, source->GetDataSource()->GetPackName());
        AppendFingerprintPart(fingerprint_data, path);
        AppendFingerprintPart(fingerprint_data, HashImageBytes(input.GetDataSpan()));

        vector<ImageImport::DependencyReference> references;

        if (is_recipe) {
            auto recipe = ParseImageRecipe(path, input.GetStr());

            for (const auto& [section, frames] : recipe.References) {
                ignore_unused(section);

                for (const string& reference : frames) {
                    references.push_back({ImageReferencePath(path, reference), false});
                }
            }

            if (!recipe.Source.empty()) {
                references.push_back({ImageReferencePath(path, recipe.Source), false});
            }
        }
        else if (auto loader = loaders.find(strex(path).get_file_extension()); loader != loaders.end() && loader->second.DiscoverDependencies) {
            references = loader->second.DiscoverDependencies(path, graph.Files);
        }

        chain.emplace_back(path);
        auto pop_chain = scope_exit([&chain]() noexcept { chain.pop_back(); });

        vector<ImageSourceId> dependencies;
        set<string> seen;
        size_t height = is_recipe ? 1 : 0;

        for (const auto& reference : references) {
            if (!seen.emplace(reference.Path).second) {
                continue;
            }

            if (reference.Optional && !graph.Sources.Find(reference.Path)) {
                graph.MissingOptional[path].emplace_back(reference.Path);
                AppendFingerprintPart(fingerprint_data, "missing-optional");
                AppendFingerprintPart(fingerprint_data, reference.Path);
                continue;
            }

            size_t child_height = self(self, reference.Path, recipe_depth + (is_recipe ? 1 : 0));
            height = std::max(height, (is_recipe ? 1 : 0) + child_height);
            AppendFingerprintPart(fingerprint_data, reference.Path);
            AppendFingerprintPart(fingerprint_data, graph.SourceFingerprints.at(reference.Path));
            dependencies.emplace_back(graph.Sources.Identify(reference.Path));
            graph.Consumed.emplace(reference.Path);
        }

        graph.Dependencies.emplace(path, std::move(dependencies));
        graph.SourceFingerprints.emplace(path, HashImageBytes(make_const_span(fingerprint_data)));
        heights.emplace(path, height);

        return height;
    };

    for (const ImageOutputCandidate& candidate : candidates) {
        try {
            (void)visit(visit, candidate.Source.Path, 0);
        }
        catch (const std::exception& ex) {
            throw ImageBakerException("Image dependency graph construction failed", candidate.Source.Path, ex.what());
        }
    }

    graph.Outputs = BuildImageOutputPlan(graph);

    return graph;
}

auto BuildImageOutputPlan(const ImageDependencyGraph& graph) -> ImageOutputPlan
{
    ImageOutputPlan plan;

    for (const ImageOutputCandidate& candidate : graph.Candidates) {
        auto& outputs = plan.Roots[candidate.Pack];

        if (!graph.Consumed.contains(candidate.Source.Path)) {
            outputs.emplace_back(candidate);
        }
    }

    return plan;
}

FO_END_NAMESPACE
