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

class ImageBaker : public NonCopyable
{
public:
    ImageBaker(GeometrySettings& sett, FileCollection& all_files);
    void AutoBakeImages();
    void BakeImage(const string& fname_with_opt);
    void FillBakedFiles(map<string, UCharVec>& baked_files);

private:
    static const int MaxFrameSequence = 50;
    static const int MaxDirsMinusOne = 7;

    struct FrameShot : public NonCopyable
    {
        ushort Width {};
        ushort Height {};
        short NextX {};
        short NextY {};
        UCharVec Data {};
        bool Shared {};
        ushort SharedIndex {};
    };

    struct FrameSequence : public NonCopyable
    {
        short OffsX {};
        short OffsY {};
        FrameShot Frames[MaxFrameSequence] {};
    };

    struct FrameCollection : public NonCopyable
    {
        ushort SequenceSize {};
        ushort AnimTicks {};
        FrameSequence Main {};
        bool HaveDirs {};
        FrameSequence Dirs[MaxDirsMinusOne] {};
        string EffectName {};
        string NewExtension {};
    };

    using LoadFunc = std::function<FrameCollection(const string&, const string&, File&)>;

    void ProcessImages(const string& target_ext, LoadFunc loader);
    void BakeCollection(const string& fname, const FrameCollection& collection);

    FrameCollection LoadAny(const string& fname_with_opt);
    FrameCollection LoadFofrm(const string& fname, const string& opt, File& file);
    FrameCollection LoadFrm(const string& fname, const string& opt, File& file);
    FrameCollection LoadFrX(const string& fname, const string& opt, File& file);
    FrameCollection LoadRix(const string& fname, const string& opt, File& file);
    FrameCollection LoadArt(const string& fname, const string& opt, File& file);
    FrameCollection LoadSpr(const string& fname, const string& opt, File& file);
    FrameCollection LoadZar(const string& fname, const string& opt, File& file);
    FrameCollection LoadTil(const string& fname, const string& opt, File& file);
    FrameCollection LoadMos(const string& fname, const string& opt, File& file);
    FrameCollection LoadBam(const string& fname, const string& opt, File& file);
    FrameCollection LoadPng(const string& fname, const string& opt, File& file);
    FrameCollection LoadTga(const string& fname, const string& opt, File& file);

    GeometrySettings& settings;
    FileCollection& allFiles;
    map<string, UCharVec> bakedFiles {};
    unordered_map<string, File> cachedFiles {};
};
