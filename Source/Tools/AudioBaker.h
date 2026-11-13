// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#include "Baker.h"
#include "FileSystem.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(AudioBakerException);

// Every audio resource the runtime plays is Ogg Vorbis; the baked file keeps the authored path, so the
// extension names the source format, never the payload
class AudioBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME {"Audio"};
    static constexpr string_view NATIVE_EXTENSION {"ogg"};

    struct PcmAudio
    {
        int32_t Channels {};
        int32_t SampleRate {};
        vector<int16_t> Samples {}; // Interleaved
    };

    using LoadFunc = copyable_function<PcmAudio(string_view, FileReader)>;

    explicit AudioBaker(shared_ptr<BakingContext> ctx);
    AudioBaker(const AudioBaker&) = delete;
    AudioBaker(AudioBaker&&) noexcept = delete;
    auto operator=(const AudioBaker&) = delete;
    auto operator=(AudioBaker&&) noexcept = delete;
    ~AudioBaker() override = default;

    [[nodiscard]] auto GetName() const -> string_view override { return NAME; }
    [[nodiscard]] auto GetOrder() const -> int32_t override { return 4; }

    void AddLoader(const LoadFunc& loader, const vector<string>& file_extensions);
    void BakeFiles(const FileCollection& files, string_view target_path) const override;

private:
    struct BakedAudioInfo
    {
        bool Encoded {};
        size_t InputBytes {};
        size_t OutputBytes {};
    };

    auto IsBakeableExtension(string_view path) const -> bool;
    auto BakeFile(const File& file) const -> BakedAudioInfo;
    auto LoadWav(string_view fname, FileReader reader) const -> PcmAudio;
    auto EncodeVorbis(string_view fname, const PcmAudio& pcm) const -> vector<uint8_t>;

    unordered_map<string, LoadFunc> _fileLoaders {};
};

FO_END_NAMESPACE
