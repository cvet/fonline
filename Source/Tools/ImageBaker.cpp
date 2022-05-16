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
// Copyright (c) 2006 - 2022, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

// Todo: finish with GLSL to SPIRV to GLSL/HLSL/MSL
// Todo: add supporting of APNG file format

#include "ImageBaker.h"
#include "F2Palette-Include.h"
#include "FileSystem.h"
#include "GenericUtils.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtils.h"
#include "WinApi-Include.h"

#include "minizip/zip.h"
#include "png.h"

static auto PngLoad(const uchar* data, uint& result_width, uint& result_height) -> uchar*;
static auto TgaLoad(const uchar* data, size_t data_size, uint& result_width, uint& result_height) -> uchar*;

ImageBaker::ImageBaker(GeometrySettings& settings, FileCollection& all_files) : _settings {settings}, _allFiles {all_files}
{
    // Swap palette R&B
    // Todo: swap colors of fo palette once in header
    static std::once_flag once;
    std::call_once(once, []() {
        for (uint i = 0; i < sizeof(FoPalette); i += 4) {
            std::swap(FoPalette[i], FoPalette[i + 2]);
        }
    });
}

void ImageBaker::AutoBakeImages()
{
    _errors = 0;

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

    if (_errors > 0) {
        throw ImageBakerException("Errors during images bakering", _errors);
    }
}

void ImageBaker::BakeImage(string_view fname_with_opt)
{
    if (_bakedFiles.count(string(fname_with_opt)) != 0u) {
        return;
    }

    const auto collection = LoadAny(fname_with_opt);
    BakeCollection(fname_with_opt, collection);
}

void ImageBaker::FillBakedFiles(map<string, vector<uchar>>& baked_files)
{
    for (auto&& [name, data] : _bakedFiles) {
        baked_files.emplace(name, data);
    }
}

void ImageBaker::ProcessImages(string_view target_ext, const LoadFunc& loader)
{
    _allFiles.ResetCounter();
    while (_allFiles.MoveNext()) {
        auto file_header = _allFiles.GetCurFileHeader();
        if (_bakedFiles.count(string(file_header.GetPath())) != 0u) {
            continue;
        }

        string ext = _str(file_header.GetPath()).getFileExtension();
        if (target_ext != ext) {
            continue;
        }

        auto file = _allFiles.GetCurFile();

        try {
            auto collection = loader(file_header.GetPath(), "", file);
            BakeCollection(file_header.GetPath(), collection);
        }
        catch (const ImageBakerException& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
        catch (const FileSystemExeption& ex) {
            ReportExceptionAndContinue(ex);
            _errors++;
        }
    }
}

void ImageBaker::BakeCollection(string_view fname, const FrameCollection& collection)
{
    RUNTIME_ASSERT(!_bakedFiles.count(string(fname)));

    vector<uchar> data;
    auto writer = DataWriter(data);

    const auto check_number = static_cast<ushort>(42);
    const auto dirs = static_cast<uchar>(collection.HaveDirs ? _settings.MapDirCount : 1);

    writer.Write<ushort>(check_number);
    writer.Write<ushort>(collection.SequenceSize);
    writer.Write<ushort>(collection.AnimTicks);
    writer.Write<uchar>(dirs);

    for (const auto dir : xrange(dirs)) {
        const auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        writer.Write<short>(sequence.OffsX);
        writer.Write<short>(sequence.OffsY);

        for (uint i = 0; i < collection.SequenceSize; i++) {
            const auto& shot = sequence.Frames[i];
            writer.Write<bool>(shot.Shared);
            if (!shot.Shared) {
                writer.Write<ushort>(shot.Width);
                writer.Write<ushort>(shot.Height);
                writer.Write<short>(shot.NextX);
                writer.Write<short>(shot.NextY);
                writer.WritePtr(shot.Data.data(), shot.Data.size());
                RUNTIME_ASSERT(shot.Data.size() == shot.Width * shot.Height * 4);
            }
            else {
                writer.Write<ushort>(shot.SharedIndex);
            }
        }
    }

    writer.Write<ushort>(check_number);

    if (!collection.NewExtension.empty()) {
        _bakedFiles.emplace(_str("{}.{}", _str(fname).eraseFileExtension(), collection.NewExtension), std::move(data));
    }
    else {
        _bakedFiles.emplace(fname, std::move(data));
    }
}

auto ImageBaker::LoadAny(string_view fname_with_opt) -> FrameCollection
{
    const string ext = _str(fname_with_opt).getFileExtension();
    const string dir = _str(fname_with_opt).extractDir();
    const string name = _str(fname_with_opt).extractFileName().eraseFileExtension().substringUntil('$');
    const string fname = _str("{}/{}.{}", dir, name, ext);
    const string opt = _str(fname_with_opt).extractFileName().eraseFileExtension().substringAfter('$');

    File temp_storage;
    auto find_file = [this, &fname, &ext, &temp_storage]() -> File& {
        const auto it = _cachedFiles.find(fname);
        if (it != _cachedFiles.end()) {
            return it->second;
        }

        temp_storage = _allFiles.FindFileByPath(fname);

        const auto need_cache = temp_storage && ext == "spr";
        if (need_cache) {
            return _cachedFiles.emplace(fname, std::move(temp_storage)).first->second;
        }

        return temp_storage;
    };

    auto& file = find_file();
    if (!file) {
        throw ImageBakerException("Image file not found", fname, fname_with_opt, dir, name, ext);
    }

    file.SetCurPos(0);

    if (ext == "fofrm") {
        return LoadFofrm(fname, opt, file);
    }
    if (ext == "frm") {
        return LoadFrm(fname, opt, file);
    }
    if (ext == "fr0") {
        return LoadFrX(fname, opt, file);
    }
    if (ext == "rix") {
        return LoadRix(fname, opt, file);
    }
    if (ext == "art") {
        return LoadArt(fname, opt, file);
    }
    if (ext == "spr") {
        return LoadSpr(fname, opt, file);
    }
    if (ext == "zar") {
        return LoadZar(fname, opt, file);
    }
    if (ext == "til") {
        return LoadTil(fname, opt, file);
    }
    if (ext == "mos") {
        return LoadMos(fname, opt, file);
    }
    if (ext == "bam") {
        return LoadBam(fname, opt, file);
    }
    if (ext == "png") {
        return LoadPng(fname, opt, file);
    }
    if (ext == "tga") {
        return LoadTga(fname, opt, file);
    }

    throw ImageBakerException("Invalid image file extension", fname);
}

auto ImageBaker::LoadFofrm(string_view fname, string_view opt, File& file) -> FrameCollection
{
    FrameCollection collection;

    // Load ini parser
    ConfigFile fofrm(file.GetStr(), nullptr);

    auto frm_fps = fofrm.GetInt("", "fps", 0);
    if (frm_fps <= 0) {
        frm_fps = 10;
    }

    auto frm_num = fofrm.GetInt("", "count", 0);
    if (frm_num <= 0) {
        frm_num = 1;
    }

    auto ox = fofrm.GetInt("", "offs_x", 0);
    auto oy = fofrm.GetInt("", "offs_y", 0);

    collection.EffectName = fofrm.GetStr("", "effect");

    for (const auto dir : xrange(_settings.MapDirCount)) {
        vector<tuple<FrameCollection, int, int>> sub_collections;
        sub_collections.reserve(10);

        string dir_str = _str("dir_{}", dir);
        if (!fofrm.IsApp(dir_str)) {
            dir_str = "";
        }

        if (dir_str.empty()) {
            if (dir == 1) {
                break;
            }

            if (dir > 1) {
                throw ImageBakerException("FOFRM file invalid apps", fname);
            }
        }
        else {
            ox = fofrm.GetInt(dir_str, "offs_x", ox);
            oy = fofrm.GetInt(dir_str, "offs_y", oy);
        }

        string frm_dir = _str(fname).extractDir();

        uint frames = 0;
        auto no_info = false;
        auto load_fail = false;
        for (auto frm = 0; frm < frm_num; frm++) {
            string frm_name;
            if ((frm_name = fofrm.GetStr(dir_str, _str("frm_{}", frm))).empty() && (frm != 0 || (frm_name = fofrm.GetStr(dir_str, "frm")).empty())) {
                no_info = true;
                break;
            }

            auto sub_collection = LoadAny(_str("{}/{}", frm_dir, frm_name));
            frames += sub_collection.SequenceSize;

            auto next_x = fofrm.GetInt(dir_str, _str("next_x_{}", frm), 0);
            auto next_y = fofrm.GetInt(dir_str, _str("next_y_{}", frm), 0);

            sub_collections.emplace_back(std::move(sub_collection), next_x, next_y);
        }

        // No frames found or error
        if (no_info || load_fail || sub_collections.empty() || (dir > 0 && frames != collection.SequenceSize)) {
            if (no_info && dir == 1) {
                break;
            }

            throw ImageBakerException("FOFRM file invalid data", fname);
        }

        // Allocate animation storage
        if (dir == 0u) {
            RUNTIME_ASSERT(frames <= MAX_FRAME_SEQUENCE);
            collection.SequenceSize = static_cast<ushort>(frames);
            collection.AnimTicks = static_cast<ushort>(1000 * frm_num / frm_fps);
        }
        else {
            collection.HaveDirs = true;
        }

        auto& sequence = (dir == 0 ? collection.Main : collection.Dirs[dir - 1]);
        sequence.OffsX = static_cast<short>(ox);
        sequence.OffsY = static_cast<short>(oy);

        // Merge many animations in one
        uint frm = 0;
        for (auto& sub_collection : sub_collections) {
            auto&& [sc, next_x, next_y] = sub_collection;
            for (uint j = 0; j < sc.SequenceSize; j++, frm++) {
                auto& shot = sequence.Frames[frm];
                shot.NextX = static_cast<short>(sc.Main.OffsX + next_x);
                shot.NextY = static_cast<short>(sc.Main.OffsY + next_y);
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadFrm(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    auto anim_pix = opt.find('a') != string::npos;

    file.SetCurPos(0x4);
    auto frm_fps = file.GetBEUShort();
    if (frm_fps == 0u) {
        frm_fps = 10;
    }

    file.SetCurPos(0x8);
    auto frm_num = file.GetBEUShort();
    RUNTIME_ASSERT(frm_num <= MAX_FRAME_SEQUENCE);

    FrameCollection collection;
    collection.SequenceSize = frm_num;
    collection.AnimTicks = 1000 / frm_fps * frm_num;

    for (const auto dir : xrange(_settings.MapDirCount)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];

        auto dir_frm = dir;
        if (!_settings.MapHexagonal) {
            if (dir >= 3) {
                dir_frm--;
            }
            if (dir >= 7) {
                dir_frm--;
            }
        }

        file.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = file.GetBEShort();
        file.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = file.GetBEShort();

        file.SetCurPos(0x22 + dir_frm * 4);
        auto offset = 0x3E + file.GetBEUInt();
        if (offset == 0x3E && dir_frm != 0) {
            if (dir > 1) {
                throw ImageBakerException("FRM file truncated", fname);
            }
            break;
        }

        if (dir == 1) {
            collection.HaveDirs = true;
        }

        // Make palette
        auto* palette = reinterpret_cast<uint*>(FoPalette);
        uint palette_entry[256];
        auto fm_palette = _allFiles.FindFileByPath(_str(fname).eraseFileExtension() + ".pal");
        if (fm_palette) {
            for (auto& i : palette_entry) {
                uchar r = fm_palette.GetUChar() * 4;
                uchar g = fm_palette.GetUChar() * 4;
                uchar b = fm_palette.GetUChar() * 4;
                i = COLOR_RGB(r, g, b);
            }
            palette = palette_entry;
        }

        // Animate pixels
        auto anim_pix_type = 0;
        // 0x00 - None
        // 0x01 - Slime, 229 - 232, 4
        // 0x02 - Monitors, 233 - 237, 5
        // 0x04 - FireSlow, 238 - 242, 5
        // 0x08 - FireFast, 243 - 247, 5
        // 0x10 - Shoreline, 248 - 253, 6
        // 0x20 - BlinkingRed, 254, parse on 15 frames
        const uchar blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (auto frm = 0; frm < frm_num; frm++) {
            auto& shot = sequence.Frames[frm];

            file.SetCurPos(offset);
            auto w = file.GetBEUShort();
            auto h = file.GetBEUShort();

            shot.Width = w;
            shot.Height = h;

            file.GoForward(4); // Frame size

            shot.NextX = file.GetBEShort();
            shot.NextY = file.GetBEShort();

            // Allocate data
            shot.Data.resize(w * h * 4);
            auto* ptr = reinterpret_cast<uint*>(shot.Data.data());
            file.SetCurPos(offset + 12);

            if (anim_pix_type == 0) {
                for (auto i = 0, j = w * h; i < j; i++) {
                    *(ptr + i) = palette[file.GetUChar()];
                }
            }
            else {
                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = file.GetUChar();
                    if (index >= 229 && index < 255) {
                        if (index >= 229 && index <= 232) {
                            index -= frm % 4;
                            if (index < 229) {
                                index += 4;
                            }
                        }
                        else if (index >= 233 && index <= 237) {
                            index -= frm % 5;
                            if (index < 233) {
                                index += 5;
                            }
                        }
                        else if (index >= 238 && index <= 242) {
                            index -= frm % 5;
                            if (index < 238) {
                                index += 5;
                            }
                        }
                        else if (index >= 243 && index <= 247) {
                            index -= frm % 5;
                            if (index < 243) {
                                index += 5;
                            }
                        }
                        else if (index >= 248 && index <= 253) {
                            index -= frm % 6;
                            if (index < 248) {
                                index += 6;
                            }
                        }
                        else {
                            *(ptr + i) = COLOR_RGB(blinking_red_vals[frm % 10], 0, 0);
                            continue;
                        }
                    }
                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (frm == 0 && anim_pix && palette == reinterpret_cast<uint*>(FoPalette)) {
                file.SetCurPos(offset + 12);
                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = file.GetUChar();
                    if (index < 229 || index == 255) {
                        continue;
                    }
                    if (index >= 229 && index <= 232) {
                        anim_pix_type |= 0x01;
                    }
                    else if (index >= 233 && index <= 237) {
                        anim_pix_type |= 0x02;
                    }
                    else if (index >= 238 && index <= 242) {
                        anim_pix_type |= 0x04;
                    }
                    else if (index >= 243 && index <= 247) {
                        anim_pix_type |= 0x08;
                    }
                    else if (index >= 248 && index <= 253) {
                        anim_pix_type |= 0x10;
                    }
                    else {
                        anim_pix_type |= 0x20;
                    }
                }

                if ((anim_pix_type & 0x01) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x04) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x10) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x08) != 0) {
                    collection.AnimTicks = 142;
                }
                if ((anim_pix_type & 0x02) != 0) {
                    collection.AnimTicks = 100;
                }
                if ((anim_pix_type & 0x20) != 0) {
                    collection.AnimTicks = 100;
                }

                if (anim_pix_type != 0) {
                    int divs[4] {1, 1, 1, 1};
                    if ((anim_pix_type & 0x01) != 0) {
                        divs[0] = 4;
                    }
                    if ((anim_pix_type & 0x02) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x04) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x08) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x10) != 0) {
                        divs[2] = 6;
                    }
                    if ((anim_pix_type & 0x20) != 0) {
                        divs[3] = 10;
                    }

                    frm_num = 4;
                    for (auto i = 0; i < 4; i++) {
                        if (frm_num % divs[i] == 0) {
                            continue;
                        }

                        frm_num++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_num;
                    collection.SequenceSize = frm_num;
                    RUNTIME_ASSERT(frm_num <= MAX_FRAME_SEQUENCE);
                }
            }

            if (anim_pix_type == 0) {
                offset += w * h + 12;
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadFrX(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    auto anim_pix = opt.find('a') != string::npos;

    // Load from frm
    file.SetCurPos(0x4);
    auto frm_fps = file.GetBEUShort();
    if (frm_fps == 0u) {
        frm_fps = 10;
    }

    file.SetCurPos(0x8);
    auto frm_num = file.GetBEUShort();
    RUNTIME_ASSERT(frm_num <= MAX_FRAME_SEQUENCE);

    FrameCollection collection;
    collection.SequenceSize = frm_num;
    collection.AnimTicks = 1000 / frm_fps * frm_num;
    collection.NewExtension = "frm";

    for (const auto dir : xrange(_settings.MapDirCount)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];

        auto dir_frm = dir;
        if (!_settings.MapHexagonal) {
            if (dir >= 3) {
                dir_frm--;
            }
            if (dir >= 7) {
                dir_frm--;
            }
        }

        if (dir_frm != 0) {
            string next_fname = _str("{}{}", fname.substr(0, fname.size() - 1), '0' + dir_frm);
            file = _allFiles.FindFileByPath(next_fname);
            if (!file) {
                if (dir > 1) {
                    throw ImageBakerException("FRX file not found", next_fname);
                }
                break;
            }
        }

        file.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = file.GetBEShort();
        file.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = file.GetBEShort();

        file.SetCurPos(0x22 + dir_frm * 4);
        auto offset = 0x3E + file.GetBEUInt();

        if (dir == 1) {
            collection.HaveDirs = true;
        }

        // Make palette
        auto* palette = reinterpret_cast<uint*>(FoPalette);
        uint palette_entry[256];
        auto fm_palette = _allFiles.FindFileByPath(_str(fname).eraseFileExtension() + ".pal");
        if (fm_palette) {
            for (auto& i : palette_entry) {
                uchar r = fm_palette.GetUChar() * 4;
                uchar g = fm_palette.GetUChar() * 4;
                uchar b = fm_palette.GetUChar() * 4;
                i = COLOR_RGB(r, g, b);
            }
            palette = palette_entry;
        }

        // Animate pixels
        auto anim_pix_type = 0;
        // 0x00 - None
        // 0x01 - Slime, 229 - 232, 4
        // 0x02 - Monitors, 233 - 237, 5
        // 0x04 - FireSlow, 238 - 242, 5
        // 0x08 - FireFast, 243 - 247, 5
        // 0x10 - Shoreline, 248 - 253, 6
        // 0x20 - BlinkingRed, 254, parse on 15 frames
        const uchar blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

        for (auto frm = 0; frm < frm_num; frm++) {
            auto& shot = sequence.Frames[frm];

            file.SetCurPos(offset);
            auto w = file.GetBEUShort();
            auto h = file.GetBEUShort();

            shot.Width = w;
            shot.Height = h;

            file.GoForward(4); // Frame size

            shot.NextX = file.GetBEShort();
            shot.NextY = file.GetBEShort();

            // Allocate data
            shot.Data.resize(w * h * 4);
            auto* ptr = reinterpret_cast<uint*>(shot.Data.data());
            file.SetCurPos(offset + 12);

            if (anim_pix_type == 0) {
                for (auto i = 0, j = w * h; i < j; i++) {
                    *(ptr + i) = palette[file.GetUChar()];
                }
            }
            else {
                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = file.GetUChar();
                    if (index >= 229 && index < 255) {
                        if (index >= 229 && index <= 232) {
                            index -= frm % 4;
                            if (index < 229) {
                                index += 4;
                            }
                        }
                        else if (index >= 233 && index <= 237) {
                            index -= frm % 5;
                            if (index < 233) {
                                index += 5;
                            }
                        }
                        else if (index >= 238 && index <= 242) {
                            index -= frm % 5;
                            if (index < 238) {
                                index += 5;
                            }
                        }
                        else if (index >= 243 && index <= 247) {
                            index -= frm % 5;
                            if (index < 243) {
                                index += 5;
                            }
                        }
                        else if (index >= 248 && index <= 253) {
                            index -= frm % 6;
                            if (index < 248) {
                                index += 6;
                            }
                        }
                        else {
                            *(ptr + i) = COLOR_RGB(blinking_red_vals[frm % 10], 0, 0);
                            continue;
                        }
                    }
                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (frm == 0 && anim_pix && palette == reinterpret_cast<uint*>(FoPalette)) {
                file.SetCurPos(offset + 12);
                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = file.GetUChar();
                    if (index < 229 || index == 255) {
                        continue;
                    }
                    if (index >= 229 && index <= 232) {
                        anim_pix_type |= 0x01;
                    }
                    else if (index >= 233 && index <= 237) {
                        anim_pix_type |= 0x02;
                    }
                    else if (index >= 238 && index <= 242) {
                        anim_pix_type |= 0x04;
                    }
                    else if (index >= 243 && index <= 247) {
                        anim_pix_type |= 0x08;
                    }
                    else if (index >= 248 && index <= 253) {
                        anim_pix_type |= 0x10;
                    }
                    else {
                        anim_pix_type |= 0x20;
                    }
                }

                if ((anim_pix_type & 0x01) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x04) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x10) != 0) {
                    collection.AnimTicks = 200;
                }
                if ((anim_pix_type & 0x08) != 0) {
                    collection.AnimTicks = 142;
                }
                if ((anim_pix_type & 0x02) != 0) {
                    collection.AnimTicks = 100;
                }
                if ((anim_pix_type & 0x20) != 0) {
                    collection.AnimTicks = 100;
                }

                if (anim_pix_type != 0) {
                    int divs[4];
                    divs[0] = 1;
                    divs[1] = 1;
                    divs[2] = 1;
                    divs[3] = 1;
                    if ((anim_pix_type & 0x01) != 0) {
                        divs[0] = 4;
                    }
                    if ((anim_pix_type & 0x02) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x04) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x08) != 0) {
                        divs[1] = 5;
                    }
                    if ((anim_pix_type & 0x10) != 0) {
                        divs[2] = 6;
                    }
                    if ((anim_pix_type & 0x20) != 0) {
                        divs[3] = 10;
                    }

                    frm_num = 4;
                    for (auto i = 0; i < 4; i++) {
                        if (frm_num % divs[i] == 0) {
                            continue;
                        }
                        frm_num++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_num;
                    collection.SequenceSize = frm_num;
                    RUNTIME_ASSERT(frm_num <= MAX_FRAME_SEQUENCE);
                }
            }

            if (anim_pix_type == 0) {
                offset += w * h + 12;
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadRix(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();
    UNUSED_VARIABLE(fname);

    file.SetCurPos(0x4);
    ushort w = 0;
    file.CopyData(&w, 2);
    ushort h = 0;
    file.CopyData(&h, 2);

    file.SetCurPos(0xA);
    const auto* palette = file.GetCurBuf();

    vector<uchar> data(w * h * 4);
    auto* ptr = reinterpret_cast<uint*>(data.data());
    file.SetCurPos(0xA + 256 * 3);

    for (auto i = 0, j = w * h; i < j; i++) {
        const auto index = static_cast<uint>(file.GetUChar()) * 3;
        const uint r = *(palette + index + 2) * 4;
        const uint g = *(palette + index + 1) * 4;
        const uint b = *(palette + index + 0) * 4;
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

auto ImageBaker::LoadArt(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    auto palette_index = 0; // 0..3
    auto transparent = false;
    auto mirror_hor = false;
    auto mirror_ver = false;
    uint frm_from = 0;
    uint frm_to = 100000;

    for (size_t i = 0; i < opt.length(); i++) {
        switch (opt[i]) {
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
            auto f = string(opt.substr(i));
            istringstream idelim(f);
            char ch = 0;
            if (f.find('-') != string::npos) {
                idelim >> frm_from >> ch >> frm_to;
                i += 3;
            }
            else {
                idelim >> frm_from;
                frm_to = frm_from;
                i += 1;
            }
        } break;
        default:
            break;
        }
    }

    struct ArtHeader
    {
        unsigned int Flags {};
        // 0x00000001 = Static - no rotation, contains frames only for south direction.
        // 0x00000002 = Critter - uses delta attribute, while playing walking animation.
        // 0x00000004 = Font - X offset is equal to number of pixels to advance horizontally for next
        // character. 0x00000008 = Facade - requires fackwalk file. 0x00000010 = Unknown - used in eye candy,
        // for example, DIVINATION.art.
        unsigned int FrameRate {};
        unsigned int RotationCount {};
        unsigned int PaletteList[4] {};
        unsigned int ActionFrame {};
        unsigned int FrameCount {};
        unsigned int InfoList[8] {};
        unsigned int SizeList[8] {};
        unsigned int DataList[8] {};
    } header;

    struct ArtFrameInfo
    {
        unsigned int FrameWidth {};
        unsigned int FrameHeight {};
        unsigned int FrameSize {};
        signed int OffsetX {};
        signed int OffsetY {};
        signed int DeltaX {};
        signed int DeltaY {};
    } frame_info;

    using ArtPalette = unsigned int[256];
    ArtPalette palette[4];

    file.CopyData(&header, sizeof(header));
    if ((header.Flags & 0x00000001) != 0u) {
        header.RotationCount = 1;
    }

    // Load palettes
    auto palette_count = 0;
    for (auto i = 0; i < 4; i++) {
        if (header.PaletteList[i] != 0u) {
            file.CopyData(&palette[i], sizeof(ArtPalette));
            palette_count++;
        }
    }
    if (palette_index >= palette_count) {
        palette_index = 0;
    }

    auto frm_fps = header.FrameRate;
    if (frm_fps == 0u) {
        frm_fps = 10;
    }

    auto frm_count = header.FrameCount;

    if (frm_from >= frm_count) {
        frm_from = frm_count - 1;
    }
    if (frm_to >= frm_count) {
        frm_to = frm_count - 1;
    }
    auto frm_count_anim = std::max(frm_from, frm_to) - std::min(frm_from, frm_to) + 1;
    RUNTIME_ASSERT(frm_count_anim <= MAX_FRAME_SEQUENCE);

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = static_cast<ushort>(frm_count_anim);
    collection.AnimTicks = static_cast<ushort>(1000u / frm_fps * frm_count_anim);
    collection.HaveDirs = (header.RotationCount == 8);

    for (const auto dir : xrange(_settings.MapDirCount)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];

        auto dir_art = dir;
        if (_settings.MapHexagonal) {
            switch (dir_art) {
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
        else {
            dir_art = (dir + 1) % 8;
        }

        // Read data
        auto frm_read = frm_from;
        uint frm_write = 0;
        while (true) {
            file.SetCurPos(sizeof(ArtHeader) + sizeof(ArtPalette) * palette_count + sizeof(ArtFrameInfo) * dir_art * frm_count + sizeof(ArtFrameInfo) * frm_read);

            file.CopyData(&frame_info, sizeof(frame_info));

            auto w = frame_info.FrameWidth;
            auto h = frame_info.FrameHeight;
            vector<uchar> data(w * h * 4);
            auto* ptr = reinterpret_cast<uint*>(data.data());

            auto& shot = sequence.Frames[frm_write];
            shot.Width = static_cast<ushort>(w);
            shot.Height = static_cast<ushort>(h);
            shot.NextX = static_cast<short>((-frame_info.OffsetX + frame_info.FrameWidth / 2) * (mirror_hor ? -1 : 1));
            shot.NextY = static_cast<short>((-frame_info.OffsetY + frame_info.FrameHeight) * (mirror_ver ? -1 : 1));

            auto color = 0u;
            auto art_get_color = [&color, &file, &palette, &palette_index, &transparent]() {
                const auto index = file.GetUChar();
                color = palette[palette_index][index];
                std::swap(reinterpret_cast<uchar*>(&color)[0], reinterpret_cast<uchar*>(&color)[2]);
                if (index == 0u) {
                    color = 0u;
                }
                else if (transparent) {
                    color |= std::max((color >> 16) & 0xFFu, std::max((color >> 8) & 0xFFu, color & 0xFFu)) << 24u;
                }
                else {
                    color |= 0xFF000000u;
                }
            };

            auto pos = 0u;
            auto x = 0u;
            auto y = 0u;
            auto mirror = mirror_hor || mirror_ver;
            auto art_write_color = [&color, &pos, &x, &y, &w, &h, ptr, &mirror, &mirror_hor, &mirror_ver]() {
                if (mirror) {
                    *(ptr + ((mirror_ver ? h - y - 1 : y) * w + (mirror_hor ? w - x - 1 : x))) = color;
                    x++;
                    if (x >= w) {
                        x = 0;
                        y++;
                    }
                }
                else {
                    *(ptr + pos) = color;
                    pos++;
                }
            };

            if (w * h == frame_info.FrameSize) {
                for (uint i = 0; i < frame_info.FrameSize; i++) {
                    art_get_color();
                    art_write_color();
                }
            }
            else {
                for (uint i = 0; i < frame_info.FrameSize; i++) {
                    auto cmd = file.GetUChar();
                    if (cmd > 128) {
                        cmd -= 128;
                        i += cmd;
                        for (; cmd > 0; cmd--) {
                            art_get_color();
                            art_write_color();
                        }
                    }
                    else {
                        art_get_color();
                        for (; cmd > 0; cmd--) {
                            art_write_color();
                        }
                        i++;
                    }
                }
            }

            shot.Data = std::move(data);

            if (frm_read == frm_to) {
                break;
            }

            if (frm_to > frm_from) {
                frm_read++;
            }
            else {
                frm_read--;
            }

            frm_write++;
        }

        if (header.RotationCount != 8) {
            break;
        }
    }

    return collection;
}

auto ImageBaker::LoadSpr(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    FrameCollection collection;

    for (const auto dir : xrange(_settings.MapDirCount)) {
        auto dir_spr = dir;
        if (_settings.MapHexagonal) {
            switch (dir_spr) {
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
        else {
            dir_spr = (dir + 2) % 8;
        }

        // Color offsets
        // 0 - other
        // 1 - skin
        // 2 - hair
        // 3 - armor
        int rgb_offs[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

        // Format: fileName$[1,100,0,0][2,0,0,100]animName.spr
        auto first = opt.find_first_of('[');
        auto last = opt.find_last_of(']', first);
        auto seq_name = last != string::npos ? opt.substr(last + 1) : opt;

        while (first != string::npos && last != string::npos) {
            last = opt.find_first_of(']', first);
            auto entry = string(opt.substr(first, last - first - 1));
            istringstream ientry(entry);

            // Parse numbers
            int rgb[4];
            if (ientry >> rgb[0] >> rgb[1] >> rgb[2] >> rgb[3]) {
                // To one part
                if (rgb[0] >= 0 && rgb[0] <= 3) {
                    rgb_offs[rgb[0]][0] = rgb[1]; // R
                    rgb_offs[rgb[0]][1] = rgb[2]; // G
                    rgb_offs[rgb[0]][2] = rgb[3]; // B
                }
                // To all parts
                else {
                    for (auto& rgb_off : rgb_offs) {
                        rgb_off[0] = rgb[1]; // R
                        rgb_off[1] = rgb[2]; // G
                        rgb_off[2] = rgb[3]; // B
                    }
                }
            }

            first = opt.find_first_of('[', last);
        }

        // Read header
        char head[11];
        file.CopyData(head, 11);
        if (head[8] != 0 || strcmp(head, "<sprite>") != 0) {
            throw ImageBakerException("Invalid SPR header", fname);
        }

        auto dimension_left = static_cast<float>(file.GetUChar()) * 6.7f;
        auto dimension_up = static_cast<float>(file.GetUChar()) * 7.6f;
        UNUSED_VARIABLE(dimension_up);
        auto dimension_right = static_cast<float>(file.GetUChar()) * 6.7f;
        int center_x = file.GetLEUInt();
        int center_y = file.GetLEUInt();
        file.GoForward(2); // ushort unknown1  sometimes it is 0, and sometimes it is 3
        file.GoForward(1); // CHAR unknown2  0x64, other values were not observed

        auto ta = (127.0f / 2.0f) * 3.141592654f / 180.0f; // Tactics grid angle
        auto center_x_ex = static_cast<int>((dimension_left * sinf(ta) + dimension_right * sinf(ta)) / 2.0f - dimension_left * sinf(ta));
        auto center_y_ex = static_cast<int>((dimension_left * cosf(ta) + dimension_right * cosf(ta)) / 2.0f);

        uint anim_index = 0;
        vector<uint> anim_frames;
        anim_frames.reserve(1000);

        // Find sequence
        auto seq_founded = false;
        auto seq_cnt = file.GetLEUInt();
        for (uint seq = 0; seq < seq_cnt; seq++) {
            // Find by name
            auto item_cnt = file.GetLEUInt();
            file.GoForward(sizeof(short) * item_cnt);
            file.GoForward(sizeof(uint) * item_cnt); // uint  unknown3[item_cnt]

            auto name_len = file.GetLEUInt();
            auto name = string(reinterpret_cast<const char*>(file.GetCurBuf()), name_len);
            file.GoForward(name_len);
            auto index = file.GetLEUShort();

            if (seq_name.empty() || _str(seq_name).compareIgnoreCase(name)) {
                anim_index = index;

                // Read frame numbers
                file.GoBack(sizeof(ushort) + name_len + sizeof(uint) + sizeof(uint) * item_cnt + sizeof(short) * item_cnt);

                for (uint i = 0; i < item_cnt; i++) {
                    short val = file.GetLEShort();
                    if (val >= 0) {
                        anim_frames.push_back(val);
                    }
                    else if (val == -43) {
                        file.GoForward(sizeof(short) * 3);
                        i += 3;
                    }
                }

                // Animation founded, go forward
                seq_founded = true;
                break;
            }
        }
        if (!seq_founded) {
            throw ImageBakerException("Sequence for SPR not found", fname);
        }

        // Find animation
        file.SetCurPos(0);
        for (uint i = 0; i <= anim_index; i++) {
            if (!file.FindFragment("<spranim>")) {
                throw ImageBakerException("Spranim for SPR not found", fname);
            }
            file.GoForward(12);
        }

        auto file_offset = file.GetLEUInt();
        file.GoForward(file.GetLEUInt()); // Collection name
        auto frame_cnt = file.GetLEUInt();
        auto dir_cnt = file.GetLEUInt();
        vector<uint> bboxes;
        bboxes.resize(frame_cnt * dir_cnt * 4);
        file.CopyData(bboxes.data(), sizeof(uint) * frame_cnt * dir_cnt * 4);

        // Fix dir
        if (dir_cnt != 8) {
            dir_spr = dir_spr * (dir_cnt * 100 / 8) / 100;
        }
        if (static_cast<uint>(dir_spr) >= dir_cnt) {
            dir_spr = 0;
        }

        // Check wrong frames
        for (uint i = 0; i < anim_frames.size();) {
            if (anim_frames[i] >= frame_cnt) {
                anim_frames.erase(anim_frames.begin() + i);
            }
            else {
                i++;
            }
        }

        // Get images file
        file.SetCurPos(file_offset);
        file.GoForward(14); // <spranim_img>\0
        auto type = file.GetUChar();
        file.GoForward(1); // \0

        size_t data_len = 0u;
        auto cur_pos = file.GetCurPos();
        if (file.FindFragment("<spranim_img>")) {
            data_len = file.GetCurPos() - cur_pos;
        }
        else {
            data_len = file.GetSize() - cur_pos;
        }
        file.SetCurPos(cur_pos);

        auto packed = type == 0x32;
        vector<uchar> data;
        if (packed) {
            // Unpack with zlib
            auto unpacked_len = file.GetLEUInt();
            auto unpacked_data = Compressor::Uncompress({file.GetCurBuf(), data_len}, unpacked_len / data_len + 1);
            if (unpacked_data.empty()) {
                throw ImageBakerException("Can't unpack SPR data", fname);
            }

            data = unpacked_data;
        }
        else {
            data.resize(data_len);
            std::memcpy(data.data(), file.GetCurBuf(), data_len);
        }

        File fm_images(data);

        // Read palette
        typedef uint Palette[256];
        Palette palette[4];
        for (auto& i : palette) {
            auto palette_count = fm_images.GetLEUInt();
            if (palette_count <= 256) {
                fm_images.CopyData(&i, palette_count * 4);
            }
        }

        // Index data offsets
        vector<size_t> image_indices;
        image_indices.resize(frame_cnt * dir_cnt * 4);
        for (uint cur = 0; fm_images.GetCurPos() != fm_images.GetSize();) {
            auto tag = fm_images.GetUChar();
            if (tag == 1) {
                // Valid index
                image_indices[cur] = fm_images.GetCurPos();
                fm_images.GoForward(8 + 8 + 8 + 1);
                fm_images.GoForward(fm_images.GetLEUInt());
                cur++;
            }
            else if (tag == 0) {
                // Empty index
                cur++;
            }
            else {
                break;
            }
        }

        // Create animation
        if (anim_frames.empty()) {
            anim_frames.push_back(0);
        }

        if (dir == 0) {
            RUNTIME_ASSERT(anim_frames.size() <= MAX_FRAME_SEQUENCE);
            collection.SequenceSize = static_cast<ushort>(anim_frames.size());
            collection.AnimTicks = static_cast<ushort>(1000u / 10u * anim_frames.size());
        }

        if (dir == 1u) {
            collection.HaveDirs = true;
        }

        auto& sequence = (dir == 0u ? collection.Main : collection.Dirs[dir - 1u]);
        for (uint f = 0, ff = static_cast<uint>(anim_frames.size()); f < ff; f++) {
            auto& shot = sequence.Frames[f];
            auto frm = anim_frames[f];

            // Optimization, share frames
            auto founded = false;
            for (uint i = 0, j = f; i < j; i++) {
                if (anim_frames[i] == frm) {
                    shot.Shared = true;
                    shot.SharedIndex = static_cast<ushort>(i);
                    founded = true;
                    break;
                }
            }
            if (founded) {
                continue;
            }

            // Compute whole image size
            uint whole_w = 0;
            uint whole_h = 0;
            for (uint part = 0; part < 4; part++) {
                auto frm_index = type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : (frm * dir_cnt + (dir_spr << 2)) + part;
                if (image_indices[frm_index] == 0u) {
                    continue;
                }
                fm_images.SetCurPos(image_indices[frm_index]);

                auto posx = fm_images.GetLEUInt();
                auto posy = fm_images.GetLEUInt();
                fm_images.GoForward(8);
                auto w = fm_images.GetLEUInt();
                auto h = fm_images.GetLEUInt();
                if (w + posx > whole_w) {
                    whole_w = w + posx;
                }
                if (h + posy > whole_h) {
                    whole_h = h + posy;
                }
            }
            if (whole_w == 0u) {
                whole_w = 1;
            }
            if (whole_h == 0u) {
                whole_h = 1;
            }

            // Allocate data
            vector<uchar> whole_data(whole_w * whole_h * 4);

            for (uint part = 0; part < 4; part++) {
                auto frm_index = type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : (frm * dir_cnt + (dir_spr << 2)) + part;
                if (image_indices[frm_index] == 0u) {
                    continue;
                }
                fm_images.SetCurPos(image_indices[frm_index]);

                auto posx = fm_images.GetLEUInt();
                auto posy = fm_images.GetLEUInt();

                char zar[8] = {0};
                fm_images.CopyData(zar, 8);
                uchar subtype = zar[6];

                auto w = fm_images.GetLEUInt();
                auto h = fm_images.GetLEUInt();
                UNUSED_VARIABLE(h);
                auto palette_present = fm_images.GetUChar();
                auto rle_size = fm_images.GetLEUInt();
                const auto* rle_buf = fm_images.GetCurBuf();
                fm_images.GoForward(rle_size);
                uchar def_color = 0;

                if (zar[5] != 0 || strcmp(zar, "<zar>") != 0 || (subtype != 0x34 && subtype != 0x35) || palette_present == 1) {
                    throw ImageBakerException("Invalid SPR file innded ZAR header", fname);
                }

                auto* ptr = reinterpret_cast<uint*>(whole_data.data()) + posy * whole_w + posx;
                auto x = posx;
                auto y = posy;
                while (rle_size != 0u) {
                    int control = *rle_buf;
                    rle_buf++;
                    rle_size--;

                    auto control_mode = control & 3;
                    auto control_count = control >> 2;

                    for (auto i = 0; i < control_count; i++) {
                        uint col = 0;
                        switch (control_mode) {
                        case 1:
                            col = palette[part][rle_buf[i]];
                            reinterpret_cast<uchar*>(&col)[3] = 0xFF;
                            break;
                        case 2:
                            col = palette[part][rle_buf[2 * i]];
                            reinterpret_cast<uchar*>(&col)[3] = rle_buf[2 * i + 1];
                            break;
                        case 3:
                            col = palette[part][def_color];
                            reinterpret_cast<uchar*>(&col)[3] = rle_buf[i];
                            break;
                        default:
                            break;
                        }

                        for (auto j = 0; j < 3; j++) {
                            if (rgb_offs[part][j] != 0) {
                                auto val = static_cast<int>(reinterpret_cast<uchar*>(&col)[2 - j]) + rgb_offs[part][j];
                                reinterpret_cast<uchar*>(&col)[2 - j] = static_cast<uchar>(std::clamp(val, 0, 255));
                            }
                        }

                        std::swap(reinterpret_cast<uchar*>(&col)[0], reinterpret_cast<uchar*>(&col)[2]);

                        if (part == 0u) {
                            *ptr = col;
                        }
                        else if (col >> 24 >= 128) {
                            *ptr = col;
                        }
                        ptr++;

                        if (++x >= w + posx) {
                            x = posx;
                            y++;
                            ptr = reinterpret_cast<uint*>(whole_data.data()) + y * whole_w + x;
                        }
                    }

                    if (control_mode != 0) {
                        rle_size -= control_count;
                        rle_buf += control_count;
                        if (control_mode == 2) {
                            rle_size -= control_count;
                            rle_buf += control_count;
                        }
                    }
                }
            }

            shot.Width = static_cast<ushort>(whole_w);
            shot.Height = static_cast<ushort>(whole_h);
            shot.NextX = static_cast<short>(bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 0] - center_x + center_x_ex + whole_w / 2);
            shot.NextY = static_cast<short>(bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 1] - center_y + center_y_ex + whole_h);
            shot.Data = std::move(whole_data);
        }

        if (dir_cnt == 1) {
            break;
        }
    }

    return collection;
}

auto ImageBaker::LoadZar(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    // Read header
    char head[6];
    file.CopyData(head, 6);
    if (head[5] != 0 || strcmp(head, "<zar>") != 0) {
        throw ImageBakerException("Invalid ZAR header", fname);
    }

    const auto type = file.GetUChar();
    file.GoForward(1); // \0
    const auto w = file.GetLEUInt();
    const auto h = file.GetLEUInt();
    const auto palette_present = file.GetUChar();

    // Read palette
    uint palette[256] = {0};
    uchar def_color = 0;
    if (palette_present != 0u) {
        const auto palette_count = file.GetLEUInt();
        if (palette_count > 256) {
            throw ImageBakerException("Invalid ZAR palette count", fname);
        }

        file.CopyData(palette, sizeof(uint) * palette_count);
        if (type == 0x34) {
            def_color = file.GetUChar();
        }
    }

    // Read image
    auto rle_size = file.GetLEUInt();
    const auto* rle_buf = file.GetCurBuf();
    file.GoForward(rle_size);

    // Allocate data
    vector<uchar> data(w * h * 4);
    auto* ptr = reinterpret_cast<uint*>(data.data());

    // Decode
    while (rle_size != 0u) {
        const int control = *rle_buf;
        rle_buf++;
        rle_size--;

        const auto control_mode = control & 3;
        const auto control_count = control >> 2;

        for (auto i = 0; i < control_count; i++) {
            uint col = 0;
            switch (control_mode) {
            case 1:
                col = palette[rle_buf[i]];
                reinterpret_cast<uchar*>(&col)[3] = 0xFF;
                break;
            case 2:
                col = palette[rle_buf[2 * i]];
                reinterpret_cast<uchar*>(&col)[3] = rle_buf[2 * i + 1];
                break;
            case 3:
                col = palette[def_color];
                reinterpret_cast<uchar*>(&col)[3] = rle_buf[i];
                break;
            default:
                break;
            }

            *ptr++ = col;
        }

        if (control_mode != 0) {
            rle_size -= control_count;
            rle_buf += control_count;
            if (control_mode == 2) {
                rle_size -= control_count;
                rle_buf += control_count;
            }
        }
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = static_cast<ushort>(w);
    collection.Main.Frames[0].Height = static_cast<ushort>(h);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadTil(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    // Read header
    char head[7];
    file.CopyData(head, 7);
    if (head[6] != 0 || strcmp(head, "<tile>") != 0) {
        throw ImageBakerException("Invalid TIL file header", fname);
    }

    while (file.GetUChar() != 0u) {
        // Nop
    }

    file.GoForward(7 + 4); // Unknown

    const auto w = file.GetLEUInt();
    UNUSED_VARIABLE(w);
    const auto h = file.GetLEUInt();
    UNUSED_VARIABLE(h);

    if (!file.FindFragment("<tiledata>")) {
        throw ImageBakerException("Tiledata in TIL file not found", fname);
    }

    file.GoForward(10 + 3); // Signature
    const auto frames_count = file.GetLEUInt();
    RUNTIME_ASSERT(frames_count <= MAX_FRAME_SEQUENCE);

    FrameCollection collection;
    collection.SequenceSize = static_cast<ushort>(frames_count);
    collection.AnimTicks = static_cast<ushort>(1000u / 10u * frames_count);

    for (uint frm = 0; frm < frames_count; frm++) {
        // Read header
        char zar_head[6];
        file.CopyData(zar_head, 6);
        if (zar_head[5] != 0 || strcmp(zar_head, "<zar>") != 0) {
            throw ImageBakerException("ZAR header in TIL file not found", fname);
        }

        const auto type = file.GetUChar();
        file.GoForward(1); // \0
        const auto zar_w = file.GetLEUInt();
        const auto zar_h = file.GetLEUInt();
        const auto palette_present = file.GetUChar();

        // Read palette
        uint palette[256] = {0};
        uchar def_color = 0;
        if (palette_present != 0u) {
            const auto palette_count = file.GetLEUInt();
            if (palette_count > 256) {
                throw ImageBakerException("TIL file invalid palettes", fname);
            }

            file.CopyData(palette, sizeof(uint) * palette_count);
            if (type == 0x34) {
                def_color = file.GetUChar();
            }
        }

        // Read image
        auto rle_size = file.GetLEUInt();
        const auto* rle_buf = file.GetCurBuf();
        file.GoForward(rle_size);

        // Allocate data
        vector<uchar> data(zar_w * zar_h * 4);
        auto* ptr = reinterpret_cast<uint*>(data.data());

        // Decode
        while (rle_size != 0u) {
            const int control = *rle_buf;
            rle_buf++;
            rle_size--;

            const auto control_mode = control & 3;
            const auto control_count = control >> 2;

            for (auto i = 0; i < control_count; i++) {
                uint col = 0;
                switch (control_mode) {
                case 1:
                    col = palette[rle_buf[i]];
                    reinterpret_cast<uchar*>(&col)[3] = 0xFF;
                    break;
                case 2:
                    col = palette[rle_buf[2 * i]];
                    reinterpret_cast<uchar*>(&col)[3] = rle_buf[2 * i + 1];
                    break;
                case 3:
                    col = palette[def_color];
                    reinterpret_cast<uchar*>(&col)[3] = rle_buf[i];
                    break;
                default:
                    break;
                }

                *ptr++ = COLOR_SWAP_RB(col);
            }

            if (control_mode != 0) {
                rle_size -= control_count;
                rle_buf += control_count;
                if (control_mode == 2) {
                    rle_size -= control_count;
                    rle_buf += control_count;
                }
            }
        }

        auto& shot = collection.Main.Frames[frm];
        shot.Width = static_cast<ushort>(zar_w);
        shot.Height = static_cast<ushort>(zar_h);
        shot.Data = std::move(data);
    }

    return collection;
}

auto ImageBaker::LoadMos(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    // Read signature
    char head[8];
    file.CopyData(head, 8);
    if (!_str(head).startsWith("MOS")) {
        throw ImageBakerException("Invalid MOS file header", fname);
    }

    // Packed
    if (head[3] == 'C') {
        const auto unpacked_len = file.GetLEUInt();
        auto data_len = file.GetSize() - 12;

        auto* buf = const_cast<uchar*>(file.GetBuf());
        *reinterpret_cast<ushort*>(buf) = 0x9C78;

        const auto data = Compressor::Uncompress({buf, data_len}, unpacked_len / file.GetSize() + 1);

        if (data.empty()) {
            throw ImageBakerException("Can't unpack MOS file", fname);
        }

        file = File(data);
        file.CopyData(head, 8);
        if (!_str(head).startsWith("MOS")) {
            throw ImageBakerException("Invalid MOS file unpacked header", fname);
        }
    }

    // Read header
    const uint w = file.GetLEUShort(); // Width (pixels)
    const uint h = file.GetLEUShort(); // Height (pixels)
    const uint col = file.GetLEUShort(); // Columns (blocks)
    const uint row = file.GetLEUShort(); // Rows (blocks)
    auto block_size = file.GetLEUInt(); // Block size (pixels)
    UNUSED_VARIABLE(block_size);
    const auto palette_offset = file.GetLEUInt(); // Offset (from start of file) to palettes
    const auto tiles_offset = palette_offset + col * row * 256 * 4;
    const auto data_offset = tiles_offset + col * row * 4;

    // Allocate data
    vector<uchar> data(w * h * 4);
    auto* ptr = reinterpret_cast<uint*>(data.data());

    // Read image data
    uint palette[256] = {0};
    uint block = 0;
    for (uint y = 0; y < row; y++) {
        for (uint x = 0; x < col; x++) {
            // Get palette for current block
            file.SetCurPos(palette_offset + block * 256 * 4);
            file.CopyData(palette, 256 * 4);

            // Set initial position
            file.SetCurPos(tiles_offset + block * 4);
            file.SetCurPos(data_offset + file.GetLEUInt());

            // Calculate current block size
            auto block_w = x == col - 1 ? w % 64 : 64;
            auto block_h = y == row - 1 ? h % 64 : 64;
            if (block_w == 0u) {
                block_w = 64;
            }
            if (block_h == 0u) {
                block_h = 64;
            }

            // Read data
            auto pos = y * 64 * w + x * 64;
            for (uint yy = 0; yy < block_h; yy++) {
                for (uint xx = 0; xx < block_w; xx++) {
                    auto color = palette[file.GetUChar()];
                    if (color == 0xFF00) {
                        color = 0; // Green is transparent
                    }
                    else {
                        color |= 0xFF000000;
                    }

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
    collection.Main.Frames[0].Width = static_cast<ushort>(w);
    collection.Main.Frames[0].Height = static_cast<ushort>(h);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadBam(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    // Format: fileName$5-6.spr
    auto opt_str = string(opt);
    istringstream idelim(opt_str);
    uint need_cycle = 0;
    auto specific_frame = -1;
    idelim >> need_cycle;
    if (opt.find('-') != string::npos) {
        char ch = 0;
        idelim >> ch >> specific_frame;
        if (specific_frame < 0) {
            specific_frame = -1;
        }
    }

    // Read signature
    char head[8];
    file.CopyData(head, 8);
    if (!_str(head).startsWith("BAM")) {
        throw ImageBakerException("Invalid BAM file header", fname);
    }

    // Packed
    if (head[3] == 'C') {
        auto unpacked_len = file.GetLEUInt();
        auto data_len = file.GetSize() - 12;

        auto* buf = const_cast<uchar*>(file.GetBuf());
        *reinterpret_cast<ushort*>(buf) = 0x9C78;

        const auto data = Compressor::Uncompress({buf, data_len}, unpacked_len / file.GetSize() + 1);

        if (data.empty()) {
            throw ImageBakerException("Cab't unpack BAM file", fname);
        }

        file = File(data);
        file.CopyData(head, 8);
        if (!_str(head).startsWith("BAM")) {
            throw ImageBakerException("Invalid BAM file unpacked header", fname);
        }
    }

    // Read header
    uint frames_count = file.GetLEUShort();
    uint cycles_count = file.GetUChar();
    auto compr_color = file.GetUChar();
    auto frames_offset = file.GetLEUInt();
    auto palette_offset = file.GetLEUInt();
    auto lookup_table_offset = file.GetLEUInt();

    // Count whole frames
    if (need_cycle >= cycles_count) {
        need_cycle = 0;
    }

    file.SetCurPos(frames_offset + frames_count * 12 + need_cycle * 4);
    uint cycle_frames = file.GetLEUShort();
    RUNTIME_ASSERT(cycle_frames <= MAX_FRAME_SEQUENCE);
    uint table_index = file.GetLEUShort();
    if (specific_frame != -1 && specific_frame >= static_cast<int>(cycle_frames)) {
        specific_frame = 0;
    }

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = static_cast<ushort>(specific_frame == -1 ? cycle_frames : 1u);
    collection.AnimTicks = 100u;

    // Palette
    uint palette[256] = {0};
    file.SetCurPos(palette_offset);
    file.CopyData(palette, 256 * 4);

    // Find in lookup table
    for (auto i = 0u; i < cycle_frames; i++) {
        if (specific_frame != -1 && specific_frame != static_cast<int>(i)) {
            continue;
        }

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
        auto data_offset = file.GetLEUInt();
        auto rle = (data_offset & 0x80000000) == 0;
        data_offset &= 0x7FFFFFFF;

        // Allocate data
        vector<uchar> data(w * h * 4);
        auto* ptr = reinterpret_cast<uint*>(data.data());

        // Fill it
        file.SetCurPos(data_offset);
        for (uint k = 0, l = w * h; k < l;) {
            auto index = file.GetUChar();
            auto color = palette[index];
            if (color == 0xFF00) {
                color = 0;
            }
            else {
                color |= 0xFF000000;
            }

            color = COLOR_SWAP_RB(color);

            if (rle && index == compr_color) {
                uint copies = file.GetUChar();
                for (uint m = 0; m <= copies; m++, k++) {
                    *ptr++ = color;
                }
            }
            else {
                *ptr++ = color;
                k++;
            }
        }

        // Set in animation sequence
        auto& shot = collection.Main.Frames[specific_frame != -1 ? 0 : i];
        shot.Width = static_cast<ushort>(w);
        shot.Height = static_cast<ushort>(h);
        shot.NextX = static_cast<short>(-ox + w / 2);
        shot.NextY = static_cast<short>(-oy + h);
        shot.Data = std::move(data);

        if (specific_frame != -1) {
            break;
        }
    }

    return collection;
}

auto ImageBaker::LoadPng(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    uint w = 0;
    uint h = 0;
    const auto* png_data = PngLoad(file.GetBuf(), w, h);
    if (png_data == nullptr) {
        throw ImageBakerException("Can't read PNG", fname);
    }

    vector<uchar> data(w * h * 4);
    std::memcpy(data.data(), png_data, w * h * 4);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = static_cast<ushort>(w);
    collection.Main.Frames[0].Height = static_cast<ushort>(h);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadTga(string_view fname, string_view opt, File& file) -> FrameCollection
{
    NON_CONST_METHOD_HINT();

    uint w = 0;
    uint h = 0;
    const auto* tga_data = TgaLoad(file.GetBuf(), file.GetSize(), w, h);
    if (tga_data == nullptr) {
        throw ImageBakerException("Can't read TGA", fname);
    }

    vector<uchar> data(w * h * 4);
    std::memcpy(data.data(), tga_data, w * h * 4);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames[0].Width = static_cast<ushort>(w);
    collection.Main.Frames[0].Height = static_cast<ushort>(h);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

static auto PngLoad(const uchar* data, uint& result_width, uint& result_height) -> uchar*
{
    struct PngMessage
    {
        static void Error(png_structp png_ptr, png_const_charp error_msg)
        {
            UNUSED_VARIABLE(png_ptr);
            WriteLog("PNG error '{}'", error_msg);
        }
        static void Warning(png_structp png_ptr, png_const_charp /*error_msg*/)
        {
            UNUSED_VARIABLE(png_ptr);
            // WriteLog( "PNG warning '{}'", error_msg );
        }
    };

    // Setup PNG reader
    auto* png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png_ptr == nullptr) {
        return nullptr;
    }

    png_set_error_fn(png_ptr, png_get_error_ptr(png_ptr), &PngMessage::Error, &PngMessage::Warning);

    auto* info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    struct PngReader
    {
        static void Read(png_structp png_ptr, png_bytep png_data, png_size_t length)
        {
            auto** io_ptr = static_cast<uchar**>(png_get_io_ptr(png_ptr));
            std::memcpy(png_data, *io_ptr, length);
            *io_ptr += length;
        }
    };
    png_set_read_fn(png_ptr, static_cast<png_voidp>(&data), &PngReader::Read);
    png_read_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    // Get information
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    auto bit_depth = 0;
    auto color_type = 0;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);

    // Settings
    png_set_strip_16(png_ptr);
    png_set_packing(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_expand(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0u) {
        png_set_expand(png_ptr);
    }
    png_set_filler(png_ptr, 0x000000ff, PNG_FILLER_AFTER);
    png_read_update_info(png_ptr, info_ptr);

    // Allocate row pointers
    auto* row_pointers = static_cast<png_bytepp>(malloc(height * sizeof(png_bytep)));
    if (row_pointers == nullptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return nullptr;
    }

    // Set the individual row_pointers to point at the correct offsets
    auto* result = new uchar[width * height * 4];
    for (uint i = 0; i < height; i++) {
        row_pointers[i] = result + i * width * 4;
    }

    // Read image
    png_read_image(png_ptr, row_pointers);

    // Clean up
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));
    free(row_pointers);

    // Return
    result_width = width;
    result_height = height;
    return result;
}

static auto TgaLoad(const uchar* data, size_t data_size, uint& result_width, uint& result_height) -> uchar*
{
    // Reading macros
    auto read_error = false;
    uint cur_pos = 0;
#define READ_TGA(x, len) \
    if (!read_error && cur_pos + (len) <= data_size) { \
        std::memcpy(x, data + cur_pos, len); \
        cur_pos += (len); \
    } \
    else { \
        std::memset(x, 0, len); \
        read_error = true; \
    }

    // Load header
    unsigned char type = 0;
    unsigned char pixel_depth = 0;
    short int width = 0;
    short int height = 0;
    unsigned char unused_char = 0;
    short int unused_short = 0;
    READ_TGA(&unused_char, 1)
    READ_TGA(&unused_char, 1)
    READ_TGA(&type, 1)
    READ_TGA(&unused_short, 2)
    READ_TGA(&unused_short, 2)
    READ_TGA(&unused_char, 1)
    READ_TGA(&unused_short, 2)
    READ_TGA(&unused_short, 2)
    READ_TGA(&width, 2)
    READ_TGA(&height, 2)
    READ_TGA(&pixel_depth, 1)
    READ_TGA(&unused_char, 1)

    // Check for errors when loading the header
    if (read_error) {
        return nullptr;
    }

    // Check if the image is color indexed
    if (type == 1) {
        return nullptr;
    }

    // Check for TrueColor
    if (type != 2 && type != 10) {
        return nullptr;
    }

    // Check for RGB(A)
    if (pixel_depth != 24 && pixel_depth != 32) {
        return nullptr;
    }

    // Read
    const auto bpp = pixel_depth / 8;
    const uint read_size = height * width * bpp;
    auto* read_data = new uchar[read_size];
    if (type == 2) {
        READ_TGA(read_data, read_size)
    }
    else {
        uint bytes_read = 0;
        uchar header = 0;
        uchar color[4];
        while (bytes_read < read_size) {
            READ_TGA(&header, 1)
            if ((header & 0x00000080) != 0) {
                header &= ~0x00000080;
                READ_TGA(color, bpp)
                if (read_error) {
                    delete[] read_data;
                    return nullptr;
                }
                const uint run_len = (header + 1) * bpp;
                for (uint i = 0; i < run_len; i += bpp) {
                    for (auto c = 0; c < bpp && bytes_read + i + c < read_size; c++) {
                        read_data[bytes_read + i + c] = color[c];
                    }
                }
                bytes_read += run_len;
            }
            else {
                const uint run_len = (header + 1) * bpp;
                uint to_read;
                if (bytes_read + run_len > read_size) {
                    to_read = read_size - bytes_read;
                }
                else {
                    to_read = run_len;
                }
                READ_TGA(read_data + bytes_read, to_read)
                if (read_error) {
                    delete[] read_data;
                    return nullptr;
                }
                bytes_read += run_len;
                if (bytes_read + run_len > read_size) {
                    cur_pos += run_len - to_read;
                }
            }
        }
    }
    if (read_error) {
        delete[] read_data;
        return nullptr;
    }

    // Copy data
    auto* result = new uchar[width * height * 4];
    for (short y = 0; y < height; y++) {
        for (short x = 0; x < width; x++) {
            const auto i = (height - y - 1) * width + x;
            const auto j = y * width + x;
            result[i * 4 + 0] = read_data[j * bpp + 2];
            result[i * 4 + 1] = read_data[j * bpp + 1];
            result[i * 4 + 2] = read_data[j * bpp + 0];
            result[i * 4 + 3] = bpp == 4 ? read_data[j * bpp + 3] : 0xFF;
        }
    }
    delete[] read_data;

    // Return data
    result_width = width;
    result_height = height;
    return result;

#undef READ_TGA
}
