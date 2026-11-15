// FOnline Engine
// SPDX-License-Identifier: MIT

#include "ImageDependencyCache.h"

#include "Baker.h"

FO_BEGIN_NAMESPACE

static constexpr string_view ImageDependencyCacheHeader {"FONLINE_IMAGE_DEPENDENCIES_V1\n"};
static constexpr string_view ImagePackCacheHeader {"FONLINE_IMAGE_INDEX_V1\n"};

auto GetImageDependencyCachePath(const BakingSettings& settings, const ImageOutputCandidate& output) -> string
{
    if (settings.Baking.BakeOutput.empty()) {
        return {};
    }

    return strex(settings.Baking.BakeOutput).combine_path(BAKER_CACHE_DIR).combine_path("Image").combine_path(output.Pack).combine_path(strex("{}.deps", output.ResourcePath)).str();
}

auto ReadImageDependencySnapshot(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint) -> optional<SpriteInfoFileEntry>
{
    string cache_path {GetImageDependencyCachePath(settings, output)};

    if (cache_path.empty() || !fs::exists(strex(settings.Baking.BakeOutput).combine_path(output.Pack).combine_path(output.ResourcePath))) {
        return std::nullopt;
    }

    auto snapshot {fs::read_file(cache_path)};
    string prefix {strex("{}{}\n", ImageDependencyCacheHeader, fingerprint).str()};

    if (!snapshot || !snapshot->starts_with(prefix)) {
        return std::nullopt;
    }

    try {
        auto entries {ReadSpriteInfoFile(cache_path, string_view {*snapshot}.substr(prefix.size()))};

        if (entries.size() != 1 || entries[0].SourcePath != output.Source.Path || entries[0].ResourcePath != output.ResourcePath) {
            return std::nullopt;
        }

        return std::move(entries[0]);
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

auto IsOutputCurrent(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint) -> bool
{
    return ReadImageDependencySnapshot(settings, output, fingerprint).has_value();
}

void WriteImageDependencySnapshot(const BakingSettings& settings, const ImageOutputCandidate& output, string_view fingerprint, const SpriteInfoFileEntry& entry)
{
    string cache_path {GetImageDependencyCachePath(settings, output)};

    if (cache_path.empty()) {
        return;
    }

    string snapshot {strex("{}{}\n{}", ImageDependencyCacheHeader, fingerprint, WriteSpriteInfoFile({entry})).str()};
    bool written {fs::write_file(cache_path, snapshot)};
    FO_VERIFY_AND_THROW(written, "Unable to write image dependency snapshot", cache_path);
}

static auto ImagePackInfoOutput(string_view pack) -> ImageOutputCandidate
{
    return {string {pack}, {}, strex("{}/{}.foinfo", SPRITE_INFO_DIRECTORY, pack).str()};
}

auto IsImagePackInfoCurrent(const BakingSettings& settings, string_view pack, string_view fingerprint) -> bool
{
    auto output {ImagePackInfoOutput(pack)};
    string cache_path {GetImageDependencyCachePath(settings, output)};

    if (cache_path.empty() || !fs::exists(strex(settings.Baking.BakeOutput).combine_path(pack).combine_path(output.ResourcePath))) {
        return false;
    }

    auto snapshot {fs::read_file(cache_path)};

    return snapshot && *snapshot == strex("{}{}\n", ImagePackCacheHeader, fingerprint).str();
}

void WriteImagePackInfoSnapshot(const BakingSettings& settings, string_view pack, string_view fingerprint)
{
    string cache_path {GetImageDependencyCachePath(settings, ImagePackInfoOutput(pack))};

    if (cache_path.empty()) {
        return;
    }

    bool written {fs::write_file(cache_path, strex("{}{}\n", ImagePackCacheHeader, fingerprint).str())};
    FO_VERIFY_AND_THROW(written, "Unable to write image metadata dependency snapshot", cache_path);
}

FO_END_NAMESPACE
