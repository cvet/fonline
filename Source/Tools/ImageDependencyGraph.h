// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "FileSystem.h"
#include "ImageImport.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

struct ImageSourceId
{
    string Mount {};
    string Path {};
};

constexpr auto operator==(const ImageSourceId& lhs, const ImageSourceId& rhs) noexcept -> bool
{
    return lhs.Mount == rhs.Mount && lhs.Path == rhs.Path;
}

class ImageSourceCatalog
{
public:
    [[nodiscard]] auto Find(string_view path) const -> nptr<const FileHeader>;
    [[nodiscard]] auto Snapshot() const -> FileCollection;
    [[nodiscard]] auto Identify(string_view path) const -> ImageSourceId;

    void AddFiles(const FileCollection& files);

private:
    map<string, FileHeader> _files {};
};

struct ImageOutputCandidate
{
    string Pack {};
    ImageSourceId Source {};
    string ResourcePath {};
};

struct ImageOutputPlan
{
    map<string, vector<ImageOutputCandidate>> Roots {};
    vector<ImageOutputCandidate> EmptyRoots {};
};

struct ImageDependencyGraph
{
    static constexpr size_t MAX_FOFRM_DEPTH {64};
    ImageSourceCatalog Sources {};
    FileCollection Files {};
    vector<ImageOutputCandidate> Candidates {};
    map<string, vector<ImageSourceId>> Dependencies {};
    map<string, vector<string>> MissingOptional {};
    map<string, string> ResourcePaths {};
    set<string> Consumed {};
    ImageOutputPlan Outputs {};
    map<string, string> SourceFingerprints {};
    string BakingFingerprint {};
};

auto ImageReferencePath(string_view descriptor_path, string_view reference) -> string;
auto MakeImageBakingFingerprint(const BakingSettings& settings) -> string;
auto BuildImageDependencyGraph(ImageSourceCatalog sources, const_span<ImageOutputCandidate> candidates, const unordered_map<string, ImageImport::Loader>& loaders, string baking_fingerprint = {}) -> ImageDependencyGraph;
auto BuildImageOutputPlan(const ImageDependencyGraph& graph) -> ImageOutputPlan;
auto GetDependencies(const ImageDependencyGraph& graph, const ImageSourceId& source) -> const vector<ImageSourceId>&;
auto GetOutputFingerprint(const ImageDependencyGraph& graph, const ImageOutputCandidate& candidate) -> string;
auto GetPackFingerprint(const ImageDependencyGraph& graph, string_view pack) -> string;
auto GetRootOutputs(const ImageOutputPlan& plan, string_view pack) -> const vector<ImageOutputCandidate>&;

FO_END_NAMESPACE
