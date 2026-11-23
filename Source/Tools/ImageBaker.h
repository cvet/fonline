// FOnline Engine
// SPDX-License-Identifier: MIT

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
    static constexpr string_view_nt NAME {"Image"};
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
