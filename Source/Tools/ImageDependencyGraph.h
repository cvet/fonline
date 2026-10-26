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

[[nodiscard]] constexpr auto operator==(const ImageSourceId& lhs, const ImageSourceId& rhs) noexcept -> bool
{
    return lhs.Mount == rhs.Mount && lhs.Path == rhs.Path;
}

class ImageSourceCatalog
{
public:
    void AddFiles(const FileCollection& files);
    auto Find(string_view path) const -> nptr<const FileHeader>;
    auto Snapshot() const -> FileCollection;
    auto Identify(string_view path) const -> ImageSourceId;

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
    static constexpr size_t MAX_FOFRM_DEPTH = 64;
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
