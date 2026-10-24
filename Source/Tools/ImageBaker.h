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

#include "Baker.h"
#include "FileSystem.h"
#include "ImageImport.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

struct SpriteInfoFileEntry;
struct ImageOutputCandidate;

class ImageBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME = "Image";
    explicit ImageBaker(shared_ptr<BakingContext> ctx);
    ImageBaker(const ImageBaker&) = delete;
    ImageBaker(ImageBaker&&) noexcept = delete;
    auto operator=(const ImageBaker&) = delete;
    auto operator=(ImageBaker&&) noexcept = delete;
    ~ImageBaker() override = default;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void AddLoader(const ImageImport::LoadFunc& loader, const vector<string>& file_extensions, const ImageImport::OutputResolver& output_resolver = {}, const ImageImport::DependencyDiscovery& dependency_discovery = {});
    [[nodiscard]] auto GetLoaders() const -> const unordered_map<string, ImageImport::Loader>& { return _fileLoaders; }
    [[nodiscard]] auto CollectOutputCandidates(string_view pack, const FileCollection& files) const -> vector<ImageOutputCandidate>;
    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    auto BakeCollection(string_view fname, const ImageImport::FrameCollection& collection) const -> SpriteInfoFileEntry;
    auto LoadAny(string_view fname_with_opt, const FileCollection& files) const -> ImageImport::FrameCollection;

    unordered_map<string, ImageImport::Loader> _fileLoaders {};
};

FO_END_NAMESPACE
