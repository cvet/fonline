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

#include "AudioBaker-Acm.h"

#if FO_AUDIO_ACM

FO_BEGIN_NAMESPACE

namespace AudioBakerAcm
{

    static constexpr uint32_t ACM_SIGNATURE = 0x032897;
    static constexpr uint32_t ACM_VERSION = 1;
    static constexpr size_t AMPLITUDE_TABLE_SIZE = 0x10000;
    static constexpr int32_t AMPLITUDE_TABLE_MIDDLE = 0x8000;
    static constexpr string_view MUSIC_DIR = "sound/music/";

    // Three base-3 digits packed two bits apiece, indexed by a 5-bit code
    static constexpr std::array<uint8_t, 27> TRIPLETS_OF_3 = {0, 1, 2, 4, 5, 6, 8, 9, 10, 16, 17, 18, 20, 21, 22, 24, 25, 26, 32, 33, 34, 36, 37, 38, 40, 41, 42};

    // Three base-5 digits packed three bits apiece, indexed by a 7-bit code
    static constexpr std::array<uint16_t, 125> TRIPLETS_OF_5 = {0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28, 32, 33, 34, 35, 36, //
        64, 65, 66, 67, 68, 72, 73, 74, 75, 76, 80, 81, 82, 83, 84, 88, 89, 90, 91, 92, 96, 97, 98, 99, 100, //
        128, 129, 130, 131, 132, 136, 137, 138, 139, 140, 144, 145, 146, 147, 148, 152, 153, 154, 155, 156, 160, 161, 162, 163, 164, //
        192, 193, 194, 195, 196, 200, 201, 202, 203, 204, 208, 209, 210, 211, 212, 216, 217, 218, 219, 220, 224, 225, 226, 227, 228, //
        256, 257, 258, 259, 260, 264, 265, 266, 267, 268, 272, 273, 274, 275, 276, 280, 281, 282, 283, 284, 288, 289, 290, 291, 292};

    // Two base-11 digits packed four bits apiece, indexed by a 7-bit code
    static constexpr std::array<uint8_t, 121> PAIRS_OF_11 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, //
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, //
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, //
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, //
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, //
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, //
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, //
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, //
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, //
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, //
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA};

    Decoder::Decoder(const_span<uint8_t> data) :
        _data {data}
    {
        if ((GetBits(24) & 0xFFFFFF) != ACM_SIGNATURE || (GetBits(8) & 0xFF) != ACM_VERSION) {
            throw DecoderException("Not an ACM stream");
        }

        uint32_t samples_low = GetBits(16) & 0xFFFF;
        uint32_t samples_high = GetBits(16) & 0xFFFF;
        _sampleCount = numeric_cast<int32_t>(samples_low | (samples_high << 16));
        _valsToGo = _sampleCount;
        _channels = numeric_cast<int32_t>(GetBits(16) & 0xFFFF);
        _sampleRate = numeric_cast<int32_t>(GetBits(16) & 0xFFFF);

        _packAttrs = numeric_cast<int32_t>(GetBits(4) & 0xF);
        _packAttrs2 = numeric_cast<int32_t>(GetBits(12) & 0xFFF);

        if (_packAttrs2 == 0) {
            throw DecoderException("ACM stream declares an empty block", _sampleCount);
        }

        _someSize = 1 << _packAttrs;
        _blocks = std::max(0x800 / _someSize - 2, 1);
        _totalBlockSize = _blocks * _someSize;

        _someBuff.resize(numeric_cast<size_t>(_someSize) * numeric_cast<size_t>(_packAttrs2));
        _amplitudes.resize(AMPLITUDE_TABLE_SIZE);

        // The original kept one int buffer of 3 * size / 2 - 2 entries and read its first size / 2 entries as twice as
        // many shorts; the two views never overlap, so they are two arrays here
        if (_packAttrs != 0) {
            _firstLevelDecomp.resize(numeric_cast<size_t>(_someSize));
            _nextLevelDecomp.resize(numeric_cast<size_t>(_someSize - 2));
        }
    }

    auto Decoder::Decode() -> vector<int16_t>
    {
        FO_TRACE_ZONE(Baking);

        FO_VERIFY_AND_THROW(!_decoded, "ACM stream is already decoded");
        _decoded = true;

        vector<int16_t> samples;
        samples.reserve(numeric_cast<size_t>(_sampleCount));

        while (numeric_cast<int32_t>(samples.size()) < _sampleCount) {
            if (_valCount == 0) {
                MakeNewValues();
            }

            // Truncation to 16 bits is the format's own, a louder value wraps as it always did
            samples.emplace_back(static_cast<int16_t>(_someBuff[_valuesPos] >> _packAttrs));
            _valuesPos++;
            _valCount--;
        }

        return samples;
    }

    auto Decoder::ReadByte() noexcept -> uint32_t
    {
        // A truncated stream reads on as zeros, which is what the original portion reader handed out past the end
        if (_dataPos >= _data.size()) {
            return 0;
        }

        return _data[_dataPos++];
    }

    void Decoder::PrepareBits(int32_t bits) noexcept
    {
        while (bits > _availBits) {
            _nextBits |= ReadByte() << _availBits;
            _availBits += 8;
        }
    }

    auto Decoder::GetBits(int32_t bits) noexcept -> uint32_t
    {
        PrepareBits(bits);
        uint32_t result = _nextBits;
        SkipBits(bits);
        return result;
    }

    void Decoder::SkipBits(int32_t bits) noexcept
    {
        _availBits -= bits;
        _nextBits >>= bits;
    }

    auto Decoder::Amplitude(int32_t index) const -> int32_t
    {
        return _amplitudes[numeric_cast<size_t>(AMPLITUDE_TABLE_MIDDLE + index)];
    }

    auto Decoder::Cell(int32_t row, int32_t pass) -> int32_t&
    {
        return _someBuff[numeric_cast<size_t>(row) * numeric_cast<size_t>(_someSize) + numeric_cast<size_t>(pass)];
    }

    void Decoder::MakeNewValues()
    {
        CreateAmplitudeDictionary();
        UnpackValues();

        _valuesPos = 0;
        _valCount = std::min(_someSize * _packAttrs2, _valsToGo);
        _valsToGo -= _valCount;
    }

    void Decoder::CreateAmplitudeDictionary()
    {
        int32_t power = numeric_cast<int32_t>(GetBits(4) & 0xF);
        int64_t step = numeric_cast<int64_t>(GetBits(16) & 0xFFFF);
        int32_t count = 1 << power;

        // Entries are shorts, so a step times a large index wraps the way the original's short table did
        int64_t value = 0;

        for (int32_t i = 0; i < count; i++) {
            _amplitudes[numeric_cast<size_t>(AMPLITUDE_TABLE_MIDDLE + i)] = static_cast<int16_t>(value);
            value += step;
        }

        value = -step;

        for (int32_t i = 0; i < count; i++) {
            _amplitudes[numeric_cast<size_t>(AMPLITUDE_TABLE_MIDDLE - i - 1)] = static_cast<int16_t>(value);
            value -= step;
        }

        for (int32_t pass = 0; pass < _someSize; pass++) {
            int32_t ind = numeric_cast<int32_t>(GetBits(5) & 0x1F);
            FillColumn(pass, ind);
        }
    }

    void Decoder::FillColumn(int32_t pass, int32_t ind)
    {
        switch (ind) {
        case 0:
            ZeroFill(pass);
            break;
        case 17:
            K1Bits3(pass);
            break;
        case 18:
            K1Bits2(pass);
            break;
        case 19:
            T1Bits5(pass);
            break;
        case 20:
            K2Bits4(pass);
            break;
        case 21:
            K2Bits3(pass);
            break;
        case 22:
            T2Bits7(pass);
            break;
        case 23:
            K3Bits5(pass);
            break;
        case 24:
            K3Bits4(pass);
            break;
        case 26:
            K4Bits5(pass);
            break;
        case 27:
            K4Bits4(pass);
            break;
        case 29:
            T3Bits7(pass);
            break;
        default:
            if (ind >= 3 && ind <= 16) {
                LinearFill(pass, ind);
                break;
            }

            throw DecoderException("ACM stream uses an undefined column coding", pass, ind);
        }
    }

    void Decoder::UnpackValues()
    {
        if (_packAttrs == 0) {
            return;
        }

        int32_t counter = _packAttrs2;
        size_t block_start = 0;

        while (counter > 0) {
            size_t dec_start = 0;
            int32_t size = _someSize / 2;
            int32_t blocks = std::min(_blocks, counter) * 2;

            TransformFirstLevel(block_start, size, blocks);

            for (int32_t i = 0; i < blocks; i++) {
                _someBuff[block_start + numeric_cast<size_t>(i * size)]++;
            }

            size /= 2;
            blocks *= 2;

            while (size != 0) {
                TransformNextLevel(dec_start, block_start, size, blocks);
                dec_start += numeric_cast<size_t>(size * 2);
                size /= 2;
                blocks *= 2;
            }

            counter -= _blocks;
            block_start += numeric_cast<size_t>(_totalBlockSize);
        }
    }

    // The first level keeps its running pair in shorts, which truncates it; the next levels keep ints
    void Decoder::TransformFirstLevel(size_t block_start, int32_t size, int32_t blocks)
    {
        vector<int16_t>& dec = _firstLevelDecomp;
        size_t row = numeric_cast<size_t>(size);
        int32_t row_0 = 0;
        int32_t row_1 = 0;
        int32_t row_2 = 0;
        int32_t row_3 = 0;

        for (int32_t i = 0; i < size; i++) {
            size_t d = numeric_cast<size_t>(i) * 2;
            size_t s = block_start + numeric_cast<size_t>(i);

            if (blocks == 2) {
                row_0 = _someBuff[s];
                row_1 = _someBuff[s + row];
                _someBuff[s] = _someBuff[s] + dec[d] + 2 * dec[d + 1];
                _someBuff[s + row] = 2 * row_0 - dec[d + 1] - _someBuff[s + row];
                dec[d] = static_cast<int16_t>(row_0);
                dec[d + 1] = static_cast<int16_t>(row_1);
            }
            else if (blocks == 4) {
                row_0 = _someBuff[s];
                row_1 = _someBuff[s + row];
                row_2 = _someBuff[s + 2 * row];
                row_3 = _someBuff[s + 3 * row];
                _someBuff[s] = dec[d] + 2 * dec[d + 1] + row_0;
                _someBuff[s + row] = -dec[d + 1] + 2 * row_0 - row_1;
                _someBuff[s + 2 * row] = row_0 + 2 * row_1 + row_2;
                _someBuff[s + 3 * row] = -row_1 + 2 * row_2 - row_3;
                dec[d] = static_cast<int16_t>(row_2);
                dec[d + 1] = static_cast<int16_t>(row_3);
            }
            else {
                size_t p = s;
                int32_t db_0 = 0;
                int32_t db_1 = 0;

                if (((blocks >> 1) & 1) != 0) {
                    row_0 = _someBuff[p];
                    row_1 = _someBuff[p + row];
                    _someBuff[p] = dec[d] + 2 * dec[d + 1] + row_0;
                    _someBuff[p + row] = -dec[d + 1] + 2 * row_0 - row_1;
                    p += 2 * row;
                    db_0 = row_0;
                    db_1 = row_1;
                }
                else {
                    db_0 = dec[d];
                    db_1 = dec[d + 1];
                }

                for (int32_t j = 0; j < blocks >> 2; j++) {
                    row_0 = _someBuff[p];
                    _someBuff[p] = db_0 + 2 * db_1 + row_0;
                    p += row;
                    row_1 = _someBuff[p];
                    _someBuff[p] = -db_1 + 2 * row_0 - row_1;
                    p += row;
                    row_2 = _someBuff[p];
                    _someBuff[p] = row_0 + 2 * row_1 + row_2;
                    p += row;
                    row_3 = _someBuff[p];
                    _someBuff[p] = -row_1 + 2 * row_2 - row_3;
                    p += row;
                    db_0 = row_2;
                    db_1 = row_3;
                }

                dec[d] = static_cast<int16_t>(row_2);
                dec[d + 1] = static_cast<int16_t>(row_3);
            }
        }
    }

    void Decoder::TransformNextLevel(size_t dec_start, size_t block_start, int32_t size, int32_t blocks)
    {
        vector<int32_t>& dec = _nextLevelDecomp;
        size_t row = numeric_cast<size_t>(size);
        int32_t row_0 = 0;
        int32_t row_1 = 0;
        int32_t row_2 = 0;
        int32_t row_3 = 0;

        for (int32_t i = 0; i < size; i++) {
            size_t d = dec_start + numeric_cast<size_t>(i) * 2;
            size_t s = block_start + numeric_cast<size_t>(i);

            if (blocks == 4) {
                row_0 = _someBuff[s];
                row_1 = _someBuff[s + row];
                row_2 = _someBuff[s + 2 * row];
                row_3 = _someBuff[s + 3 * row];
                _someBuff[s] = dec[d] + 2 * dec[d + 1] + row_0;
                _someBuff[s + row] = -dec[d + 1] + 2 * row_0 - row_1;
                _someBuff[s + 2 * row] = row_0 + 2 * row_1 + row_2;
                _someBuff[s + 3 * row] = -row_1 + 2 * row_2 - row_3;
                dec[d] = row_2;
                dec[d + 1] = row_3;
            }
            else {
                size_t p = s;
                int32_t db_0 = dec[d];
                int32_t db_1 = dec[d + 1];

                for (int32_t j = 0; j < blocks >> 2; j++) {
                    row_0 = _someBuff[p];
                    _someBuff[p] = db_0 + 2 * db_1 + row_0;
                    p += row;
                    row_1 = _someBuff[p];
                    _someBuff[p] = -db_1 + 2 * row_0 - row_1;
                    p += row;
                    row_2 = _someBuff[p];
                    _someBuff[p] = row_0 + 2 * row_1 + row_2;
                    p += row;
                    row_3 = _someBuff[p];
                    _someBuff[p] = -row_1 + 2 * row_2 - row_3;
                    p += row;
                    db_0 = row_2;
                    db_1 = row_3;
                }

                dec[d] = row_2;
                dec[d + 1] = row_3;
            }
        }
    }

    // Column fillers: each decodes one column of the block from a coding chosen per column by the encoder

    void Decoder::ZeroFill(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            Cell(i, pass) = 0;
        }
    }

    void Decoder::LinearFill(int32_t pass, int32_t ind)
    {
        uint32_t mask = (1u << ind) - 1;
        int32_t base = -(1 << (ind - 1));

        for (int32_t i = 0; i < _packAttrs2; i++) {
            Cell(i, pass) = Amplitude(base + numeric_cast<int32_t>(GetBits(ind) & mask));
        }
    }

    // Zeros, often paired, and +-1
    void Decoder::K1Bits3(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(3);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;

                if (++i == _packAttrs2) {
                    break;
                }

                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 2) == 0) {
                SkipBits(2);
                Cell(i, pass) = 0;
            }
            else {
                Cell(i, pass) = (_nextBits & 4) != 0 ? Amplitude(1) : Amplitude(-1);
                SkipBits(3);
            }
        }
    }

    // Zeros and +-1
    void Decoder::K1Bits2(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(2);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;
            }
            else {
                Cell(i, pass) = (_nextBits & 2) != 0 ? Amplitude(1) : Amplitude(-1);
                SkipBits(2);
            }
        }
    }

    // Every triplet of -1, 0 and +1
    void Decoder::T1Bits5(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            uint32_t code = GetBits(5) & 0x1F;

            if (code >= TRIPLETS_OF_3.size()) {
                throw DecoderException("ACM stream uses an undefined triplet code", code);
            }

            int32_t bits = TRIPLETS_OF_3[code];
            Cell(i, pass) = Amplitude(-1 + (bits & 3));

            if (++i == _packAttrs2) {
                break;
            }

            bits >>= 2;
            Cell(i, pass) = Amplitude(-1 + (bits & 3));

            if (++i == _packAttrs2) {
                break;
            }

            bits >>= 2;
            Cell(i, pass) = Amplitude(-1 + bits);
        }
    }

    // -2..+2 with paired zeros
    void Decoder::K2Bits4(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(4);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;

                if (++i == _packAttrs2) {
                    break;
                }

                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 2) == 0) {
                SkipBits(2);
                Cell(i, pass) = 0;
            }
            else {
                if ((_nextBits & 8) != 0) {
                    Cell(i, pass) = (_nextBits & 4) != 0 ? Amplitude(2) : Amplitude(1);
                }
                else {
                    Cell(i, pass) = (_nextBits & 4) != 0 ? Amplitude(-1) : Amplitude(-2);
                }

                SkipBits(4);
            }
        }
    }

    // -2..+2
    void Decoder::K2Bits3(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(3);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;
            }
            else {
                if ((_nextBits & 4) != 0) {
                    Cell(i, pass) = (_nextBits & 2) != 0 ? Amplitude(2) : Amplitude(1);
                }
                else {
                    Cell(i, pass) = (_nextBits & 2) != 0 ? Amplitude(-1) : Amplitude(-2);
                }

                SkipBits(3);
            }
        }
    }

    // Every triplet of -2..+2
    void Decoder::T2Bits7(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            uint32_t code = GetBits(7) & 0x7F;

            if (code >= TRIPLETS_OF_5.size()) {
                throw DecoderException("ACM stream uses an undefined triplet code", code);
            }

            int32_t val = TRIPLETS_OF_5[code];
            Cell(i, pass) = Amplitude(-2 + (val & 7));

            if (++i == _packAttrs2) {
                break;
            }

            val >>= 3;
            Cell(i, pass) = Amplitude(-2 + (val & 7));

            if (++i == _packAttrs2) {
                break;
            }

            val >>= 3;
            Cell(i, pass) = Amplitude(-2 + val);
        }
    }

    // -3..+3 with paired zeros
    void Decoder::K3Bits5(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(5);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;

                if (++i == _packAttrs2) {
                    break;
                }

                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 2) == 0) {
                SkipBits(2);
                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 4) == 0) {
                Cell(i, pass) = (_nextBits & 8) != 0 ? Amplitude(1) : Amplitude(-1);
                SkipBits(4);
            }
            else {
                int32_t val = numeric_cast<int32_t>((_nextBits & 0x18) >> 3);
                SkipBits(5);

                if (val >= 2) {
                    val += 3;
                }

                Cell(i, pass) = Amplitude(-3 + val);
            }
        }
    }

    // -3..+3
    void Decoder::K3Bits4(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(4);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 2) == 0) {
                Cell(i, pass) = (_nextBits & 4) != 0 ? Amplitude(1) : Amplitude(-1);
                SkipBits(3);
            }
            else {
                int32_t val = numeric_cast<int32_t>((_nextBits & 0xC) >> 2);
                SkipBits(4);

                if (val >= 2) {
                    val += 3;
                }

                Cell(i, pass) = Amplitude(-3 + val);
            }
        }
    }

    // -4..+4 with paired zeros
    void Decoder::K4Bits5(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(5);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;

                if (++i == _packAttrs2) {
                    break;
                }

                Cell(i, pass) = 0;
            }
            else if ((_nextBits & 2) == 0) {
                SkipBits(2);
                Cell(i, pass) = 0;
            }
            else {
                int32_t val = numeric_cast<int32_t>((_nextBits & 0x1C) >> 2);

                if (val >= 4) {
                    val++;
                }

                Cell(i, pass) = Amplitude(-4 + val);
                SkipBits(5);
            }
        }
    }

    // -4..+4
    void Decoder::K4Bits4(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            PrepareBits(4);

            if ((_nextBits & 1) == 0) {
                SkipBits(1);
                Cell(i, pass) = 0;
            }
            else {
                int32_t val = numeric_cast<int32_t>((_nextBits & 0xE) >> 1);
                SkipBits(4);

                if (val >= 4) {
                    val++;
                }

                Cell(i, pass) = Amplitude(-4 + val);
            }
        }
    }

    // Every pair of -5..+5
    void Decoder::T3Bits7(int32_t pass)
    {
        for (int32_t i = 0; i < _packAttrs2; i++) {
            uint32_t code = GetBits(7) & 0x7F;

            if (code >= PAIRS_OF_11.size()) {
                throw DecoderException("ACM stream uses an undefined pair code", code);
            }

            int32_t val = PAIRS_OF_11[code];
            Cell(i, pass) = Amplitude(-5 + (val & 0xF));

            if (++i == _packAttrs2) {
                break;
            }

            val >>= 4;
            Cell(i, pass) = Amplitude(-5 + val);
        }
    }

    auto Load(string_view fname, FileReader reader) -> AudioBaker::PcmAudio
    {
        FO_TRACE_ZONE(Baking);

        try {
            Decoder decoder {reader.GetDataSpan()};
            FO_VERIFY_AND_THROW(decoder.GetSampleRate() > 0, "ACM stream declares no sample rate");
            FO_VERIFY_AND_THROW(decoder.GetSampleCount() > 0, "ACM stream declares no samples");

            bool is_music = strex(fname).lower().str().starts_with(MUSIC_DIR);

            AudioBaker::PcmAudio pcm;
            pcm.Channels = is_music ? 2 : 1;
            pcm.SampleRate = decoder.GetSampleRate();
            pcm.Samples = decoder.Decode();
            return pcm;
        }
        catch (const std::exception& ex) {
            throw DecoderException("ACM file failed to decode", fname, ex.what());
        }
    }
}

FO_END_NAMESPACE

#endif
