//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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
#include "Settings.h"

DECLARE_EXCEPTION(ImageBakerException);

class ImageBaker final
{
public:
    ImageBaker() = delete;
    explicit ImageBaker(GeometrySettings& settings, FileCollection& all_files);
    ImageBaker(const ImageBaker&) = delete;
    ImageBaker(ImageBaker&&) noexcept = delete;
    auto operator=(const ImageBaker&) = delete;
    auto operator=(ImageBaker&&) noexcept = delete;
    ~ImageBaker() = default;

    void AutoBakeImages();
    void BakeImage(const string& fname_with_opt);
    void FillBakedFiles(map<string, UCharVec>& baked_files);

private:
    static const int MAX_FRAME_SEQUENCE = 50;
    static const int MAX_DIRS_MINUS_ONE = 7;

    struct FrameShot
    {
        ushort Width {};
        ushort Height {};
        short NextX {};
        short NextY {};
        UCharVec Data {};
        bool Shared {};
        ushort SharedIndex {};
    };

    struct FrameSequence
    {
        short OffsX {};
        short OffsY {};
        FrameShot Frames[MAX_FRAME_SEQUENCE] {};
    };

    struct FrameCollection
    {
        ushort SequenceSize {};
        ushort AnimTicks {};
        FrameSequence Main {};
        bool HaveDirs {};
        FrameSequence Dirs[MAX_DIRS_MINUS_ONE] {};
        string EffectName {};
        string NewExtension {};
    };

    using LoadFunc = std::function<FrameCollection(const string&, const string&, File&)>;

    void ProcessImages(const string& target_ext, const LoadFunc& loader);
    void BakeCollection(const string& fname, const FrameCollection& collection);

    auto LoadAny(const string& fname_with_opt) -> FrameCollection;
    auto LoadFofrm(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadFrm(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadFrX(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadRix(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadArt(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadSpr(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadZar(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadTil(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadMos(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadBam(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadPng(const string& fname, const string& opt, File& file) -> FrameCollection;
    auto LoadTga(const string& fname, const string& opt, File& file) -> FrameCollection;

    GeometrySettings& _settings;
    FileCollection& _allFiles;
    map<string, UCharVec> _bakedFiles {};
    unordered_map<string, File> _cachedFiles {};
};
