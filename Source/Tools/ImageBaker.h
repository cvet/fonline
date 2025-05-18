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
// Copyright (c) 2006 - 2025, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

DECLARE_EXCEPTION(ImageBakerException);

class ImageBaker final : public BaseBaker
{
public:
    static constexpr int MAX_DIRS_MINUS_ONE = 7;

    struct FrameShot
    {
        uint16 Width {};
        uint16 Height {};
        int16 NextX {};
        int16 NextY {};
        vector<uint8> Data {};
        bool Shared {};
        uint16 SharedIndex {};
    };

    struct FrameSequence
    {
        int16 OffsX {};
        int16 OffsY {};
        vector<FrameShot> Frames {};
    };

    struct FrameCollection
    {
        uint16 SequenceSize {};
        uint16 AnimTicks {};
        FrameSequence Main {};
        bool HaveDirs {};
        FrameSequence Dirs[MAX_DIRS_MINUS_ONE] {};
        string EffectName {};
        string NewName {};
    };

    using LoadFunc = std::function<FrameCollection(string_view, string_view, FileReader)>;

    ImageBaker(const BakerSettings& settings, string pack_name, BakeCheckerCallback bake_checker, AsyncWriteDataCallback write_data, const FileSystem* baked_files);
    ImageBaker(const ImageBaker&) = delete;
    ImageBaker(ImageBaker&&) noexcept = delete;
    auto operator=(const ImageBaker&) = delete;
    auto operator=(ImageBaker&&) noexcept = delete;
    ~ImageBaker() override = default;

    [[nodiscard]] auto IsExtSupported(string_view ext) const -> bool override;

    void AddLoader(const LoadFunc& loader, const vector<string_view>& file_extensions);
    void BakeFiles(FileCollection files) override;

private:
    void BakeCollection(string_view fname, const FrameCollection& collection);

    [[nodiscard]] auto LoadAny(string_view fname_with_opt) -> FrameCollection;
    [[nodiscard]] auto LoadFofrm(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadFrm(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadFrX(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadRix(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadArt(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadSpr(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadZar(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadTil(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadMos(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadBam(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadPng(string_view fname, string_view opt, FileReader reader) -> FrameCollection;
    [[nodiscard]] auto LoadTga(string_view fname, string_view opt, FileReader reader) -> FrameCollection;

    unordered_map<string, LoadFunc> _fileLoaders {};
    unordered_map<string, File> _cachedFiles {};
    FileCollection _files {};
    mutable std::mutex _filesLocker {};
    bool _nonConstHelper {};
};
