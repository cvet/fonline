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

#include "AudioBaker.h"
#include "FileSystem.h"

#if FO_AUDIO_ACM

FO_BEGIN_NAMESPACE

namespace AudioBakerAcm
{

    FO_DECLARE_EXCEPTION(DecoderException);

    // The amplitude table is per decoder so concurrent files do not share unpacker state
    class Decoder final
    {
    public:
        explicit Decoder(const_span<uint8_t> data);
        Decoder(const Decoder&) = delete;
        Decoder(Decoder&&) noexcept = delete;
        auto operator=(const Decoder&) = delete;
        auto operator=(Decoder&&) noexcept = delete;
        ~Decoder() = default;

        [[nodiscard]] auto GetChannels() const noexcept -> int32_t { return _channels; }
        [[nodiscard]] auto GetSampleRate() const noexcept -> int32_t { return _sampleRate; }
        [[nodiscard]] auto GetSampleCount() const noexcept -> int32_t { return _sampleCount; } // All channels together

        [[nodiscard]] auto Decode() -> vector<int16_t>;

    private:
        [[nodiscard]] auto ReadByte() noexcept -> uint32_t;
        void PrepareBits(int32_t bits) noexcept;
        [[nodiscard]] auto GetBits(int32_t bits) noexcept -> uint32_t;
        void SkipBits(int32_t bits) noexcept;
        [[nodiscard]] auto Amplitude(int32_t index) const -> int32_t;
        [[nodiscard]] auto Cell(int32_t row, int32_t pass) -> int32_t&;

        void MakeNewValues();
        void CreateAmplitudeDictionary();
        void FillColumn(int32_t pass, int32_t ind);
        void UnpackValues();
        void TransformFirstLevel(size_t block_start, int32_t size, int32_t blocks);
        void TransformNextLevel(size_t dec_start, size_t block_start, int32_t size, int32_t blocks);

        void ZeroFill(int32_t pass);
        void LinearFill(int32_t pass, int32_t ind);
        void K1Bits3(int32_t pass);
        void K1Bits2(int32_t pass);
        void T1Bits5(int32_t pass);
        void K2Bits4(int32_t pass);
        void K2Bits3(int32_t pass);
        void T2Bits7(int32_t pass);
        void K3Bits5(int32_t pass);
        void K3Bits4(int32_t pass);
        void K4Bits5(int32_t pass);
        void K4Bits4(int32_t pass);
        void T3Bits7(int32_t pass);

        const_span<uint8_t> _data;
        size_t _dataPos {};
        uint32_t _nextBits {};
        int32_t _availBits {};

        int32_t _channels {};
        int32_t _sampleRate {};
        int32_t _sampleCount {};
        int32_t _valsToGo {};

        int32_t _packAttrs {};
        int32_t _packAttrs2 {};
        int32_t _someSize {};
        int32_t _blocks {};
        int32_t _totalBlockSize {};

        vector<int32_t> _someBuff {};
        vector<int16_t> _firstLevelDecomp {};
        vector<int32_t> _nextLevelDecomp {};
        vector<int16_t> _amplitudes {};
        size_t _valuesPos {};
        int32_t _valCount {};
        bool _decoded {};
    };

    // Fallout effect headers may claim stereo over mono data; only sound/music/ uses stereo
    [[nodiscard]] auto Load(string_view fname, FileReader reader) -> AudioBaker::PcmAudio;
}

FO_END_NAMESPACE

#endif
