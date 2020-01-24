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

#include "ImageBaker.h"
#include "F2Palette_Include.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "Testing.h"
#include "WinApi_Include.h"

#include "minizip/zip.h"
#include "png.h"

static uchar* LoadPNG(const uchar* data, uint data_size, uint& result_width, uint& result_height);
static uchar* LoadTGA(const uchar* data, uint data_size, uint& result_width, uint& result_height);

ImageBaker::ImageBaker(GeometrySettings& sett, FileCollection& all_files) : settings {sett}, allFiles {all_files}
{
    // Swap palette R&B
    static std::once_flag once;
    std::call_once(once, []() {
        for (uint i = 0; i < sizeof(FoPalette); i += 4)
            std::swap(FoPalette[i], FoPalette[i + 2]);
    });
}

void ImageBaker::AutoBakeImages()
{
    using namespace std::placeholders;
    ProcessImages("fofrm", std::bind(&ImageBaker::LoadFofrm, this, _1, _2, _3));
    ProcessImages("frm", std::bind(&ImageBaker::LoadFrm, this, _1, _2, _3));
    ProcessImages("fr0", std::bind(&ImageBaker::LoadFrX, this, _1, _2, _3));
    ProcessImages("rix", std::bind(&ImageBaker::LoadRix, this, _1, _2, _3));
    ProcessImages("art", std::bind(&ImageBaker::LoadArt, this, _1, _2, _3));
    ProcessImages("zar", std::bind(&ImageBaker::LoadZar, this, _1, _2, _3));
    ProcessImages("til", std::bind(&ImageBaker::LoadTil, this, _1, _2, _3));
    ProcessImages("mos", std::bind(&ImageBaker::LoadMos, this, _1, _2, _3));
    ProcessImages("bam", std::bind(&ImageBaker::LoadBam, this, _1, _2, _3));
    ProcessImages("png", std::bind(&ImageBaker::LoadPng, this, _1, _2, _3));
    ProcessImages("tga", std::bind(&ImageBaker::LoadTga, this, _1, _2, _3));
}

void ImageBaker::BakeImage(const string& fname_with_opt)
{
    if (bakedFiles.count(fname_with_opt))
        return;

    FrameCollection collection = LoadAny(fname_with_opt);
    BakeCollection(fname_with_opt, collection);
}

void ImageBaker::FillBakedFiles(map<string, UCharVec>& baked_files)
{
    for (const auto& kv : bakedFiles)
        baked_files.emplace(kv.first, kv.second);
}

void ImageBaker::ProcessImages(const string& target_ext, LoadFunc loader)
{
    allFiles.ResetCounter();
    while (allFiles.MoveNext())
    {
        FileHeader file_header = allFiles.GetCurFileHeader();
        string relative_path = file_header.GetPath().substr(allFiles.GetPath().length());
        if (bakedFiles.count(relative_path))
            continue;

        string ext = _str(relative_path).getFileExtension();
        if (target_ext != ext)
            continue;

        File file = allFiles.GetCurFile();
        FrameCollection collection = loader(relative_path, "", file);
        BakeCollection(relative_path, collection);
    }
}

void ImageBaker::BakeCollection(const string& fname, const FrameCollection& collection)
{
    RUNTIME_ASSERT(!bakedFiles.count(fname));

    UCharVec data;
    DataWriter writer {data};
    const ushort check_number = 42;
    uchar dirs = (collection.HaveDirs ? settings.MapDirCount : 1);

    writer.Write(check_number);
    writer.Write(collection.SequenceSize);
    writer.Write(collection.AnimTicks);
    writer.Write(dirs);

    for (auto dir = 0; dir < dirs; dir++)
    {
        const FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);
        writer.Write(sequence.OffsX);
        writer.Write(sequence.OffsY);

        for (uint i = 0; i < collection.SequenceSize; i++)
        {
            const FrameShot& shot = sequence.Frames[i];
            writer.Write(shot.Shared);
            if (!shot.Shared)
            {
                writer.Write(shot.Width);
                writer.Write(shot.Height);
                writer.Write(shot.NextX);
                writer.Write(shot.NextY);
                writer.WritePtr(&shot.Data[0], shot.Data.size());
                RUNTIME_ASSERT(shot.Data.size() == shot.Width * shot.Height * 4);
            }
            else
            {
                writer.Write(shot.SharedIndex);
            }
        }
    }

    writer.Write(check_number);

    if (!collection.NewExtension.empty())
        bakedFiles.emplace(_str(fname).eraseFileExtension() + "." + collection.NewExtension, std::move(data));
    else
        bakedFiles.emplace(fname, std::move(data));
}

ImageBaker::FrameCollection ImageBaker::LoadAny(const string& fname_with_opt)
{
    string ext = _str(fname_with_opt).getFileExtension();
    string fname = _str("{}/{}.{}", _str(fname_with_opt).extractDir(),
        _str(fname_with_opt).extractFileName().eraseFileExtension().substringUntil('$'), ext);
    string opt = _str(fname_with_opt).extractFileName().eraseFileExtension().substringAfter('$');

    File temp_storage {};
    auto find_file = [this, &fname, &ext, &temp_storage]() -> File& {
        auto it = cachedFiles.find(fname);
        if (it != cachedFiles.end())
            return it->second;

        temp_storage = allFiles.FindFile(fname);

        bool need_cache = (temp_storage && ext == "spr");
        if (need_cache)
            return cachedFiles.emplace(fname, std::move(temp_storage)).first->second;

        return temp_storage;
    };

    File& file = find_file();
    if (!file)
        throw ImageBakerException("Image file not found", fname);

    file.SetCurPos(0);

    if (ext == "fofrm")
        return LoadFofrm(fname, opt, file);
    else if (ext == "frm")
        return LoadFrm(fname, opt, file);
    else if (ext == "fr0")
        return LoadFrX(fname, opt, file);
    else if (ext == "rix")
        return LoadRix(fname, opt, file);
    else if (ext == "art")
        return LoadArt(fname, opt, file);
    else if (ext == "spr")
        return LoadSpr(fname, opt, file);
    else if (ext == "zar")
        return LoadZar(fname, opt, file);
    else if (ext == "til")
        return LoadTil(fname, opt, file);
    else if (ext == "mos")
        return LoadMos(fname, opt, file);
    else if (ext == "bam")
        return LoadBam(fname, opt, file);
    else if (ext == "png")
        return LoadPng(fname, opt, file);
    else if (ext == "tga")
        return LoadTga(fname, opt, file);

    throw ImageBakerException("Invalid image file extension", fname);
}

ImageBaker::FrameCollection ImageBaker::LoadFofrm(const string& fname, const string& opt, File& file)
{
    FrameCollection collection;

    // Load ini parser
    ConfigFile fofrm(file.GetCStr());

    ushort frm_fps = fofrm.GetInt("", "fps", 0);
    if (!frm_fps)
        frm_fps = 10;

    ushort frm_num = fofrm.GetInt("", "count", 0);
    if (!frm_num)
        frm_num = 1;

    int ox = fofrm.GetInt("", "offs_x", 0);
    int oy = fofrm.GetInt("", "offs_y", 0);

    collection.EffectName = fofrm.GetStr("", "effect");

    for (int dir = 0; dir < settings.MapDirCount; dir++)
    {
        vector<tuple<FrameCollection, int, int>> sub_collections;
        sub_collections.reserve(10);

        string dir_str = _str("dir_{}", dir);
        if (!fofrm.IsApp(dir_str))
            dir_str = "";

        if (dir_str.empty())
        {
            if (dir == 1)
                break;

            if (dir > 1)
                throw ImageBakerException("FOFRM file invalid apps", fname);
        }
        else
        {
            ox = fofrm.GetInt(dir_str, "offs_x", ox);
            oy = fofrm.GetInt(dir_str, "offs_y", oy);
        }

        string frm_dir = _str(fname).extractDir();

        uint frames = 0;
        bool no_info = false;
        bool load_fail = false;
        for (int frm = 0; frm < frm_num; frm++)
        {
            string frm_name;
            if ((frm_name = fofrm.GetStr(dir_str, _str("frm_{}", frm))).empty() &&
                (frm != 0 || (frm_name = fofrm.GetStr(dir_str, "frm")).empty()))
            {
                no_info = true;
                break;
            }

            FrameCollection sub_collection = LoadAny(frm_dir + frm_name);
            frames += sub_collection.SequenceSize;

            int next_x = fofrm.GetInt(dir_str, _str("next_x_{}", frm), 0);
            int next_y = fofrm.GetInt(dir_str, _str("next_y_{}", frm), 0);

            // sub_collections.emplace_back(std::move(sub_collection), next_x, next_y);
        }

        // No frames found or error
        if (no_info || load_fail || sub_collections.empty() || (dir > 0 && frames != collection.SequenceSize))
        {
            if (no_info && dir == 1)
                break;

            throw ImageBakerException("FOFRM file invalid data", fname);
        }

        // Allocate animation storage
        if (dir == 0)
        {
            collection.SequenceSize = frames;
            collection.AnimTicks = 1000 * frm_num / frm_fps;
        }
        else
        {
            collection.HaveDirs = true;
        }

        FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);
        sequence.OffsX = ox;
        sequence.OffsY = oy;

        // Merge many animations in one
        uint frm = 0;
        for (auto& sub_collection : sub_collections)
        {
            const auto& [sc, next_x, next_y] = sub_collection;
            for (uint j = 0; j < sc.SequenceSize; j++, frm++)
            {
                FrameShot& shot = sequence.Frames[frm];
                shot.NextX = sc.Main.OffsX + next_x;
                shot.NextY = sc.Main.OffsY + next_y;
            }
        }
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadFrm(const string& fname, const string& opt, File& file)
{
    bool anim_pix = (opt.find('a') != string::npos);

    file.SetCurPos(0x4);
    ushort frm_fps = file.GetBEUShort();
    if (!frm_fps)
        frm_fps = 10;

    file.SetCurPos(0x8);
    ushort frm_num = file.GetBEUShort();

    FrameCollection collection;
    collection.SequenceSize = frm_num;
    collection.AnimTicks = 1000 / frm_fps * frm_num;

    for (int dir = 0; dir < settings.MapDirCount; dir++)
    {
        FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);

        int dir_frm = dir;
        if (!settings.MapHexagonal)
        {
            if (dir >= 3)
                dir_frm--;
            if (dir >= 7)
                dir_frm--;
        }

        file.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = file.GetBEShort();
        file.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = file.GetBEShort();

        file.SetCurPos(0x22 + dir_frm * 4);
        uint offset = 0x3E + file.GetBEUInt();
        if (offset == 0x3E && dir_frm)
        {
            if (dir > 1)
                throw ImageBakerException("FRM file truncated", fname);
            break;
        }

        if (dir == 1)
            collection.HaveDirs = true;

        // Make palette
        uint* palette = (uint*)FoPalette;
        uint palette_entry[256];
        File fm_palette = allFiles.FindFile(_str(fname).eraseFileExtension() + ".pal");
        if (fm_palette)
        {
            for (uint i = 0; i < 256; i++)
            {
                uchar r = fm_palette.GetUChar() * 4;
                uchar g = fm_palette.GetUChar() * 4;
                uchar b = fm_palette.GetUChar() * 4;
                palette_entry[i] = COLOR_RGB(r, g, b);
            }
            palette = palette_entry;
        }

        // Animate pixels
        int anim_pix_type = 0;
        // 0x00 - None
        // 0x01 - Slime, 229 - 232, 4
        // 0x02 - Monitors, 233 - 237, 5
        // 0x04 - FireSlow, 238 - 242, 5
        // 0x08 - FireFast, 243 - 247, 5
        // 0x10 - Shoreline, 248 - 253, 6
        // 0x20 - BlinkingRed, 254, parse on 15 frames
        const uchar blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (int frm = 0; frm < frm_num; frm++)
        {
            FrameShot& shot = sequence.Frames[frm];

            file.SetCurPos(offset);
            ushort w = file.GetBEUShort();
            ushort h = file.GetBEUShort();

            shot.Width = w;
            shot.Height = h;

            file.GoForward(4); // Frame size

            shot.NextX = file.GetBEShort();
            shot.NextY = file.GetBEShort();

            // Allocate data
            shot.Data.resize(w * h * 4);
            uint* ptr = (uint*)&shot.Data[0];
            file.SetCurPos(offset + 12);

            if (!anim_pix_type)
            {
                for (int i = 0, j = w * h; i < j; i++)
                    *(ptr + i) = palette[file.GetUChar()];
            }
            else
            {
                for (int i = 0, j = w * h; i < j; i++)
                {
                    uchar index = file.GetUChar();
                    if (index >= 229 && index < 255)
                    {
                        if (index >= 229 && index <= 232)
                        {
                            index -= frm % 4;
                            if (index < 229)
                                index += 4;
                        }
                        else if (index >= 233 && index <= 237)
                        {
                            index -= frm % 5;
                            if (index < 233)
                                index += 5;
                        }
                        else if (index >= 238 && index <= 242)
                        {
                            index -= frm % 5;
                            if (index < 238)
                                index += 5;
                        }
                        else if (index >= 243 && index <= 247)
                        {
                            index -= frm % 5;
                            if (index < 243)
                                index += 5;
                        }
                        else if (index >= 248 && index <= 253)
                        {
                            index -= frm % 6;
                            if (index < 248)
                                index += 6;
                        }
                        else
                        {
                            *(ptr + i) = COLOR_RGB(blinking_red_vals[frm % 10], 0, 0);
                            continue;
                        }
                    }
                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (!frm && anim_pix && palette == (uint*)FoPalette)
            {
                file.SetCurPos(offset + 12);
                for (int i = 0, j = w * h; i < j; i++)
                {
                    uchar index = file.GetUChar();
                    if (index < 229 || index == 255)
                        continue;
                    if (index >= 229 && index <= 232)
                        anim_pix_type |= 0x01;
                    else if (index >= 233 && index <= 237)
                        anim_pix_type |= 0x02;
                    else if (index >= 238 && index <= 242)
                        anim_pix_type |= 0x04;
                    else if (index >= 243 && index <= 247)
                        anim_pix_type |= 0x08;
                    else if (index >= 248 && index <= 253)
                        anim_pix_type |= 0x10;
                    else
                        anim_pix_type |= 0x20;
                }

                if (anim_pix_type & 0x01)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x04)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x10)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x08)
                    collection.AnimTicks = 142;
                if (anim_pix_type & 0x02)
                    collection.AnimTicks = 100;
                if (anim_pix_type & 0x20)
                    collection.AnimTicks = 100;

                if (anim_pix_type)
                {
                    int divs[4] {1, 1, 1, 1};
                    if (anim_pix_type & 0x01)
                        divs[0] = 4;
                    if (anim_pix_type & 0x02)
                        divs[1] = 5;
                    if (anim_pix_type & 0x04)
                        divs[1] = 5;
                    if (anim_pix_type & 0x08)
                        divs[1] = 5;
                    if (anim_pix_type & 0x10)
                        divs[2] = 6;
                    if (anim_pix_type & 0x20)
                        divs[3] = 10;

                    frm_num = 4;
                    for (int i = 0; i < 4; i++)
                    {
                        if (!(frm_num % divs[i]))
                            continue;

                        frm_num++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_num;
                    collection.SequenceSize = frm_num;
                }
            }

            if (!anim_pix_type)
                offset += w * h + 12;
        }
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadFrX(const string& fname, const string& opt, File& file)
{
    bool anim_pix = (opt.find('a') != string::npos);

    // Load from frm
    file.SetCurPos(0x4);
    ushort frm_fps = file.GetBEUShort();
    if (!frm_fps)
        frm_fps = 10;

    file.SetCurPos(0x8);
    ushort frm_num = file.GetBEUShort();

    FrameCollection collection;
    collection.SequenceSize = frm_num;
    collection.AnimTicks = 1000 / frm_fps * frm_num;
    collection.NewExtension = "frm";

    for (int dir = 0; dir < settings.MapDirCount; dir++)
    {
        FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);

        int dir_frm = dir;
        if (!settings.MapHexagonal)
        {
            if (dir >= 3)
                dir_frm--;
            if (dir >= 7)
                dir_frm--;
        }

        if (dir_frm)
        {
            string next_fname = _str("{}{}", fname.substr(0, fname.size() - 1), '0' + dir_frm);
            file = allFiles.FindFile(next_fname);
            if (!file)
            {
                if (dir > 1)
                    throw ImageBakerException("FRX file not found", next_fname);
                break;
            }
        }

        file.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = file.GetBEShort();
        file.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = file.GetBEShort();

        file.SetCurPos(0x22 + dir_frm * 4);
        uint offset = 0x3E + file.GetBEUInt();

        if (dir == 1)
            collection.HaveDirs = true;

        // Make palette
        uint* palette = (uint*)FoPalette;
        uint palette_entry[256];
        File fm_palette = allFiles.FindFile(_str(fname).eraseFileExtension() + ".pal");
        if (fm_palette)
        {
            for (uint i = 0; i < 256; i++)
            {
                uchar r = fm_palette.GetUChar() * 4;
                uchar g = fm_palette.GetUChar() * 4;
                uchar b = fm_palette.GetUChar() * 4;
                palette_entry[i] = COLOR_RGB(r, g, b);
            }
            palette = palette_entry;
        }

        // Animate pixels
        int anim_pix_type = 0;
        // 0x00 - None
        // 0x01 - Slime, 229 - 232, 4
        // 0x02 - Monitors, 233 - 237, 5
        // 0x04 - FireSlow, 238 - 242, 5
        // 0x08 - FireFast, 243 - 247, 5
        // 0x10 - Shoreline, 248 - 253, 6
        // 0x20 - BlinkingRed, 254, parse on 15 frames
        const uchar blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (int frm = 0; frm < frm_num; frm++)
        {
            FrameShot& shot = sequence.Frames[frm];

            file.SetCurPos(offset);
            ushort w = file.GetBEUShort();
            ushort h = file.GetBEUShort();

            shot.Width = w;
            shot.Height = h;

            file.GoForward(4); // Frame size

            shot.NextX = file.GetBEShort();
            shot.NextY = file.GetBEShort();

            // Allocate data
            shot.Data.resize(w * h * 4);
            uint* ptr = (uint*)&shot.Data[0];
            file.SetCurPos(offset + 12);

            if (!anim_pix_type)
            {
                for (int i = 0, j = w * h; i < j; i++)
                    *(ptr + i) = palette[file.GetUChar()];
            }
            else
            {
                for (int i = 0, j = w * h; i < j; i++)
                {
                    uchar index = file.GetUChar();
                    if (index >= 229 && index < 255)
                    {
                        if (index >= 229 && index <= 232)
                        {
                            index -= frm % 4;
                            if (index < 229)
                                index += 4;
                        }
                        else if (index >= 233 && index <= 237)
                        {
                            index -= frm % 5;
                            if (index < 233)
                                index += 5;
                        }
                        else if (index >= 238 && index <= 242)
                        {
                            index -= frm % 5;
                            if (index < 238)
                                index += 5;
                        }
                        else if (index >= 243 && index <= 247)
                        {
                            index -= frm % 5;
                            if (index < 243)
                                index += 5;
                        }
                        else if (index >= 248 && index <= 253)
                        {
                            index -= frm % 6;
                            if (index < 248)
                                index += 6;
                        }
                        else
                        {
                            *(ptr + i) = COLOR_RGB(blinking_red_vals[frm % 10], 0, 0);
                            continue;
                        }
                    }
                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (!frm && anim_pix && palette == (uint*)FoPalette)
            {
                file.SetCurPos(offset + 12);
                for (int i = 0, j = w * h; i < j; i++)
                {
                    uchar index = file.GetUChar();
                    if (index < 229 || index == 255)
                        continue;
                    if (index >= 229 && index <= 232)
                        anim_pix_type |= 0x01;
                    else if (index >= 233 && index <= 237)
                        anim_pix_type |= 0x02;
                    else if (index >= 238 && index <= 242)
                        anim_pix_type |= 0x04;
                    else if (index >= 243 && index <= 247)
                        anim_pix_type |= 0x08;
                    else if (index >= 248 && index <= 253)
                        anim_pix_type |= 0x10;
                    else
                        anim_pix_type |= 0x20;
                }

                if (anim_pix_type & 0x01)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x04)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x10)
                    collection.AnimTicks = 200;
                if (anim_pix_type & 0x08)
                    collection.AnimTicks = 142;
                if (anim_pix_type & 0x02)
                    collection.AnimTicks = 100;
                if (anim_pix_type & 0x20)
                    collection.AnimTicks = 100;

                if (anim_pix_type)
                {
                    int divs[4];
                    divs[0] = 1;
                    divs[1] = 1;
                    divs[2] = 1;
                    divs[3] = 1;
                    if (anim_pix_type & 0x01)
                        divs[0] = 4;
                    if (anim_pix_type & 0x02)
                        divs[1] = 5;
                    if (anim_pix_type & 0x04)
                        divs[1] = 5;
                    if (anim_pix_type & 0x08)
                        divs[1] = 5;
                    if (anim_pix_type & 0x10)
                        divs[2] = 6;
                    if (anim_pix_type & 0x20)
                        divs[3] = 10;

                    frm_num = 4;
                    for (int i = 0; i < 4; i++)
                    {
                        if (!(frm_num % divs[i]))
                            continue;
                        frm_num++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_num;
                    collection.SequenceSize = frm_num;
                }
            }

            if (!anim_pix_type)
                offset += w * h + 12;
        }
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadRix(const string& fname, const string& opt, File& file)
{
    file.SetCurPos(0x4);
    ushort w;
    file.CopyMem(&w, 2);
    ushort h;
    file.CopyMem(&h, 2);

    file.SetCurPos(0xA);
    uchar* palette = file.GetCurBuf();

    UCharVec data(w * h * 4);
    uint* ptr = (uint*)&data[0];
    file.SetCurPos(0xA + 256 * 3);

    for (int i = 0, j = w * h; i < j; i++)
    {
        uint index = (uint)file.GetUChar() * 3;
        uint r = *(palette + index + 2) * 4;
        uint g = *(palette + index + 1) * 4;
        uint b = *(palette + index + 0) * 4;
        *(ptr + i) = COLOR_RGB(r, g, b);
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadArt(const string& fname, const string& opt, File& file)
{
    int palette_index = 0; // 0..3
    bool transparent = false;
    bool mirror_hor = false;
    bool mirror_ver = false;
    uint frm_from = 0;
    uint frm_to = 100000;

    for (size_t i = 0; i < opt.length(); i++)
    {
        switch (opt[i])
        {
        case '0':
            palette_index = 0;
            break;
        case '1':
            palette_index = 1;
            break;
        case '2':
            palette_index = 2;
            break;
        case '3':
            palette_index = 3;
            break;
        case 'T':
        case 't':
            transparent = true;
            break;
        case 'H':
        case 'h':
            mirror_hor = true;
            break;
        case 'V':
        case 'v':
            mirror_ver = true;
            break;
        case 'F':
        case 'f': {
            // name$1vf5-7.art
            string f = opt.substr(i);
            istringstream idelim(f);
            char ch;
            if (f.find('-') != string::npos)
            {
                idelim >> frm_from >> ch >> frm_to;
                i += 3;
            }
            else
            {
                idelim >> frm_from;
                frm_to = frm_from;
                i += 1;
            }
        }
        break;
        default:
            break;
        }
    }

    struct ArtHeader
    {
        unsigned int flags;
        // 0x00000001 = Static - no rotation, contains frames only for south direction.
        // 0x00000002 = Critter - uses delta attribute, while playing walking animation.
        // 0x00000004 = Font - X offset is equal to number of pixels to advance horizontally for next
        // character. 0x00000008 = Facade - requires fackwalk file. 0x00000010 = Unknown - used in eye candy,
        // for example, DIVINATION.art.
        unsigned int frameRate;
        unsigned int rotationCount;
        unsigned int paletteList[4];
        unsigned int actionFrame;
        unsigned int frameCount;
        unsigned int infoList[8];
        unsigned int sizeList[8];
        unsigned int dataList[8];
    } header;

    struct ArtFrameInfo
    {
        unsigned int frameWidth;
        unsigned int frameHeight;
        unsigned int frameSize;
        signed int offsetX;
        signed int offsetY;
        signed int deltaX;
        signed int deltaY;
    } frame_info;

    using ArtPalette = unsigned int[256];
    ArtPalette palette[4];

    file.CopyMem(&header, sizeof(header));
    if (header.flags & 0x00000001)
        header.rotationCount = 1;

    // Load palettes
    int palette_count = 0;
    for (int i = 0; i < 4; i++)
    {
        if (header.paletteList[i])
        {
            file.CopyMem(&palette[i], sizeof(ArtPalette));
            palette_count++;
        }
    }
    if (palette_index >= palette_count)
        palette_index = 0;

    uint frm_fps = header.frameRate;
    if (!frm_fps)
        frm_fps = 10;
    uint frm_count = header.frameCount;
    uint dir_count = header.rotationCount;

    if (frm_from >= frm_count)
        frm_from = frm_count - 1;
    if (frm_to >= frm_count)
        frm_to = frm_count - 1;
    uint frm_count_anim = MAX(frm_from, frm_to) - MIN(frm_from, frm_to) + 1;

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = frm_count_anim;
    collection.AnimTicks = 1000 / frm_fps * frm_count_anim;
    collection.HaveDirs = (header.rotationCount == 8);

    for (int dir = 0; dir < settings.MapDirCount; dir++)
    {
        FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);

        int dir_art = dir;
        if (settings.MapHexagonal)
        {
            switch (dir_art)
            {
            case 0:
                dir_art = 1;
                break;
            case 1:
                dir_art = 2;
                break;
            case 2:
                dir_art = 3;
                break;
            case 3:
                dir_art = 5;
                break;
            case 4:
                dir_art = 6;
                break;
            case 5:
                dir_art = 7;
                break;
            default:
                throw ImageBakerException("Invalid ART direction", fname);
            }
        }
        else
        {
            dir_art = (dir + 1) % 8;
        }

        // Read data
        uint frm_read = frm_from;
        uint frm_write = 0;
        while (true)
        {
            file.SetCurPos(sizeof(ArtHeader) + sizeof(ArtPalette) * palette_count +
                sizeof(ArtFrameInfo) * dir_art * frm_count + sizeof(ArtFrameInfo) * frm_read);

            file.CopyMem(&frame_info, sizeof(frame_info));

            uint w = frame_info.frameWidth;
            uint h = frame_info.frameHeight;
            UCharVec data(w * h * 4);
            uint* ptr = (uint*)&data[0];

            FrameShot& shot = sequence.Frames[frm_write];
            shot.Width = w;
            shot.Height = h;
            shot.NextX = (-frame_info.offsetX + frame_info.frameWidth / 2) * (mirror_hor ? -1 : 1);
            shot.NextY = (-frame_info.offsetY + frame_info.frameHeight) * (mirror_ver ? -1 : 1);

            uint color = 0;
            auto art_get_color = [&color, &file, &palette, &palette_index, &transparent]() {
                uchar index = file.GetUChar();
                uint color = palette[palette_index][index];
                std::swap(((uchar*)&color)[0], ((uchar*)&color)[2]);
                if (!index)
                    color = 0;
                else if (transparent)
                    color |= MAX((color >> 16) & 0xFF, MAX((color >> 8) & 0xFF, color & 0xFF)) << 24;
                else
                    color |= 0xFF000000;
            };

            uint pos = 0;
            uint x = 0;
            uint y = 0;
            bool mirror = (mirror_hor || mirror_ver);
            auto art_write_color = [&color, &pos, &x, &y, &w, &h, ptr, &mirror, &mirror_hor, &mirror_ver]() {
                if (mirror)
                {
                    *(ptr + ((mirror_ver ? h - y - 1 : y) * w + (mirror_hor ? w - x - 1 : x))) = color;
                    x++;
                    if (x >= w)
                    {
                        x = 0;
                        y++;
                    }
                }
                else
                {
                    *(ptr + pos) = color;
                    pos++;
                }
            };

            if (w * h == frame_info.frameSize)
            {
                for (uint i = 0; i < frame_info.frameSize; i++)
                {
                    art_get_color();
                    art_write_color();
                }
            }
            else
            {
                for (uint i = 0; i < frame_info.frameSize; i++)
                {
                    uchar cmd = file.GetUChar();
                    if (cmd > 128)
                    {
                        cmd -= 128;
                        i += cmd;
                        for (; cmd > 0; cmd--)
                        {
                            art_get_color();
                            art_write_color();
                        }
                    }
                    else
                    {
                        art_get_color();
                        for (; cmd > 0; cmd--)
                            art_write_color();
                        i++;
                    }
                }
            }

            shot.Data = std::move(data);

            if (frm_read == frm_to)
                break;

            if (frm_to > frm_from)
                frm_read++;
            else
                frm_read--;

            frm_write++;
        }

        if (header.rotationCount != 8)
            break;
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadSpr(const string& fname, const string& opt, File& file)
{
    FrameCollection collection;

    for (int dir = 0; dir < settings.MapDirCount; dir++)
    {
        int dir_spr = dir;
        if (settings.MapHexagonal)
        {
            switch (dir_spr)
            {
            case 0:
                dir_spr = 2;
                break;
            case 1:
                dir_spr = 3;
                break;
            case 2:
                dir_spr = 4;
                break;
            case 3:
                dir_spr = 6;
                break;
            case 4:
                dir_spr = 7;
                break;
            case 5:
                dir_spr = 0;
                break;
            default:
                dir_spr = 0;
                break;
            }
        }
        else
        {
            dir_spr = (dir + 2) % 8;
        }

        // Color offsets
        // 0 - other
        // 1 - skin
        // 2 - hair
        // 3 - armor
        int rgb_offs[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

        // Format: fileName$[1,100,0,0][2,0,0,100]animName.spr
        size_t first = opt.find_first_of('[');
        size_t last = opt.find_last_of(']', first);
        string seq_name = (last != string::npos ? opt.substr(last + 1) : opt);

        while (first != string::npos && last != string::npos)
        {
            last = opt.find_first_of(']', first);
            string entry = opt.substr(first, last - first - 1);
            istringstream ientry(entry);

            // Parse numbers
            int rgb[4];
            if (ientry >> rgb[0] >> rgb[1] >> rgb[2] >> rgb[3])
            {
                // To one part
                if (rgb[0] >= 0 && rgb[0] <= 3)
                {
                    rgb_offs[rgb[0]][0] = rgb[1]; // R
                    rgb_offs[rgb[0]][1] = rgb[2]; // G
                    rgb_offs[rgb[0]][2] = rgb[3]; // B
                }
                // To all parts
                else
                {
                    for (int i = 0; i < 4; i++)
                    {
                        rgb_offs[i][0] = rgb[1]; // R
                        rgb_offs[i][1] = rgb[2]; // G
                        rgb_offs[i][2] = rgb[3]; // B
                    }
                }
            }

            first = opt.find_first_of('[', last);
        }

        // Read header
        char head[11];
        file.CopyMem(head, 11);
        if (head[8] || strcmp(head, "<sprite>"))
            throw ImageBakerException("Invalid SPR header", fname);

        float dimension_left = (float)file.GetUChar() * 6.7f;
        float dimension_up = (float)file.GetUChar() * 7.6f;
        UNUSED_VARIABLE(dimension_up);
        float dimension_right = (float)file.GetUChar() * 6.7f;
        int center_x = file.GetLEUInt();
        int center_y = file.GetLEUInt();
        file.GoForward(2); // ushort unknown1  sometimes it is 0, and sometimes it is 3
        file.GoForward(1); // CHAR unknown2  0x64, other values were not observed

        float ta = RAD(127.0f / 2.0f); // Tactics grid angle
        int center_x_ex =
            (int)((dimension_left * sinf(ta) + dimension_right * sinf(ta)) / 2.0f - dimension_left * sinf(ta));
        int center_y_ex = (int)((dimension_left * cosf(ta) + dimension_right * cosf(ta)) / 2.0f);

        uint anim_index = 0;
        UIntVec anim_frames;
        anim_frames.reserve(1000);

        // Find sequence
        bool seq_founded = false;
        uint seq_cnt = file.GetLEUInt();
        for (uint seq = 0; seq < seq_cnt; seq++)
        {
            // Find by name
            uint item_cnt = file.GetLEUInt();
            file.GoForward(sizeof(short) * item_cnt);
            file.GoForward(sizeof(uint) * item_cnt); // uint  unknown3[item_cnt]

            uint name_len = file.GetLEUInt();
            string name = string((const char*)file.GetCurBuf(), name_len);
            file.GoForward(name_len);
            ushort index = file.GetLEUShort();

            if (seq_name.empty() || _str(seq_name).compareIgnoreCase(name))
            {
                anim_index = index;

                // Read frame numbers
                file.GoBack(
                    sizeof(ushort) + name_len + sizeof(uint) + sizeof(uint) * item_cnt + sizeof(short) * item_cnt);

                for (uint i = 0; i < item_cnt; i++)
                {
                    short val = file.GetLEUShort();
                    if (val >= 0)
                    {
                        anim_frames.push_back(val);
                    }
                    else if (val == -43)
                    {
                        file.GoForward(sizeof(short) * 3);
                        i += 3;
                    }
                }

                // Animation founded, go forward
                seq_founded = true;
                break;
            }
        }
        if (!seq_founded)
            throw ImageBakerException("Sequence for SPR not found", fname);

        // Find animation
        file.SetCurPos(0);
        for (uint i = 0; i <= anim_index; i++)
        {
            if (!file.FindFragment((uchar*)"<spranim>", 9, file.GetCurPos()))
                throw ImageBakerException("Spranim for SPR not found", fname);
            file.GoForward(12);
        }

        uint file_offset = file.GetLEUInt();
        file.GoForward(file.GetLEUInt()); // Collection name
        uint frame_cnt = file.GetLEUInt();
        uint dir_cnt = file.GetLEUInt();
        UIntVec bboxes;
        bboxes.resize(frame_cnt * dir_cnt * 4);
        file.CopyMem(&bboxes[0], sizeof(uint) * frame_cnt * dir_cnt * 4);

        // Fix dir
        if (dir_cnt != 8)
            dir_spr = dir_spr * (dir_cnt * 100 / 8) / 100;
        if ((uint)dir_spr >= dir_cnt)
            dir_spr = 0;

        // Check wrong frames
        for (uint i = 0; i < anim_frames.size();)
        {
            if (anim_frames[i] >= frame_cnt)
                anim_frames.erase(anim_frames.begin() + i);
            else
                i++;
        }

        // Get images file
        file.SetCurPos(file_offset);
        file.GoForward(14); // <spranim_img>\0
        uchar type = file.GetUChar();
        file.GoForward(1); // \0

        uint data_len;
        uint cur_pos = file.GetCurPos();
        if (file.FindFragment((uchar*)"<spranim_img>", 13, cur_pos))
            data_len = file.GetCurPos() - cur_pos;
        else
            data_len = file.GetFsize() - cur_pos;
        file.SetCurPos(cur_pos);

        bool packed = (type == 0x32);
        uchar* data = file.GetCurBuf();
        if (packed)
        {
            // Unpack with zlib
            uint unpacked_len = file.GetLEUInt();
            data = Compressor::Uncompress(file.GetCurBuf(), data_len, unpacked_len / data_len + 1);
            if (!data)
                throw ImageBakerException("Can't unpack SPR data", fname);
            data_len = unpacked_len;
        }
        File fm_images(data, data_len);
        if (packed)
            delete[] data;

        // Read palette
        typedef uint Palette[256];
        Palette palette[4];
        for (int i = 0; i < 4; i++)
        {
            uint palette_count = fm_images.GetLEUInt();
            if (palette_count <= 256)
                fm_images.CopyMem(&palette[i], palette_count * 4);
        }

        // Index data offsets
        UIntVec image_indices;
        image_indices.resize(frame_cnt * dir_cnt * 4);
        for (uint cur = 0; fm_images.GetCurPos() != fm_images.GetFsize();)
        {
            uchar tag = fm_images.GetUChar();
            if (tag == 1)
            {
                // Valid index
                image_indices[cur] = fm_images.GetCurPos();
                fm_images.GoForward(8 + 8 + 8 + 1);
                fm_images.GoForward(fm_images.GetLEUInt());
                cur++;
            }
            else if (tag == 0)
            {
                // Empty index
                cur++;
            }
            else
            {
                break;
            }
        }

        // Create animation
        if (anim_frames.empty())
            anim_frames.push_back(0);

        if (dir == 0)
        {
            collection.SequenceSize = (uint)anim_frames.size();
            collection.AnimTicks = 1000 / 10 * (uint)anim_frames.size();
        }

        if (dir == 1)
            collection.HaveDirs = true;

        FrameSequence& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);
        for (uint f = 0, ff = (uint)anim_frames.size(); f < ff; f++)
        {
            FrameShot& shot = sequence.Frames[f];
            uint frm = anim_frames[f];

            // Optimization, share frames
            bool founded = false;
            for (uint i = 0, j = f; i < j; i++)
            {
                if (anim_frames[i] == frm)
                {
                    shot.Shared = true;
                    shot.SharedIndex = i;
                    founded = true;
                    break;
                }
            }
            if (founded)
                continue;

            // Compute whole image size
            uint whole_w = 0, whole_h = 0;
            for (uint part = 0; part < 4; part++)
            {
                uint frm_index = (type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm :
                                                 ((frm * dir_cnt + dir_spr) << 2) + part);
                if (!image_indices[frm_index])
                    continue;
                fm_images.SetCurPos(image_indices[frm_index]);

                uint posx = fm_images.GetLEUInt();
                uint posy = fm_images.GetLEUInt();
                fm_images.GoForward(8);
                uint w = fm_images.GetLEUInt();
                uint h = fm_images.GetLEUInt();
                if (w + posx > whole_w)
                    whole_w = w + posx;
                if (h + posy > whole_h)
                    whole_h = h + posy;
            }
            if (!whole_w)
                whole_w = 1;
            if (!whole_h)
                whole_h = 1;

            // Allocate data
            UCharVec data(whole_w * whole_h * 4);

            for (uint part = 0; part < 4; part++)
            {
                uint frm_index = (type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm :
                                                 ((frm * dir_cnt + dir_spr) << 2) + part);
                if (!image_indices[frm_index])
                    continue;
                fm_images.SetCurPos(image_indices[frm_index]);

                uint posx = fm_images.GetLEUInt();
                uint posy = fm_images.GetLEUInt();

                char zar[8] = {0};
                fm_images.CopyMem(zar, 8);
                uchar subtype = zar[6];

                uint w = fm_images.GetLEUInt();
                uint h = fm_images.GetLEUInt();
                UNUSED_VARIABLE(h);
                uchar palette_present = fm_images.GetUChar();
                uint rle_size = fm_images.GetLEUInt();
                uchar* rle_buf = fm_images.GetCurBuf();
                fm_images.GoForward(rle_size);
                uchar def_color = 0;

                if (zar[5] || strcmp(zar, "<zar>") || (subtype != 0x34 && subtype != 0x35) || palette_present == 1)
                    throw ImageBakerException("Invalid SPR file innded ZAR header", fname);

                uint* ptr = (uint*)&data[0] + (posy * whole_w) + posx;
                uint x = posx, y = posy;
                while (rle_size)
                {
                    int control = *rle_buf;
                    rle_buf++;
                    rle_size--;

                    int control_mode = (control & 3);
                    int control_count = (control >> 2);

                    for (int i = 0; i < control_count; i++)
                    {
                        uint col = 0;
                        switch (control_mode)
                        {
                        case 1:
                            col = palette[part][rle_buf[i]];
                            ((uchar*)&col)[3] = 0xFF;
                            break;
                        case 2:
                            col = palette[part][rle_buf[2 * i]];
                            ((uchar*)&col)[3] = rle_buf[2 * i + 1];
                            break;
                        case 3:
                            col = palette[part][def_color];
                            ((uchar*)&col)[3] = rle_buf[i];
                            break;
                        default:
                            break;
                        }

                        for (int j = 0; j < 3; j++)
                        {
                            if (rgb_offs[part][j])
                            {
                                int val = (int)(((uchar*)&col)[2 - j]) + rgb_offs[part][j];
                                ((uchar*)&col)[2 - j] = CLAMP(val, 0, 255);
                            }
                        }

                        std::swap(((uchar*)&col)[0], ((uchar*)&col)[2]);

                        if (!part)
                            *ptr = col;
                        else if ((col >> 24) >= 128)
                            *ptr = col;
                        ptr++;

                        if (++x >= w + posx)
                        {
                            x = posx;
                            y++;
                            ptr = (uint*)&data[0] + (y * whole_w) + x;
                        }
                    }

                    if (control_mode)
                    {
                        rle_size -= control_count;
                        rle_buf += control_count;
                        if (control_mode == 2)
                        {
                            rle_size -= control_count;
                            rle_buf += control_count;
                        }
                    }
                }
            }

            shot.Width = whole_w;
            shot.Height = whole_h;
            shot.NextX = bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 0] - center_x + center_x_ex + whole_w / 2;
            shot.NextY = bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 1] - center_y + center_y_ex + whole_h;
            shot.Data = std::move(data);
        }

        if (dir_cnt == 1)
            break;
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadZar(const string& fname, const string& opt, File& file)
{
    // Read header
    char head[6];
    file.CopyMem(head, 6);
    if (head[5] || strcmp(head, "<zar>"))
        throw ImageBakerException("Invalid ZAR header", fname);

    uchar type = file.GetUChar();
    file.GoForward(1); // \0
    uint w = file.GetLEUInt();
    uint h = file.GetLEUInt();
    uchar palette_present = file.GetUChar();

    // Read palette
    uint palette[256] = {0};
    uchar def_color = 0;
    if (palette_present)
    {
        uint palette_count = file.GetLEUInt();
        if (palette_count > 256)
            throw ImageBakerException("Invalid ZAR palette count", fname);

        file.CopyMem(palette, sizeof(uint) * palette_count);
        if (type == 0x34)
            def_color = file.GetUChar();
    }

    // Read image
    uint rle_size = file.GetLEUInt();
    uchar* rle_buf = file.GetCurBuf();
    file.GoForward(rle_size);

    // Allocate data
    UCharVec data(w * h * 4);
    uint* ptr = (uint*)&data[0];

    // Decode
    while (rle_size)
    {
        int control = *rle_buf;
        rle_buf++;
        rle_size--;

        int control_mode = (control & 3);
        int control_count = (control >> 2);

        for (int i = 0; i < control_count; i++)
        {
            uint col = 0;
            switch (control_mode)
            {
            case 1:
                col = palette[rle_buf[i]];
                ((uchar*)&col)[3] = 0xFF;
                break;
            case 2:
                col = palette[rle_buf[2 * i]];
                ((uchar*)&col)[3] = rle_buf[2 * i + 1];
                break;
            case 3:
                col = palette[def_color];
                ((uchar*)&col)[3] = rle_buf[i];
                break;
            default:
                break;
            }

            *ptr++ = col;
        }

        if (control_mode)
        {
            rle_size -= control_count;
            rle_buf += control_count;
            if (control_mode == 2)
            {
                rle_size -= control_count;
                rle_buf += control_count;
            }
        }
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadTil(const string& fname, const string& opt, File& file)
{
    // Read header
    char head[7];
    file.CopyMem(head, 7);
    if (head[6] || strcmp(head, "<tile>"))
        throw ImageBakerException("Invalid TIL file header", fname);

    while (file.GetUChar())
        continue;

    file.GoForward(7 + 4); // Unknown

    uint w = file.GetLEUInt();
    UNUSED_VARIABLE(w);
    uint h = file.GetLEUInt();
    UNUSED_VARIABLE(h);

    if (!file.FindFragment((uchar*)"<tiledata>", 10, file.GetCurPos()))
        throw ImageBakerException("Tiledata in TIL file not found", fname);

    file.GoForward(10 + 3); // Signature
    uint frames_count = file.GetLEUInt();

    FrameCollection collection;
    collection.SequenceSize = frames_count;
    collection.AnimTicks = 1000 / 10 * frames_count;

    for (uint frm = 0; frm < frames_count; frm++)
    {
        // Read header
        char head[6];
        file.CopyMem(head, 6);
        if (head[5] || strcmp(head, "<zar>"))
            throw ImageBakerException("ZAR header in TIL file not found", fname);

        uchar type = file.GetUChar();
        file.GoForward(1); // \0
        uint w = file.GetLEUInt();
        uint h = file.GetLEUInt();
        uchar palette_present = file.GetUChar();

        // Read palette
        uint palette[256] = {0};
        uchar def_color = 0;
        if (palette_present)
        {
            uint palette_count = file.GetLEUInt();
            if (palette_count > 256)
                throw ImageBakerException("TIL file invalid palettes", fname);

            file.CopyMem(palette, sizeof(uint) * palette_count);
            if (type == 0x34)
                def_color = file.GetUChar();
        }

        // Read image
        uint rle_size = file.GetLEUInt();
        uchar* rle_buf = file.GetCurBuf();
        file.GoForward(rle_size);

        // Allocate data
        UCharVec data(w * h * 4);
        uint* ptr = (uint*)&data[0];

        // Decode
        while (rle_size)
        {
            int control = *rle_buf;
            rle_buf++;
            rle_size--;

            int control_mode = (control & 3);
            int control_count = (control >> 2);

            for (int i = 0; i < control_count; i++)
            {
                uint col = 0;
                switch (control_mode)
                {
                case 1:
                    col = palette[rle_buf[i]];
                    ((uchar*)&col)[3] = 0xFF;
                    break;
                case 2:
                    col = palette[rle_buf[2 * i]];
                    ((uchar*)&col)[3] = rle_buf[2 * i + 1];
                    break;
                case 3:
                    col = palette[def_color];
                    ((uchar*)&col)[3] = rle_buf[i];
                    break;
                default:
                    break;
                }

                *ptr++ = COLOR_SWAP_RB(col);
            }

            if (control_mode)
            {
                rle_size -= control_count;
                rle_buf += control_count;
                if (control_mode == 2)
                {
                    rle_size -= control_count;
                    rle_buf += control_count;
                }
            }
        }

        FrameShot& shot = collection.Main.Frames[frm];
        shot.Width = w;
        shot.Height = h;
        shot.Data = std::move(data);
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadMos(const string& fname, const string& opt, File& file)
{
    // Read signature
    char head[8];
    file.CopyMem(head, 8);
    if (!_str(head).startsWith("MOS"))
        throw ImageBakerException("Invalid MOS file header", fname);

    // Packed
    if (head[3] == 'C')
    {
        uint unpacked_len = file.GetLEUInt();
        uint data_len = file.GetFsize() - 12;
        uchar* buf = file.GetCurBuf();
        *(ushort*)buf = 0x9C78;
        uchar* data = Compressor::Uncompress(buf, data_len, unpacked_len / file.GetFsize() + 1);
        if (!data)
            throw ImageBakerException("Can't unpack MOS file", fname);

        file = File(data, data_len);
        file.CopyMem(head, 8);
        if (!_str(head).startsWith("MOS"))
            throw ImageBakerException("Invalid MOS file unpacked header", fname);
    }

    // Read header
    uint w = file.GetLEUShort(); // Width (pixels)
    uint h = file.GetLEUShort(); // Height (pixels)
    uint col = file.GetLEUShort(); // Columns (blocks)
    uint row = file.GetLEUShort(); // Rows (blocks)
    uint block_size = file.GetLEUInt(); // Block size (pixels)
    UNUSED_VARIABLE(block_size);
    uint palette_offset = file.GetLEUInt(); // Offset (from start of file) to palettes
    uint tiles_offset = palette_offset + col * row * 256 * 4;
    uint data_offset = tiles_offset + col * row * 4;

    // Allocate data
    UCharVec data(w * h * 4);
    uint* ptr = (uint*)&data[0];

    // Read image data
    uint palette[256] = {0};
    uint block = 0;
    for (uint y = 0; y < row; y++)
    {
        for (uint x = 0; x < col; x++)
        {
            // Get palette for current block
            file.SetCurPos(palette_offset + block * 256 * 4);
            file.CopyMem(palette, 256 * 4);

            // Set initial position
            file.SetCurPos(tiles_offset + block * 4);
            file.SetCurPos(data_offset + file.GetLEUInt());

            // Calculate current block size
            uint block_w = ((x == col - 1) ? w % 64 : 64);
            uint block_h = ((y == row - 1) ? h % 64 : 64);
            if (!block_w)
                block_w = 64;
            if (!block_h)
                block_h = 64;

            // Read data
            uint pos = y * 64 * w + x * 64;
            for (uint yy = 0; yy < block_h; yy++)
            {
                for (uint xx = 0; xx < block_w; xx++)
                {
                    uint color = palette[file.GetUChar()];
                    if (color == 0xFF00)
                        color = 0; // Green is transparent
                    else
                        color |= 0xFF000000;

                    *(ptr + pos) = color;
                    pos++;
                }

                pos += w - block_w;
            }

            // Go to next block
            block++;
        }
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadBam(const string& fname, const string& opt, File& file)
{
    // Format: fileName$5-6.spr
    istringstream idelim(opt);
    uint need_cycle = 0;
    int specific_frame = -1;
    idelim >> need_cycle;
    if (opt.find('-') != string::npos)
    {
        char ch;
        idelim >> ch >> specific_frame;
        if (specific_frame < 0)
            specific_frame = -1;
    }

    // Read signature
    char head[8];
    file.CopyMem(head, 8);
    if (!_str(head).startsWith("BAM"))
        throw ImageBakerException("Invalid BAM file header", fname);

    // Packed
    if (head[3] == 'C')
    {
        uint unpacked_len = file.GetLEUInt();
        uint data_len = file.GetFsize() - 12;
        uchar* buf = file.GetCurBuf();
        *(ushort*)buf = 0x9C78;
        uchar* data = Compressor::Uncompress(buf, data_len, unpacked_len / file.GetFsize() + 1);
        if (!data)
            throw ImageBakerException("Cab't unpack BAM file", fname);

        file = File(data, data_len);
        file.CopyMem(head, 8);
        if (!_str(head).startsWith("BAM"))
            throw ImageBakerException("Invalid BAM file unpacked header", fname);
    }

    // Read header
    uint frames_count = file.GetLEUShort();
    uint cycles_count = file.GetUChar();
    uchar compr_color = file.GetUChar();
    uint frames_offset = file.GetLEUInt();
    uint palette_offset = file.GetLEUInt();
    uint lookup_table_offset = file.GetLEUInt();

    // Count whole frames
    if (need_cycle >= cycles_count)
        need_cycle = 0;

    file.SetCurPos(frames_offset + frames_count * 12 + need_cycle * 4);
    uint cycle_frames = file.GetLEUShort();
    uint table_index = file.GetLEUShort();
    if (specific_frame != -1 && specific_frame >= (int)cycle_frames)
        specific_frame = 0;

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = (specific_frame == -1 ? cycle_frames : 1);
    collection.AnimTicks = 100;

    // Palette
    uint palette[256] = {0};
    file.SetCurPos(palette_offset);
    file.CopyMem(palette, 256 * 4);

    // Find in lookup table
    for (uint i = 0; i < cycle_frames; i++)
    {
        if (specific_frame != -1 && specific_frame != i)
            continue;

        // Get need index
        file.SetCurPos(lookup_table_offset + table_index * 2);
        table_index++;
        uint frame_index = file.GetLEUShort();

        // Get frame data
        file.SetCurPos(frames_offset + frame_index * 12);
        uint w = file.GetLEUShort();
        uint h = file.GetLEUShort();
        int ox = file.GetLEUShort();
        int oy = file.GetLEUShort();
        uint data_offset = file.GetLEUInt();
        bool rle = (data_offset & 0x80000000 ? false : true);
        data_offset &= 0x7FFFFFFF;

        // Allocate data
        UCharVec data(w * h * 4);
        uint* ptr = (uint*)&data[0];

        // Fill it
        file.SetCurPos(data_offset);
        for (uint k = 0, l = w * h; k < l;)
        {
            uchar index = file.GetUChar();
            uint color = palette[index];
            if (color == 0xFF00)
                color = 0;
            else
                color |= 0xFF000000;

            color = COLOR_SWAP_RB(color);

            if (rle && index == compr_color)
            {
                uint copies = file.GetUChar();
                for (uint m = 0; m <= copies; m++, k++)
                    *ptr++ = color;
            }
            else
            {
                *ptr++ = color;
                k++;
            }
        }

        // Set in animation sequence
        FrameShot& shot = collection.Main.Frames[specific_frame != -1 ? 0 : i];
        shot.Width = w;
        shot.Height = h;
        shot.NextX = -ox + w / 2;
        shot.NextY = -oy + h;
        shot.Data = std::move(data);

        if (specific_frame != -1)
            break;
    }

    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadPng(const string& fname, const string& opt, File& file)
{
    uint w, h;
    uchar* png_data = LoadPNG(file.GetBuf(), file.GetFsize(), w, h);
    if (!png_data)
        throw ImageBakerException("Can't read PNG", fname);

    UCharVec data(w * h * 4);
    memcpy(&data[0], png_data, w * h * 4);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

ImageBaker::FrameCollection ImageBaker::LoadTga(const string& fname, const string& opt, File& file)
{
    uint w, h;
    uchar* tga_data = LoadTGA(file.GetBuf(), file.GetFsize(), w, h);
    if (!tga_data)
        throw ImageBakerException("Can't read TGA", fname);

    UCharVec data(w * h * 4);
    memcpy(&data[0], tga_data, w * h * 4);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

static uchar* LoadPNG(const uchar* data, uint data_size, uint& result_width, uint& result_height)
{
    struct PNGMessage
    {
        static void Error(png_structp png_ptr, png_const_charp error_msg)
        {
            UNUSED_VARIABLE(png_ptr);
            WriteLog("PNG error '{}'.\n", error_msg);
        }
        static void Warning(png_structp png_ptr, png_const_charp error_msg)
        {
            UNUSED_VARIABLE(png_ptr);
            // WriteLog( "PNG warning '{}'.\n", error_msg );
        }
    };

    // Setup PNG reader
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return nullptr;

    png_set_error_fn(png_ptr, png_get_error_ptr(png_ptr), &PNGMessage::Error, &PNGMessage::Warning);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    struct PNGReader
    {
        static void Read(png_structp png_ptr, png_bytep png_data, png_size_t length)
        {
            uchar** io_ptr = (uchar**)png_get_io_ptr(png_ptr);
            memcpy(png_data, *io_ptr, length);
            *io_ptr += length;
        }
    };
    const uchar* io_ptr = data;
    png_set_read_fn(png_ptr, (png_voidp)&data, &PNGReader::Read);
    png_read_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    // Get information
    png_uint_32 width, height;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)&width, (png_uint_32*)&height, &bit_depth, &color_type, nullptr,
        nullptr, nullptr);

    // Settings
    png_set_strip_16(png_ptr);
    png_set_packing(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_expand(png_ptr);
    png_set_filler(png_ptr, 0x000000ff, PNG_FILLER_AFTER);
    png_read_update_info(png_ptr, info_ptr);

    // Allocate row pointers
    png_bytepp row_pointers = (png_bytepp)malloc(height * sizeof(png_bytep));
    if (!row_pointers)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    // Set the individual row_pointers to point at the correct offsets
    uchar* result = new uchar[width * height * 4];
    for (uint i = 0; i < height; i++)
        row_pointers[i] = result + i * width * 4;

    // Read image
    png_read_image(png_ptr, row_pointers);

    // Clean up
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) nullptr);
    free(row_pointers);

    // Return
    result_width = width;
    result_height = height;
    return result;
}

static uchar* LoadTGA(const uchar* data, uint data_size, uint& result_width, uint& result_height)
{
    // Reading macros
    bool read_error = false;
    uint cur_pos = 0;
#define READ_TGA(x, len) \
    if (!read_error && cur_pos + len <= data_size) \
    { \
        memcpy(x, data + cur_pos, len); \
        cur_pos += len; \
    } \
    else \
    { \
        memset(x, 0, len); \
        read_error = true; \
    }

    // Load header
    unsigned char type, pixel_depth;
    short int width, height;
    unsigned char unused_char;
    short int unused_short;
    READ_TGA(&unused_char, 1);
    READ_TGA(&unused_char, 1);
    READ_TGA(&type, 1);
    READ_TGA(&unused_short, 2);
    READ_TGA(&unused_short, 2);
    READ_TGA(&unused_char, 1);
    READ_TGA(&unused_short, 2);
    READ_TGA(&unused_short, 2);
    READ_TGA(&width, 2);
    READ_TGA(&height, 2);
    READ_TGA(&pixel_depth, 1);
    READ_TGA(&unused_char, 1);

    // Check for errors when loading the header
    if (read_error)
        return nullptr;

    // Check if the image is color indexed
    if (type == 1)
        return nullptr;

    // Check for TrueColor
    if (type != 2 && type != 10)
        return nullptr;

    // Check for RGB(A)
    if (pixel_depth != 24 && pixel_depth != 32)
        return nullptr;

    // Read
    int bpp = pixel_depth / 8;
    uint read_size = height * width * bpp;
    uchar* read_data = new uchar[read_size];
    if (type == 2)
    {
        READ_TGA(read_data, read_size);
    }
    else
    {
        uint bytes_read = 0, run_len, i, to_read;
        uchar header, color[4];
        int c;
        while (bytes_read < read_size)
        {
            READ_TGA(&header, 1);
            if (header & 0x00000080)
            {
                header &= ~0x00000080;
                READ_TGA(color, bpp);
                if (read_error)
                {
                    delete[] read_data;
                    return nullptr;
                }
                run_len = (header + 1) * bpp;
                for (i = 0; i < run_len; i += bpp)
                    for (c = 0; c < bpp && bytes_read + i + c < read_size; c++)
                        read_data[bytes_read + i + c] = color[c];
                bytes_read += run_len;
            }
            else
            {
                run_len = (header + 1) * bpp;
                if (bytes_read + run_len > read_size)
                    to_read = read_size - bytes_read;
                else
                    to_read = run_len;
                READ_TGA(read_data + bytes_read, to_read);
                if (read_error)
                {
                    delete[] read_data;
                    return nullptr;
                }
                bytes_read += run_len;
                if (bytes_read + run_len > read_size)
                    cur_pos += run_len - to_read;
            }
        }
    }
    if (read_error)
    {
        delete[] read_data;
        return nullptr;
    }

    // Copy data
    uchar* result = new uchar[width * height * 4];
    for (short y = 0; y < height; y++)
    {
        for (short x = 0; x < width; x++)
        {
            int i = (height - y - 1) * width + x;
            int j = y * width + x;
            result[i * 4 + 0] = read_data[j * bpp + 2];
            result[i * 4 + 1] = read_data[j * bpp + 1];
            result[i * 4 + 2] = read_data[j * bpp + 0];
            result[i * 4 + 3] = (bpp == 4 ? read_data[j * bpp + 3] : 0xFF);
        }
    }
    delete[] read_data;

    // Return data
    result_width = width;
    result_height = height;
    return result;
}
