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
#include "Settings.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(AudioBakerException);

// Every audio resource the runtime plays is Ogg Vorbis; the baked file keeps the authored path, so the
// extension names the source format, never the payload
class AudioBaker final : public BaseBaker
{
public:
    static constexpr string_view_nt NAME = "Audio";
    static constexpr string_view NATIVE_EXTENSION = "ogg";

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

    [[nodiscard]] auto IsBakeableExtension(string_view ext) const -> bool;
    [[nodiscard]] auto BakeFile(const File& file) const -> BakedAudioInfo;
    [[nodiscard]] auto LoadWav(string_view fname, FileReader reader) const -> PcmAudio;
    [[nodiscard]] auto EncodeVorbis(string_view fname, const PcmAudio& pcm) const -> vector<uint8_t>;

    unordered_map<string, LoadFunc> _fileLoaders {};
};

FO_END_NAMESPACE
