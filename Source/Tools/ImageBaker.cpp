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

#include "ImageBaker.h"
#include "Application.h"
#include "ConfigFile.h"
#include "FileSystem.h"
#include "Settings.h"

#include "png.h"

FO_BEGIN_NAMESPACE();

[[nodiscard]] static auto PngLoad(const uint8* data, int32& result_width, int32& result_height) -> vector<uint8>;
[[nodiscard]] static auto TgaLoad(const uint8* data, size_t data_size, int32& result_width, int32& result_height) -> vector<uint8>;

// clang-format off
alignas(ucolor) static uint8 FoPalette[] = {
    // Transparent color
    0x00, 0x00, 0x00, 0x00,
    // Default colors
    0xec, 0xec, 0xec, 0xff, 0xdc, 0xdc, 0xdc, 0xff, 0xcc, 0xcc, 0xcc, 0xff, 0xbc, 0xbc, 0xbc, 0xff, 0xb0, 0xb0, 0xb0, 0xff, 0xa0, 0xa0, 0xa0, 0xff, 0x90, 0x90, 0x90, 0xff, 
    0x80, 0x80, 0x80, 0xff, 0x74, 0x74, 0x74, 0xff, 0x64, 0x64, 0x64, 0xff, 0x54, 0x54, 0x54, 0xff, 0x48, 0x48, 0x48, 0xff, 0x38, 0x38, 0x38, 0xff, 0x28, 0x28, 0x28, 0xff,
    0x20, 0x20, 0x20, 0xff, 0xfc, 0xec, 0xec, 0xff, 0xec, 0xd8, 0xd8, 0xff, 0xdc, 0xc4, 0xc4, 0xff, 0xd0, 0xb0, 0xb0, 0xff, 0xc0, 0xa0, 0xa0, 0xff, 0xb0, 0x90, 0x90, 0xff,
    0xa4, 0x80, 0x80, 0xff, 0x94, 0x70, 0x70, 0xff, 0x84, 0x60, 0x60, 0xff, 0x78, 0x54, 0x54, 0xff, 0x68, 0x44, 0x44, 0xff, 0x58, 0x38, 0x38, 0xff, 0x4c, 0x2c, 0x2c, 0xff,
    0x3c, 0x24, 0x24, 0xff, 0x2c, 0x18, 0x18, 0xff, 0x20, 0x10, 0x10, 0xff, 0xec, 0xec, 0xfc, 0xff, 0xd8, 0xd8, 0xec, 0xff, 0xc4, 0xc4, 0xdc, 0xff, 0xb0, 0xb0, 0xd0, 0xff,
    0xa0, 0xa0, 0xc0, 0xff, 0x90, 0x90, 0xb0, 0xff, 0x80, 0x80, 0xa4, 0xff, 0x70, 0x70, 0x94, 0xff, 0x60, 0x60, 0x84, 0xff, 0x54, 0x54, 0x78, 0xff, 0x44, 0x44, 0x68, 0xff,
    0x38, 0x38, 0x58, 0xff, 0x2c, 0x2c, 0x4c, 0xff, 0x24, 0x24, 0x3c, 0xff, 0x18, 0x18, 0x2c, 0xff, 0x10, 0x10, 0x20, 0xff, 0xfc, 0xb0, 0xf0, 0xff, 0xc4, 0x60, 0xa8, 0xff,
    0x68, 0x24, 0x60, 0xff, 0x4c, 0x14, 0x48, 0xff, 0x38, 0x0c, 0x34, 0xff, 0x28, 0x10, 0x24, 0xff, 0x24, 0x04, 0x24, 0xff, 0x1c, 0x0c, 0x18, 0xff, 0xfc, 0xfc, 0xc8, 0xff,
    0xfc, 0xfc, 0x7c, 0xff, 0xe4, 0xd8, 0x0c, 0xff, 0xcc, 0xb8, 0x1c, 0xff, 0xb8, 0x9c, 0x28, 0xff, 0xa4, 0x88, 0x30, 0xff, 0x90, 0x78, 0x24, 0xff, 0x7c, 0x68, 0x18, 0xff,
    0x6c, 0x58, 0x10, 0xff, 0x58, 0x48, 0x08, 0xff, 0x48, 0x38, 0x04, 0xff, 0x34, 0x28, 0x00, 0xff, 0x20, 0x18, 0x00, 0xff, 0xd8, 0xfc, 0x9c, 0xff, 0xb4, 0xd8, 0x84, 0xff,
    0x98, 0xb8, 0x70, 0xff, 0x78, 0x98, 0x5c, 0xff, 0x5c, 0x78, 0x48, 0xff, 0x40, 0x58, 0x34, 0xff, 0x28, 0x38, 0x20, 0xff, 0x70, 0x60, 0x50, 0xff, 0x54, 0x48, 0x34, 0xff,
    0x38, 0x30, 0x20, 0xff, 0x68, 0x78, 0x50, 0xff, 0x70, 0x78, 0x20, 0xff, 0x70, 0x68, 0x28, 0xff, 0x60, 0x60, 0x24, 0xff, 0x4c, 0x44, 0x24, 0xff, 0x38, 0x30, 0x20, 0xff,
    0x9c, 0xac, 0x9c, 0xff, 0x78, 0x94, 0x78, 0xff, 0x58, 0x7c, 0x58, 0xff, 0x40, 0x68, 0x40, 0xff, 0x38, 0x58, 0x58, 0xff, 0x30, 0x4c, 0x48, 0xff, 0x28, 0x44, 0x3c, 0xff,
    0x20, 0x3c, 0x2c, 0xff, 0x1c, 0x30, 0x24, 0xff, 0x14, 0x28, 0x18, 0xff, 0x10, 0x20, 0x10, 0xff, 0x18, 0x30, 0x18, 0xff, 0x10, 0x24, 0x0c, 0xff, 0x08, 0x1c, 0x04, 0xff,
    0x04, 0x14, 0x00, 0xff, 0x04, 0x0c, 0x00, 0xff, 0x8c, 0x9c, 0x9c, 0xff, 0x78, 0x94, 0x98, 0xff, 0x64, 0x88, 0x94, 0xff, 0x50, 0x7c, 0x90, 0xff, 0x40, 0x6c, 0x8c, 0xff,
    0x30, 0x58, 0x8c, 0xff, 0x2c, 0x4c, 0x7c, 0xff, 0x28, 0x44, 0x6c, 0xff, 0x20, 0x38, 0x5c, 0xff, 0x1c, 0x30, 0x4c, 0xff, 0x18, 0x28, 0x40, 0xff, 0x9c, 0xa4, 0xa4, 0xff,
    0x38, 0x48, 0x68, 0xff, 0x50, 0x58, 0x58, 0xff, 0x58, 0x68, 0x84, 0xff, 0x38, 0x40, 0x50, 0xff, 0xbc, 0xbc, 0xbc, 0xff, 0xac, 0xa4, 0x98, 0xff, 0xa0, 0x90, 0x7c, 0xff,
    0x94, 0x7c, 0x60, 0xff, 0x88, 0x68, 0x4c, 0xff, 0x7c, 0x58, 0x34, 0xff, 0x70, 0x48, 0x24, 0xff, 0x64, 0x3c, 0x14, 0xff, 0x58, 0x30, 0x08, 0xff, 0xfc, 0xcc, 0xcc, 0xff,
    0xfc, 0xb0, 0xb0, 0xff, 0xfc, 0x98, 0x98, 0xff, 0xfc, 0x7c, 0x7c, 0xff, 0xfc, 0x64, 0x64, 0xff, 0xfc, 0x48, 0x48, 0xff, 0xfc, 0x30, 0x30, 0xff, 0xfc, 0x00, 0x00, 0xff,
    0xe0, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x00, 0xff, 0xa8, 0x00, 0x00, 0xff, 0x90, 0x00, 0x00, 0xff, 0x74, 0x00, 0x00, 0xff, 0x58, 0x00, 0x00, 0xff, 0x40, 0x00, 0x00, 0xff,
    0xfc, 0xe0, 0xc8, 0xff, 0xfc, 0xc4, 0x94, 0xff, 0xfc, 0xb8, 0x78, 0xff, 0xfc, 0xac, 0x60, 0xff, 0xfc, 0x9c, 0x48, 0xff, 0xfc, 0x94, 0x2c, 0xff, 0xfc, 0x88, 0x14, 0xff,
    0xfc, 0x7c, 0x00, 0xff, 0xdc, 0x6c, 0x00, 0xff, 0xc0, 0x60, 0x00, 0xff, 0xa4, 0x50, 0x00, 0xff, 0x84, 0x44, 0x00, 0xff, 0x68, 0x34, 0x00, 0xff, 0x4c, 0x24, 0x00, 0xff,
    0x30, 0x18, 0x00, 0xff, 0xf8, 0xd4, 0xa4, 0xff, 0xd8, 0xb0, 0x78, 0xff, 0xc8, 0xa0, 0x64, 0xff, 0xbc, 0x90, 0x54, 0xff, 0xac, 0x80, 0x44, 0xff, 0x9c, 0x74, 0x34, 0xff,
    0x8c, 0x64, 0x28, 0xff, 0x7c, 0x58, 0x1c, 0xff, 0x70, 0x4c, 0x14, 0xff, 0x60, 0x40, 0x08, 0xff, 0x50, 0x34, 0x04, 0xff, 0x40, 0x28, 0x00, 0xff, 0x34, 0x20, 0x00, 0xff,
    0xfc, 0xe4, 0xb8, 0xff, 0xe8, 0xc8, 0x98, 0xff, 0xd4, 0xac, 0x7c, 0xff, 0xc4, 0x90, 0x64, 0xff, 0xb0, 0x74, 0x4c, 0xff, 0xa0, 0x5c, 0x38, 0xff, 0x90, 0x4c, 0x2c, 0xff,
    0x84, 0x3c, 0x20, 0xff, 0x78, 0x2c, 0x18, 0xff, 0x6c, 0x20, 0x10, 0xff, 0x5c, 0x14, 0x08, 0xff, 0x48, 0x0c, 0x04, 0xff, 0x3c, 0x04, 0x00, 0xff, 0xfc, 0xe8, 0xdc, 0xff,
    0xf8, 0xd4, 0xbc, 0xff, 0xf4, 0xc0, 0xa0, 0xff, 0xf0, 0xb0, 0x84, 0xff, 0xf0, 0xa0, 0x6c, 0xff, 0xf0, 0x94, 0x5c, 0xff, 0xd8, 0x80, 0x54, 0xff, 0xc0, 0x70, 0x48, 0xff,
    0xa8, 0x60, 0x40, 0xff, 0x90, 0x50, 0x38, 0xff, 0x78, 0x40, 0x30, 0xff, 0x60, 0x30, 0x24, 0xff, 0x48, 0x24, 0x1c, 0xff, 0x38, 0x18, 0x14, 0xff, 0x64, 0xe4, 0x64, 0xff,
    0x14, 0x98, 0x14, 0xff, 0x00, 0xa4, 0x00, 0xff, 0x50, 0x50, 0x48, 0xff, 0x00, 0x6c, 0x00, 0xff, 0x8c, 0x8c, 0x84, 0xff, 0x1c, 0x1c, 0x1c, 0xff, 0x68, 0x50, 0x38, 0xff,
    0x30, 0x28, 0x20, 0xff, 0x8c, 0x70, 0x60, 0xff, 0x48, 0x38, 0x28, 0xff, 0x0c, 0x0c, 0x0c, 0xff, 0x3c, 0x3c, 0x3c, 0xff, 0x6c, 0x74, 0x6c, 0xff, 0x78, 0x84, 0x78, 0xff,
    0x88, 0x94, 0x88, 0xff, 0x94, 0xa4, 0x94, 0xff, 0x58, 0x68, 0x60, 0xff, 0x60, 0x70, 0x68, 0xff, 0x3c, 0xf8, 0x00, 0xff, 0x38, 0xd4, 0x08, 0xff, 0x34, 0xb4, 0x10, 0xff,
    0x30, 0x94, 0x14, 0xff, 0x28, 0x74, 0x18, 0xff, 0xfc, 0xfc, 0xfc, 0xff, 0xf0, 0xec, 0xd0, 0xff, 0xd0, 0xb8, 0x88, 0xff, 0x98, 0x7c, 0x50, 0xff, 0x68, 0x58, 0x3c, 0xff,
    0x50, 0x40, 0x24, 0xff, 0x34, 0x28, 0x1c, 0xff, 0x18, 0x10, 0x0c, 0xff, 0x03, 0x03, 0x03, 0xff,
    // Slime
    0x00, 0x6c, 0x00, 0xff, 0x0b, 0x73, 0x07, 0xff, 0x1b, 0x7b, 0x0f, 0xff, 0x2b, 0x83, 0x1b, 0xff,
    // Monitors
    0x6b, 0x6b, 0x6f, 0xff, 0x63, 0x67, 0x7f, 0xff, 0x57, 0x6b, 0x8f, 0xff, 0x00, 0x93, 0xa3, 0xff, 0x6b, 0xbb, 0xff, 0xff,
    // FireSlow
    0xff, 0x00, 0x00, 0xff, 0xd7, 0x00, 0x00, 0xff, 0x93, 0x2b, 0x0b, 0xff, 0xff, 0x77, 0x00, 0xff, 0xff, 0x3b, 0x00, 0xff,
    // FireFast
    0x47, 0x00, 0x00, 0xff, 0x7b, 0x00, 0x00, 0xff, 0xb3, 0x00, 0x00, 0xff, 0x7b, 0x00, 0x00, 0xff, 0x47, 0x00, 0x00, 0xff,
    // Shoreline
    0x53, 0x3f, 0x2b, 0xff, 0x4b, 0x3b, 0x2b, 0xff, 0x43, 0x37, 0x27, 0xff, 0x3f, 0x33, 0x27, 0xff, 0x37, 0x2f, 0x23, 0xff, 0x33, 0x2b, 0x23, 0xff,
    // BlinkingRed
    0xfc, 0x00, 0x00, 0xff,
    // Unused
    0xff, 0xff, 0xff, 0xff
};
static_assert(sizeof(FoPalette) == 1024);
// clang-format on

ImageBaker::ImageBaker(BakerData& data) :
    BaseBaker(data)
{
    FO_STACK_TRACE_ENTRY();

    // Fill default loaders
    AddLoader(std::bind(&ImageBaker::LoadFofrm, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"fofrm"});
    AddLoader(std::bind(&ImageBaker::LoadFrm, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"frm"});
    AddLoader(std::bind(&ImageBaker::LoadFrX, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"fr0"});
    AddLoader(std::bind(&ImageBaker::LoadRix, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"rix"});
    AddLoader(std::bind(&ImageBaker::LoadArt, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"art"});
    AddLoader(std::bind(&ImageBaker::LoadSpr, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"spr"});
    AddLoader(std::bind(&ImageBaker::LoadZar, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"zar"});
    AddLoader(std::bind(&ImageBaker::LoadTil, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"til"});
    AddLoader(std::bind(&ImageBaker::LoadMos, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"mos"});
    AddLoader(std::bind(&ImageBaker::LoadBam, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"bam"});
    AddLoader(std::bind(&ImageBaker::LoadPng, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"png"});
    AddLoader(std::bind(&ImageBaker::LoadTga, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), {"tga"});
}

void ImageBaker::AddLoader(const LoadFunc& loader, const vector<string>& file_extensions)
{
    FO_STACK_TRACE_ENTRY();

    for (const auto& ext : file_extensions) {
        _fileLoaders[ext] = loader;
    }
}

void ImageBaker::BakeFiles(const FileCollection& files, string_view target_path) const
{
    FO_STACK_TRACE_ENTRY();

    vector<pair<File, LoadFunc>> files_to_bake;
    files_to_bake.reserve(files.GetFilesCount());

    if (target_path.empty()) {
        for (auto&& [ext, loader] : _fileLoaders) {
            for (const auto& file_header : files) {
                const string file_ext = strex(file_header.GetPath()).get_file_extension();

                if (file_ext != ext) {
                    continue;
                }
                if (_bakeChecker && !_bakeChecker(file_header.GetPath(), file_header.GetWriteTime())) {
                    continue;
                }

                files_to_bake.emplace_back(File::Load(file_header), loader);
            }
        }
    }
    else {
        const string ext = strex(target_path).get_file_extension();

        if (!_fileLoaders.contains(ext)) {
            return;
        }

        auto file = files.FindFileByPath(target_path);

        if (!file) {
            return;
        }
        if (_bakeChecker && !_bakeChecker(target_path, file.GetWriteTime())) {
            return;
        }

        files_to_bake.emplace_back(std::move(file), _fileLoaders.at(ext));
    }

    vector<std::future<void>> file_bakings;

    for (auto& file_to_bake : files_to_bake) {
        file_bakings.emplace_back(std::async(GetAsyncMode(), [&] {
            const auto& path = file_to_bake.first.GetPath();
            const auto collection = file_to_bake.second(path, "", file_to_bake.first.GetReader(), files);
            BakeCollection(path, collection);
        }));
    }

    size_t errors = 0;

    for (auto& file_baking : file_bakings) {
        try {
            file_baking.get();
        }
        catch (const std::exception& ex) {
            WriteLog("Image baking error: {}", ex.what());
            errors++;
        }
        catch (...) {
            FO_UNKNOWN_EXCEPTION();
        }
    }

    if (errors != 0) {
        throw ImageBakerException("Errors during images baking", errors);
    }
}

void ImageBaker::BakeCollection(string_view fname, const FrameCollection& collection) const
{
    FO_STACK_TRACE_ENTRY();

    vector<uint8> data;
    auto writer = DataWriter(data);

    constexpr auto check_number = numeric_cast<uint8>(42);
    const auto dirs = numeric_cast<uint8>(collection.HaveDirs ? GameSettings::MAP_DIR_COUNT : 1);

    writer.Write<uint8>(check_number);
    writer.Write<uint16>(collection.SequenceSize);
    writer.Write<uint16>(collection.AnimTicks);
    writer.Write<uint8>(dirs);

    for (const auto dir : iterate_range(dirs)) {
        const auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        writer.Write<int16>(sequence.OffsX);
        writer.Write<int16>(sequence.OffsY);

        for (int32 i = 0; i < collection.SequenceSize; i++) {
            const auto& shot = sequence.Frames[i];
            writer.Write<bool>(shot.Shared);

            if (!shot.Shared) {
                writer.Write<uint16>(shot.Width);
                writer.Write<uint16>(shot.Height);
                writer.Write<int16>(shot.NextX);
                writer.Write<int16>(shot.NextY);
                writer.WritePtr(shot.Data.data(), shot.Data.size());
                FO_RUNTIME_ASSERT(shot.Data.size() == numeric_cast<size_t>(shot.Width) * shot.Height * 4);
            }
            else {
                writer.Write<uint16>(shot.SharedIndex);
            }
        }
    }

    writer.Write<uint8>(check_number);

    if (!collection.NewName.empty()) {
        _writeData(collection.NewName, data);
    }
    else {
        _writeData(fname, data);
    }
}

auto ImageBaker::LoadAny(string_view fname_with_opt, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    const string ext = strex(fname_with_opt).get_file_extension();
    const string dir = strex(fname_with_opt).extract_dir();
    const string name = strex(fname_with_opt).extract_file_name().erase_file_extension().substring_until('$');
    const string fname = strex("{}/{}.{}", dir, name, ext);
    const string opt = strex(fname_with_opt).extract_file_name().erase_file_extension().substring_after('$');

    const auto file = files.FindFileByPath(fname);

    if (!file) {
        throw ImageBakerException("Image file not found", fname, fname_with_opt, dir, name, ext);
    }

    if (const auto it = _fileLoaders.find(ext); it != _fileLoaders.end()) {
        return it->second(fname, opt, file.GetReader(), files);
    }
    else {
        throw ImageBakerException("Invalid image file extension", fname);
    }
}

auto ImageBaker::LoadFofrm(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);

    FrameCollection collection;

    ConfigFile fofrm("Image.fofrm", reader.GetStr());

    int32 frm_fps = fofrm.GetAsInt("", "fps", 10);
    frm_fps = fofrm.GetAsInt("", "Fps", frm_fps);
    int32 frm_count = fofrm.GetAsInt("", "count", 1);
    frm_count = fofrm.GetAsInt("", "Count", frm_count);
    FO_RUNTIME_ASSERT(frm_count > 0);

    int32 ox = fofrm.GetAsInt("", "offs_x", 0);
    ox = fofrm.GetAsInt("", "OffsetX", ox);
    int32 oy = fofrm.GetAsInt("", "offs_y", 0);
    oy = fofrm.GetAsInt("", "OffsetY", oy);

    collection.EffectName = fofrm.GetAsStr("", "effect");
    collection.EffectName = fofrm.GetAsStr("", "Effect", collection.EffectName);

    for (const auto dir : iterate_range(GameSettings::MAP_DIR_COUNT)) {
        vector<tuple<FrameCollection, int32, int32>> sub_collections;
        sub_collections.reserve(10);

        string dir_str = strex("dir_{}", dir);

        if (!fofrm.HasSection(dir_str)) {
            dir_str = strex("Dir_{}", dir);

            if (!fofrm.HasSection(dir_str)) {
                dir_str = "";
            }
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
            ox = fofrm.GetAsInt(dir_str, "offs_x", ox);
            ox = fofrm.GetAsInt(dir_str, "OffsetX", ox);
            oy = fofrm.GetAsInt(dir_str, "offs_y", oy);
            oy = fofrm.GetAsInt(dir_str, "OffsetY", oy);
        }

        string frm_dir = strex(fname).extract_dir();

        int32 frames = 0;
        bool no_info = false;
        bool load_fail = false;

        for (int32 frm = 0; frm < frm_count; frm++) {
            auto frm_name = fofrm.GetAsStr(dir_str, strex("frm_{}", frm));

            if (frm_name.empty()) {
                frm_name = fofrm.GetAsStr(dir_str, strex("Frm_{}", frm), frm_name);
            }

            if (frm_name.empty() && frm == 0) {
                frm_name = fofrm.GetAsStr(dir_str, "frm");

                if (frm_name.empty()) {
                    frm_name = fofrm.GetAsStr(dir_str, "Frm");
                }
            }

            if (frm_name.empty()) {
                no_info = true;
                break;
            }

            auto sub_collection = LoadAny(strex("{}/{}", frm_dir, frm_name), files);
            frames += sub_collection.SequenceSize;

            int32 next_x = fofrm.GetAsInt(dir_str, strex("next_x_{}", frm), 0);
            next_x = fofrm.GetAsInt(dir_str, strex("NextX_{}", frm), next_x);
            int32 next_y = fofrm.GetAsInt(dir_str, strex("next_y_{}", frm), 0);
            next_y = fofrm.GetAsInt(dir_str, strex("NextY_{}", frm), next_y);

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
        if (dir == 0) {
            collection.SequenceSize = numeric_cast<uint16>(frames);
            collection.AnimTicks = numeric_cast<uint16>(frm_fps != 0 ? 1000 * frm_count / frm_fps : 0);
        }
        else {
            collection.HaveDirs = true;
        }

        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        sequence.OffsX = numeric_cast<int16>(ox);
        sequence.OffsY = numeric_cast<int16>(oy);
        sequence.Frames.resize(collection.SequenceSize);

        // Merge many animations in one
        int32 frm = 0;

        for (auto&& [sub_collection, next_x, next_y] : sub_collections) {
            for (int32 i = 0; i < sub_collection.SequenceSize; i++, frm++) {
                auto& shot = sequence.Frames[frm];

                if (sub_collection.Main.Frames[i].Shared) {
                    throw ImageBakerException("FOFRM file invalid data (shared index)", fname);
                }

                shot.Width = sub_collection.Main.Frames[i].Width;
                shot.Height = sub_collection.Main.Frames[i].Height;
                shot.Data = sub_collection.Main.Frames[i].Data;
                shot.NextX = numeric_cast<int16>(sub_collection.Main.Frames[i].NextX + next_x);
                shot.NextY = numeric_cast<int16>(sub_collection.Main.Frames[i].NextY + next_y);
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadFrm(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);

    reader.SetCurPos(0x4);
    auto frm_fps = reader.GetBEUInt16();

    reader.SetCurPos(0x8);
    auto frm_count = reader.GetBEUInt16();
    FO_RUNTIME_ASSERT(frm_count > 0);

    FrameCollection collection;
    collection.SequenceSize = frm_count;
    collection.AnimTicks = frm_fps != 0 ? 1000 / frm_fps * frm_count : 0;

    if (strex(fname).starts_with("art/critters/")) {
        collection.NewName = strex(fname).lower();
    }

    // Animate pixels
    // 0x00 - None
    // 0x01 - Slime, 229 - 232, 4 frames
    // 0x02 - Monitors, 233 - 237, 5 frames
    // 0x04 - FireSlow, 238 - 242, 5 frames
    // 0x08 - FireFast, 243 - 247, 5 frames
    // 0x10 - Shoreline, 248 - 253, 6 frames
    // 0x20 - BlinkingRed, 254, 15 frames
    uint32 anim_pix_type = 0;
    const uint8 blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

    for (const auto dir : iterate_range(GameSettings::MAP_DIR_COUNT)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        sequence.Frames.resize(collection.SequenceSize);

        auto dir_frm = dir;

        if constexpr (GameSettings::SQUARE_GEOMETRY) {
            if (dir >= 3) {
                dir_frm--;
            }
            if (dir >= 7) {
                dir_frm--;
            }
        }

        reader.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = reader.GetBEInt16();
        reader.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = reader.GetBEInt16();

        reader.SetCurPos(0x22 + dir_frm * 4);
        auto offset = 0x3E + reader.GetBEUInt32();

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
        auto* palette = reinterpret_cast<ucolor*>(FoPalette);
        ucolor custom_palette[256];
        File palette_file = files.FindFileByPath(strex("{}.pal", strex(fname).erase_file_extension()));

        if (palette_file) {
            auto palette_reader = palette_file.GetReader();

            for (auto& color : custom_palette) {
                uint8 r = palette_reader.GetUInt8() * 4;
                uint8 g = palette_reader.GetUInt8() * 4;
                uint8 b = palette_reader.GetUInt8() * 4;
                color = ucolor {r, g, b, 0xFF};
            }

            custom_palette[0] = ucolor {0};
            palette = custom_palette;
        }

        for (uint16 frm = 0; frm < frm_count; frm++) {
            auto& shot = sequence.Frames[frm];

            reader.SetCurPos(offset);
            auto w = reader.GetBEUInt16();
            auto h = reader.GetBEUInt16();
            shot.Width = w;
            shot.Height = h;
            reader.GoForward(4); // Frame size
            shot.NextX = reader.GetBEInt16();
            shot.NextY = reader.GetBEInt16();

            // Allocate data
            shot.Data.resize(numeric_cast<size_t>(w) * h * 4);
            auto* ptr = reinterpret_cast<ucolor*>(shot.Data.data());
            reader.SetCurPos(offset + 12);

            if (anim_pix_type == 0) {
                for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                    *(ptr + i) = palette[reader.GetUInt8()];
                }
            }
            else {
                for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                    auto index = reader.GetUInt8();

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
                            *(ptr + i) = ucolor {blinking_red_vals[frm % 10], 0, 0};
                            continue;
                        }
                    }

                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (anim_pix_type == 0 && frm == 0 && dir == 0 && frm_count == 1 && palette == reinterpret_cast<ucolor*>(FoPalette)) {
                reader.SetCurPos(offset + 12);

                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = reader.GetUInt8();

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
                    int32 divs[4] {1, 1, 1, 1};

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

                    frm_count = 4;

                    for (auto i = 0; i < 4; i++) {
                        if (frm_count % divs[i] == 0) {
                            continue;
                        }

                        frm_count++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_count;
                    collection.SequenceSize = frm_count;
                    sequence.Frames.resize(collection.SequenceSize);
                }
            }

            if (anim_pix_type == 0) {
                offset += w * h + 12;
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadFrX(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);

    File dir_file;

    // Load from frm
    reader.SetCurPos(0x4);
    auto frm_fps = reader.GetBEUInt16();

    reader.SetCurPos(0x8);
    auto frm_count = reader.GetBEUInt16();
    FO_RUNTIME_ASSERT(frm_count > 0);

    FrameCollection collection;
    collection.SequenceSize = frm_count;
    collection.AnimTicks = frm_fps != 0 ? 1000 / frm_fps * frm_count : 0;

    if (strex(fname).starts_with("art/critters/")) {
        collection.NewName = strex("{}.{}", strex(fname).erase_file_extension().lower(), "fofrm");
    }
    else {
        collection.NewName = strex("{}.{}", strex(fname).erase_file_extension(), "frm");
    }

    // Animate pixels
    // 0x00 - None
    // 0x01 - Slime, 229 - 232, 4 frames
    // 0x02 - Monitors, 233 - 237, 5 frames
    // 0x04 - FireSlow, 238 - 242, 5 frames
    // 0x08 - FireFast, 243 - 247, 5 frames
    // 0x10 - Shoreline, 248 - 253, 6 frames
    // 0x20 - BlinkingRed, 254, 15 frames
    uint32 anim_pix_type = 0;
    const uint8 blinking_red_vals[10] = {254, 210, 165, 120, 75, 45, 90, 135, 180, 225};

    for (const auto dir : iterate_range(GameSettings::MAP_DIR_COUNT)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        sequence.Frames.resize(collection.SequenceSize);

        auto dir_frm = dir;

        if constexpr (GameSettings::SQUARE_GEOMETRY) {
            if (dir >= 3) {
                dir_frm--;
            }
            if (dir >= 7) {
                dir_frm--;
            }
        }

        if (dir_frm != 0) {
            string next_fname = strex("{}{}", fname.substr(0, fname.size() - 1), '0' + dir_frm);

            dir_file = files.FindFileByPath(next_fname);

            if (dir_file) {
                reader = dir_file.GetReader();
            }
            else {
                if (dir > 1) {
                    throw ImageBakerException("FRX file not found", next_fname);
                }

                break;
            }
        }

        reader.SetCurPos(0xA + dir_frm * 2);
        sequence.OffsX = reader.GetBEInt16();
        reader.SetCurPos(0x16 + dir_frm * 2);
        sequence.OffsY = reader.GetBEInt16();

        reader.SetCurPos(0x22 + dir_frm * 4);
        auto offset = 0x3E + reader.GetBEUInt32();

        if (dir == 1) {
            collection.HaveDirs = true;
        }

        // Make palette
        auto* palette = reinterpret_cast<ucolor*>(FoPalette);
        ucolor custom_palette[256];
        File palette_file = files.FindFileByPath(strex("{}.pal", strex(fname).erase_file_extension()));

        if (palette_file) {
            auto palette_reader = palette_file.GetReader();

            for (auto& color : custom_palette) {
                uint8 r = palette_reader.GetUInt8() * 4;
                uint8 g = palette_reader.GetUInt8() * 4;
                uint8 b = palette_reader.GetUInt8() * 4;
                color = ucolor {r, g, b};
            }

            custom_palette[0] = ucolor {0};
            palette = custom_palette;
        }

        for (uint16 frm = 0; frm < frm_count; frm++) {
            auto& shot = sequence.Frames[frm];

            reader.SetCurPos(offset);
            auto w = reader.GetBEUInt16();
            auto h = reader.GetBEUInt16();

            shot.Width = w;
            shot.Height = h;

            reader.GoForward(4); // Frame size

            shot.NextX = reader.GetBEInt16();
            shot.NextY = reader.GetBEInt16();

            // Allocate data
            shot.Data.resize(numeric_cast<size_t>(w) * h * 4);
            auto* ptr = reinterpret_cast<ucolor*>(shot.Data.data());
            reader.SetCurPos(offset + 12);

            if (anim_pix_type == 0) {
                for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                    *(ptr + i) = palette[reader.GetUInt8()];
                }
            }
            else {
                for (size_t i = 0, j = numeric_cast<size_t>(w * h); i < j; i++) {
                    auto index = reader.GetUInt8();

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
                            *(ptr + i) = ucolor {blinking_red_vals[frm % 10], 0, 0};
                            continue;
                        }
                    }

                    *(ptr + i) = palette[index];
                }
            }

            // Check for animate pixels
            if (anim_pix_type == 0 && frm == 0 && dir == 0 && frm_count == 1 && palette == reinterpret_cast<ucolor*>(FoPalette)) {
                reader.SetCurPos(offset + 12);

                for (auto i = 0, j = w * h; i < j; i++) {
                    auto index = reader.GetUInt8();

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
                    int32 divs[4];
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

                    frm_count = 4;

                    for (auto i = 0; i < 4; i++) {
                        if (frm_count % divs[i] == 0) {
                            continue;
                        }

                        frm_count++;
                        i = -1;
                    }

                    collection.AnimTicks *= frm_count;
                    collection.SequenceSize = frm_count;
                    sequence.Frames.resize(collection.SequenceSize);
                }
            }

            if (anim_pix_type == 0) {
                offset += w * h + 12;
            }
        }
    }

    return collection;
}

auto ImageBaker::LoadRix(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(fname);
    ignore_unused(opt);
    ignore_unused(files);

    reader.SetCurPos(0x4);
    uint16 w = 0;
    reader.CopyData(&w, 2);
    uint16 h = 0;
    reader.CopyData(&h, 2);

    reader.SetCurPos(0xA);
    const auto* palette = reader.GetCurBuf();

    vector<uint8> data(numeric_cast<size_t>(w) * h * 4);
    auto* ptr = reinterpret_cast<ucolor*>(data.data());
    reader.SetCurPos(0xA + 256 * 3);

    for (auto i = 0, j = w * h; i < j; i++) {
        const auto index = numeric_cast<int32>(reader.GetUInt8()) * 3;
        const auto r = numeric_cast<uint8>(*(palette + index + 2) * 4);
        const auto g = numeric_cast<uint8>(*(palette + index + 1) * 4);
        const auto b = numeric_cast<uint8>(*(palette + index + 0) * 4);
        *(ptr + i) = ucolor {r, g, b};
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames.resize(collection.SequenceSize);
    collection.Main.Frames[0].Width = w;
    collection.Main.Frames[0].Height = h;
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadArt(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(files);

    auto palette_index = 0; // 0..3
    auto transparent = false;
    auto mirror_hor = false;
    auto mirror_ver = false;
    int32 frm_from = 0;
    int32 frm_to = 100000;

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
        int32 Flags {};
        // 0x00000001 = Static - no rotation, contains frames only for south direction.
        // 0x00000002 = Critter - uses delta attribute, while playing walking animation.
        // 0x00000004 = Font - X offset is equal to number of pixels to advance horizontally for next
        // character. 0x00000008 = Facade - requires fackwalk file. 0x00000010 = Unknown - used in eye candy,
        // for example, DIVINATION.art.
        int32 FrameRate {};
        int32 RotationCount {};
        uint32 PaletteList[4] {};
        int32 ActionFrame {};
        int32 FrameCount {};
        int32 InfoList[8] {};
        int32 SizeList[8] {};
        int32 DataList[8] {};
    } header;

    struct ArtFrameInfo
    {
        int32 FrameWidth {};
        int32 FrameHeight {};
        int32 FrameSize {};
        int32 OffsetX {};
        int32 OffsetY {};
        int32 DeltaX {};
        int32 DeltaY {};
    } frame_info;

    using ArtPalette = uint32[256];
    ArtPalette palette[4];

    reader.CopyData(&header, sizeof(header));

    if ((header.Flags & 0x00000001) != 0) {
        header.RotationCount = 1;
    }

    // Load palettes
    auto palette_count = 0;

    for (auto i = 0; i < 4; i++) {
        if (header.PaletteList[i] != 0) {
            reader.CopyData(&palette[i], sizeof(ArtPalette));
            palette_count++;
        }
    }
    if (palette_index >= palette_count) {
        palette_index = 0;
    }

    auto frm_fps = header.FrameRate;
    auto frm_count = header.FrameCount;
    FO_RUNTIME_ASSERT(frm_count > 0);

    if (frm_from >= frm_count) {
        frm_from = frm_count - 1;
    }
    if (frm_to >= frm_count) {
        frm_to = frm_count - 1;
    }

    auto frm_count_target = std::max(frm_from, frm_to) - std::min(frm_from, frm_to) + 1;

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = numeric_cast<uint16>(frm_count_target);
    collection.AnimTicks = numeric_cast<uint16>(frm_fps != 0 ? 1000 / frm_fps * frm_count_target : 0);
    collection.HaveDirs = header.RotationCount == 8;

    for (const auto dir : iterate_range(GameSettings::MAP_DIR_COUNT)) {
        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        sequence.Frames.resize(collection.SequenceSize);

        auto dir_art = dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
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
        int32 frm_write = 0;

        while (true) {
            reader.SetCurPos(sizeof(ArtHeader) + sizeof(ArtPalette) * palette_count + sizeof(ArtFrameInfo) * dir_art * frm_count + sizeof(ArtFrameInfo) * frm_read);

            reader.CopyData(&frame_info, sizeof(frame_info));

            auto w = frame_info.FrameWidth;
            auto h = frame_info.FrameHeight;
            vector<uint8> data(numeric_cast<size_t>(w) * h * 4);
            auto* ptr = reinterpret_cast<uint32*>(data.data());

            auto& shot = sequence.Frames[frm_write];
            shot.Width = numeric_cast<uint16>(w);
            shot.Height = numeric_cast<uint16>(h);
            shot.NextX = numeric_cast<int16>((-frame_info.OffsetX + frame_info.FrameWidth / 2) * (mirror_hor ? -1 : 1));
            shot.NextY = numeric_cast<int16>((-frame_info.OffsetY + frame_info.FrameHeight) * (mirror_ver ? -1 : 1));

            uint32 color = 0;

            auto art_get_color = [&color, &reader, &palette, &palette_index, &transparent]() {
                const auto index = reader.GetUInt8();
                color = palette[palette_index][index];
                std::swap(reinterpret_cast<uint8*>(&color)[0], reinterpret_cast<uint8*>(&color)[2]);

                if (index == 0) {
                    color = 0;
                }
                else if (transparent) {
                    color |= std::max((color >> 16) & 0xFF, std::max((color >> 8) & 0xFF, color & 0xFF)) << 24;
                }
                else {
                    color |= 0xFF00000;
                }
            };

            int32 pos = 0;
            int32 x = 0;
            int32 y = 0;
            bool mirror = mirror_hor || mirror_ver;

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
                for (int32 i = 0; i < frame_info.FrameSize; i++) {
                    art_get_color();
                    art_write_color();
                }
            }
            else {
                for (int32 i = 0; i < frame_info.FrameSize; i++) {
                    auto cmd = reader.GetUInt8();

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

auto ImageBaker::LoadSpr(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(files);

    FrameCollection collection;

    for (const auto dir : iterate_range(GameSettings::MAP_DIR_COUNT)) {
        auto dir_spr = dir;

        if constexpr (GameSettings::HEXAGONAL_GEOMETRY) {
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
        int32 rgb_offs[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

        // Format: fileName$[1,100,0,0][2,0,0,100]animName.spr
        auto first = opt.find_first_of('[');
        auto last = opt.find_last_of(']', first);
        auto seq_name = last != string::npos ? opt.substr(last + 1) : opt;

        while (first != string::npos && last != string::npos) {
            last = opt.find_first_of(']', first);
            auto entry = string(opt.substr(first, last - first - 1));
            istringstream ientry(entry);

            // Parse numbers
            int32 rgb[4];

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
        reader.CopyData(head, 11);

        if (head[8] != 0 || strcmp(head, "<sprite>") != 0) {
            throw ImageBakerException("Invalid SPR header", fname);
        }

        const auto dimension_left = numeric_cast<float32>(reader.GetUInt8()) * 6.7f;
        const auto dimension_up = numeric_cast<float32>(reader.GetUInt8()) * 7.6f;
        ignore_unused(dimension_up);
        const auto dimension_right = numeric_cast<float32>(reader.GetUInt8()) * 6.7f;
        const auto center_x = reader.GetLEInt32();
        const auto center_y = reader.GetLEInt32();
        reader.GoForward(2); // uint16 unknown1  sometimes it is 0, and sometimes it is 3
        reader.GoForward(1); // CHAR unknown2  0x64, other values were not observed

        auto ta = (127.0f / 2.0f) * DEG_TO_RAD_FLOAT; // Tactics grid angle
        auto center_x_ex = iround<int32>((dimension_left * sinf(ta) + dimension_right * sinf(ta)) / 2.0f - dimension_left * sinf(ta));
        auto center_y_ex = iround<int32>((dimension_left * cosf(ta) + dimension_right * cosf(ta)) / 2.0f);

        int32 anim_index = 0;
        vector<int32> anim_frames;
        anim_frames.reserve(1000);

        // Find sequence
        auto seq_founded = false;
        auto seq_cnt = reader.GetLEInt32();

        for (int32 seq = 0; seq < seq_cnt; seq++) {
            // Find by name
            auto item_cnt = reader.GetLEInt32();
            reader.GoForward(sizeof(int16) * item_cnt);
            reader.GoForward(sizeof(int32) * item_cnt);

            auto name_len = reader.GetLEInt32();
            auto name = string(reinterpret_cast<const char*>(reader.GetCurBuf()), name_len);
            reader.GoForward(name_len);
            auto index = reader.GetLEUInt16();

            if (seq_name.empty() || strex(seq_name).compare_ignore_case(name)) {
                anim_index = index;

                // Read frame numbers
                reader.GoBack(sizeof(uint16) + name_len + sizeof(int32) + sizeof(int32) * item_cnt + sizeof(int16) * item_cnt);

                for (int32 i = 0; i < item_cnt; i++) {
                    int16 val = reader.GetLEInt16();

                    if (val >= 0) {
                        anim_frames.push_back(val);
                    }
                    else if (val == -43) {
                        reader.GoForward(sizeof(int16) * 3);
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
        reader.SetCurPos(0);

        for (int32 i = 0; i <= anim_index; i++) {
            if (!reader.SeekFragment("<spranim>")) {
                throw ImageBakerException("Spranim for SPR not found", fname);
            }

            reader.GoForward(12);
        }

        auto file_offset = reader.GetLEInt32();
        reader.GoForward(reader.GetLEInt32()); // Collection name
        auto frame_cnt = reader.GetLEInt32();
        auto dir_cnt = reader.GetLEInt32();
        vector<int32> bboxes;
        bboxes.resize(numeric_cast<size_t>(frame_cnt) * dir_cnt * 4);
        reader.CopyData(bboxes.data(), sizeof(int32) * frame_cnt * dir_cnt * 4);

        // Fix dir
        if (dir_cnt != 8) {
            dir_spr = dir_spr * (dir_cnt * 100 / 8) / 100;
        }
        if (numeric_cast<int32>(dir_spr) >= dir_cnt) {
            dir_spr = 0;
        }

        // Check wrong frames
        for (size_t i = 0; i < anim_frames.size();) {
            if (anim_frames[i] >= frame_cnt) {
                anim_frames.erase(anim_frames.begin() + numeric_cast<ptrdiff_t>(i));
            }
            else {
                i++;
            }
        }

        // Get images file
        reader.SetCurPos(file_offset);
        reader.GoForward(14); // <spranim_img>\0
        auto type = reader.GetUInt8();
        reader.GoForward(1); // \0

        size_t data_len = 0;
        auto cur_pos = reader.GetCurPos();

        if (reader.SeekFragment("<spranim_img>")) {
            data_len = reader.GetCurPos() - cur_pos;
        }
        else {
            data_len = reader.GetSize() - cur_pos;
        }

        reader.SetCurPos(cur_pos);

        auto packed = type == 0x32;
        vector<uint8> data;

        if (packed) {
            // Unpack with zlib
            auto unpacked_len = reader.GetLEUInt32();
            auto unpacked_data = Compressor::Decompress({reader.GetCurBuf(), data_len}, unpacked_len / data_len + 1);

            if (unpacked_data.empty()) {
                throw ImageBakerException("Can't unpack SPR data", fname);
            }

            data = unpacked_data;
        }
        else {
            data.resize(data_len);
            MemCopy(data.data(), reader.GetCurBuf(), data_len);
        }

        auto fm_images = FileReader(data);

        // Read palette
        typedef ucolor Palette[256];
        Palette palette[4];

        for (auto& i : palette) {
            auto palette_count = fm_images.GetLEUInt32();

            if (palette_count <= 256) {
                fm_images.CopyData(&i, numeric_cast<size_t>(palette_count) * 4);
            }
        }

        // Index data offsets
        vector<size_t> image_indices;
        image_indices.resize(numeric_cast<size_t>(frame_cnt) * dir_cnt * 4);

        for (size_t cur = 0; fm_images.GetCurPos() != fm_images.GetSize();) {
            auto tag = fm_images.GetUInt8();

            if (tag == 1) {
                // Valid index
                image_indices[cur] = fm_images.GetCurPos();
                fm_images.GoForward(8 + 8 + 8 + 1);
                fm_images.GoForward(fm_images.GetLEUInt32());
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
            collection.SequenceSize = numeric_cast<uint16>(anim_frames.size());
            collection.AnimTicks = numeric_cast<uint16>(1000 / 10 * anim_frames.size());
        }

        if (dir == 1) {
            collection.HaveDirs = true;
        }

        auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
        sequence.Frames.resize(collection.SequenceSize);

        for (size_t anim_frame = 0; anim_frame < anim_frames.size(); anim_frame++) {
            auto& shot = sequence.Frames[anim_frame];
            auto frm = anim_frames[anim_frame];

            // Optimization, share frames
            bool founded = false;

            for (size_t i = 0, j = anim_frame; i < j; i++) {
                if (anim_frames[i] == frm) {
                    shot.Shared = true;
                    shot.SharedIndex = numeric_cast<uint16>(i);
                    founded = true;
                    break;
                }
            }

            if (founded) {
                continue;
            }

            // Compute whole image size
            int32 whole_width = 0;
            int32 whole_height = 0;

            for (int32 part = 0; part < 4; part++) {
                const int32 frm_index = type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : (frm * dir_cnt + (dir_spr << 2)) + part;

                if (image_indices[frm_index] == 0) {
                    continue;
                }

                fm_images.SetCurPos(image_indices[frm_index]);

                auto posx = fm_images.GetLEInt32();
                auto posy = fm_images.GetLEInt32();
                fm_images.GoForward(8);
                auto width = fm_images.GetLEInt32();
                auto height = fm_images.GetLEInt32();
                whole_width = std::max(width + posx, whole_width);
                whole_height = std::max(height + posy, whole_height);
            }

            if (whole_width == 0) {
                whole_width = 1;
            }
            if (whole_height == 0) {
                whole_height = 1;
            }

            // Allocate data
            vector<uint8> whole_data(numeric_cast<size_t>(whole_width) * whole_height * 4);

            for (int32 part = 0; part < 4; part++) {
                const int32 frm_index = type == 0x32 ? frame_cnt * dir_cnt * part + dir_spr * frame_cnt + frm : (frm * dir_cnt + (dir_spr << 2)) + part;

                if (image_indices[frm_index] == 0) {
                    continue;
                }

                fm_images.SetCurPos(image_indices[frm_index]);

                auto posx = fm_images.GetLEInt32();
                auto posy = fm_images.GetLEInt32();

                char zar[8] = {0};
                fm_images.CopyData(zar, 8);
                uint8 subtype = zar[6];

                auto width = fm_images.GetLEInt32();
                auto height = fm_images.GetLEInt32();
                ignore_unused(height);
                auto palette_present = fm_images.GetUInt8();
                auto rle_size = fm_images.GetLEUInt32();
                const auto* rle_buf = fm_images.GetCurBuf();
                fm_images.GoForward(rle_size);
                uint8 def_color = 0;

                if (zar[5] != 0 || strcmp(zar, "<zar>") != 0 || (subtype != 0x34 && subtype != 0x35) || palette_present == 1) {
                    throw ImageBakerException("Invalid SPR file innded ZAR header", fname);
                }

                auto* ptr = reinterpret_cast<ucolor*>(whole_data.data()) + numeric_cast<size_t>(posy) * whole_width + posx;
                auto x = posx;
                auto y = posy;

                while (rle_size != 0) {
                    int32 control = *rle_buf;
                    rle_buf++;
                    rle_size--;

                    auto control_mode = control & 3;
                    auto control_count = control >> 2;

                    for (int32 i = 0; i < control_count; i++) {
                        ucolor col;

                        switch (control_mode) {
                        case 1:
                            col = palette[part][rle_buf[i]];
                            reinterpret_cast<uint8*>(&col)[3] = 0xFF;
                            break;
                        case 2:
                            col = palette[part][rle_buf[numeric_cast<size_t>(2) * i]];
                            reinterpret_cast<uint8*>(&col)[3] = rle_buf[numeric_cast<size_t>(2) * i + 1];
                            break;
                        case 3:
                            col = palette[part][def_color];
                            reinterpret_cast<uint8*>(&col)[3] = rle_buf[i];
                            break;
                        default:
                            break;
                        }

                        for (int32 j = 0; j < 3; j++) {
                            if (rgb_offs[part][j] != 0) {
                                auto val = numeric_cast<int32>(reinterpret_cast<uint8*>(&col)[2 - j]) + rgb_offs[part][j];
                                reinterpret_cast<uint8*>(&col)[2 - j] = numeric_cast<uint8>(std::clamp(val, 0, 255));
                            }
                        }

                        std::swap(reinterpret_cast<uint8*>(&col)[0], reinterpret_cast<uint8*>(&col)[2]);

                        if (part == 0) {
                            *ptr = col;
                        }
                        else if (col.comp.a >= 128) {
                            *ptr = col;
                        }

                        ptr++;

                        if (++x >= width + posx) {
                            x = posx;
                            y++;
                            ptr = reinterpret_cast<ucolor*>(whole_data.data()) + numeric_cast<size_t>(y) * whole_width + x;
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

            shot.Width = numeric_cast<uint16>(whole_width);
            shot.Height = numeric_cast<uint16>(whole_height);
            shot.NextX = numeric_cast<int16>(bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 0] - center_x + center_x_ex + whole_width / 2);
            shot.NextY = numeric_cast<int16>(bboxes[frm * dir_cnt * 4 + dir_spr * 4 + 1] - center_y + center_y_ex + whole_height);
            shot.Data = std::move(whole_data);
        }

        if (dir_cnt == 1) {
            break;
        }
    }

    return collection;
}

auto ImageBaker::LoadZar(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);
    ignore_unused(files);

    // Read header
    char head[6];
    reader.CopyData(head, 6);

    if (head[5] != 0 || strcmp(head, "<zar>") != 0) {
        throw ImageBakerException("Invalid ZAR header", fname);
    }

    const auto type = reader.GetUInt8();
    reader.GoForward(1); // \0
    const auto width = reader.GetLEInt32();
    const auto height = reader.GetLEInt32();
    const auto palette_present = reader.GetUInt8();

    // Read palette
    uint32 palette[256] = {};
    uint8 def_color = 0;

    if (palette_present != 0) {
        const auto palette_count = reader.GetLEInt32();

        if (palette_count > 256) {
            throw ImageBakerException("Invalid ZAR palette count", fname);
        }

        reader.CopyData(palette, sizeof(uint32) * palette_count);

        if (type == 0x34) {
            def_color = reader.GetUInt8();
        }
    }

    // Read image
    auto rle_size = reader.GetLEUInt32();
    const auto* rle_buf = reader.GetCurBuf();
    reader.GoForward(rle_size);

    // Allocate data
    vector<uint8> data(numeric_cast<size_t>(width) * height * 4);
    auto* ptr = reinterpret_cast<uint32*>(data.data());

    // Decode
    while (rle_size != 0) {
        const auto control = *rle_buf;
        rle_buf++;
        rle_size--;

        const auto control_mode = control & 3;
        const auto control_count = control >> 2;

        for (auto i = 0; i < control_count; i++) {
            uint32 col = 0;

            switch (control_mode) {
            case 1:
                col = palette[rle_buf[i]];
                reinterpret_cast<uint8*>(&col)[3] = 0xFF;
                break;
            case 2:
                col = palette[rle_buf[numeric_cast<size_t>(2) * i]];
                reinterpret_cast<uint8*>(&col)[3] = rle_buf[numeric_cast<size_t>(2) * i + 1];
                break;
            case 3:
                col = palette[def_color];
                reinterpret_cast<uint8*>(&col)[3] = rle_buf[i];
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
    collection.Main.Frames.resize(collection.SequenceSize);
    collection.Main.Frames[0].Width = numeric_cast<uint16>(width);
    collection.Main.Frames[0].Height = numeric_cast<uint16>(height);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadTil(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);
    ignore_unused(files);

    // Read header
    char head[7];
    reader.CopyData(head, 7);
    if (head[6] != 0 || strcmp(head, "<tile>") != 0) {
        throw ImageBakerException("Invalid TIL file header", fname);
    }

    while (reader.GetUInt8() != 0) {
        // Nop
    }

    reader.GoForward(7 + 4); // Unknown

    const auto width = reader.GetLEInt32();
    ignore_unused(width);
    const auto height = reader.GetLEInt32();
    ignore_unused(height);

    if (!reader.SeekFragment("<tiledata>")) {
        throw ImageBakerException("Tiledata in TIL file not found", fname);
    }

    reader.GoForward(10 + 3); // Signature
    const auto frames_count = reader.GetLEInt32();

    FrameCollection collection;
    collection.SequenceSize = numeric_cast<uint16>(frames_count);
    collection.AnimTicks = numeric_cast<uint16>(1000 / 10 * frames_count);
    collection.Main.Frames.resize(collection.SequenceSize);

    for (int32 frm = 0; frm < frames_count; frm++) {
        // Read header
        char zar_head[6];
        reader.CopyData(zar_head, 6);

        if (zar_head[5] != 0 || strcmp(zar_head, "<zar>") != 0) {
            throw ImageBakerException("ZAR header in TIL file not found", fname);
        }

        const auto type = reader.GetUInt8();
        reader.GoForward(1); // \0
        const auto zar_width = reader.GetLEInt32();
        const auto zar_height = reader.GetLEInt32();
        const auto palette_present = reader.GetUInt8();

        // Read palette
        ucolor palette[256] = {};
        uint8 def_color = 0;

        if (palette_present != 0) {
            const auto palette_count = reader.GetLEUInt32();

            if (palette_count > 256) {
                throw ImageBakerException("TIL file invalid palettes", fname);
            }

            reader.CopyData(palette, sizeof(uint32) * palette_count);

            if (type == 0x34) {
                def_color = reader.GetUInt8();
            }
        }

        // Read image
        auto rle_size = reader.GetLEUInt32();
        const auto* rle_buf = reader.GetCurBuf();
        reader.GoForward(rle_size);

        // Allocate data
        vector<uint8> data(numeric_cast<size_t>(zar_width) * zar_height * 4);
        auto* ptr = reinterpret_cast<ucolor*>(data.data());

        // Decode
        while (rle_size != 0) {
            const int32 control = *rle_buf;
            rle_buf++;
            rle_size--;

            const auto control_mode = control & 3;
            const auto control_count = control >> 2;

            for (auto i = 0; i < control_count; i++) {
                ucolor col;

                switch (control_mode) {
                case 1:
                    col = palette[rle_buf[i]];
                    reinterpret_cast<uint8*>(&col)[3] = 0xFF;
                    break;
                case 2:
                    col = palette[rle_buf[numeric_cast<size_t>(2) * i]];
                    reinterpret_cast<uint8*>(&col)[3] = rle_buf[numeric_cast<size_t>(2) * i + 1];
                    break;
                case 3:
                    col = palette[def_color];
                    reinterpret_cast<uint8*>(&col)[3] = rle_buf[i];
                    break;
                default:
                    break;
                }

                std::swap(col.comp.r, col.comp.b);
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

        auto& shot = collection.Main.Frames[frm];
        shot.Width = numeric_cast<uint16>(zar_width);
        shot.Height = numeric_cast<uint16>(zar_height);
        shot.Data = std::move(data);
    }

    return collection;
}

auto ImageBaker::LoadMos(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(opt);
    ignore_unused(files);

    // Read signature
    char head[8];
    reader.CopyData(head, 8);

    if (!strex(head).starts_with("MOS")) {
        throw ImageBakerException("Invalid MOS file header", fname);
    }

    // Packed
    vector<uint8> unpacked_data;

    if (head[3] == 'C') {
        const auto unpacked_len = reader.GetLEUInt32();
        auto data_len = reader.GetSize() - 12;

        auto* buf = const_cast<uint8*>(reader.GetBuf());
        *reinterpret_cast<uint16*>(buf) = 0x9C78;

        unpacked_data = Compressor::Decompress({buf, data_len}, unpacked_len / reader.GetSize() + 1);

        if (unpacked_data.empty()) {
            throw ImageBakerException("Can't unpack MOS file", fname);
        }

        reader = FileReader(unpacked_data);
        reader.CopyData(head, 8);

        if (!strex(head).starts_with("MOS")) {
            throw ImageBakerException("Invalid MOS file unpacked header", fname);
        }
    }

    // Read header
    const int32 width = reader.GetLEUInt16(); // Width (pixels)
    const int32 height = reader.GetLEUInt16(); // Height (pixels)
    const int32 col = reader.GetLEUInt16(); // Columns (blocks)
    const int32 row = reader.GetLEUInt16(); // Rows (blocks)
    const auto block_size = reader.GetLEUInt32(); // Block size (pixels)
    ignore_unused(block_size);
    const auto palette_offset = reader.GetLEUInt32(); // Offset (from start of file) to palettes
    const auto tiles_offset = palette_offset + col * row * 256 * 4;
    const auto data_offset = tiles_offset + col * row * 4;

    // Allocate data
    vector<uint8> data(numeric_cast<size_t>(width) * height * 4);
    auto* ptr = reinterpret_cast<uint32*>(data.data());

    // Read image data
    uint32 palette[256] = {0};
    int32 block = 0;

    for (int32 y = 0; y < row; y++) {
        for (int32 x = 0; x < col; x++) {
            // Get palette for current block
            reader.SetCurPos(palette_offset + block * 256 * 4);
            reader.CopyData(palette, numeric_cast<size_t>(256) * 4);

            // Set initial position
            reader.SetCurPos(tiles_offset + block * 4);
            reader.SetCurPos(data_offset + reader.GetLEUInt32());

            // Calculate current block size
            auto block_w = x == col - 1 ? width % 64 : 64;
            auto block_h = y == row - 1 ? height % 64 : 64;

            if (block_w == 0) {
                block_w = 64;
            }
            if (block_h == 0) {
                block_h = 64;
            }

            // Read data
            auto pos = y * 64 * width + x * 64;
            for (int32 yy = 0; yy < block_h; yy++) {
                for (int32 xx = 0; xx < block_w; xx++) {
                    auto color = palette[reader.GetUInt8()];

                    if (color == 0xFF00) {
                        color = 0; // Green is transparent
                    }
                    else {
                        color |= 0xFF000000;
                    }

                    *(ptr + pos) = color;
                    pos++;
                }

                pos += width - block_w;
            }

            // Go to next block
            block++;
        }
    }

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames.resize(collection.SequenceSize);
    collection.Main.Frames[0].Width = numeric_cast<uint16>(width);
    collection.Main.Frames[0].Height = numeric_cast<uint16>(height);
    collection.Main.Frames[0].Data = std::move(data);
    return collection;
}

auto ImageBaker::LoadBam(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(files);

    // Format: fileName$5-6.spr
    auto opt_str = string(opt);
    istringstream idelim(opt_str);
    int32 need_cycle = 0;
    int32 specific_frame = -1;
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
    reader.CopyData(head, 8);

    if (!strex(head).starts_with("BAM")) {
        throw ImageBakerException("Invalid BAM file header", fname);
    }

    // Packed
    vector<uint8> unpacked_data;

    if (head[3] == 'C') {
        auto unpacked_len = reader.GetLEUInt32();
        auto data_len = reader.GetSize() - 12;

        auto* buf = const_cast<uint8*>(reader.GetBuf());
        *reinterpret_cast<uint16*>(buf) = 0x9C78;

        unpacked_data = Compressor::Decompress({buf, data_len}, unpacked_len / reader.GetSize() + 1);

        if (unpacked_data.empty()) {
            throw ImageBakerException("Cab't unpack BAM file", fname);
        }

        reader = FileReader(unpacked_data);
        reader.CopyData(head, 8);

        if (!strex(head).starts_with("BAM")) {
            throw ImageBakerException("Invalid BAM file unpacked header", fname);
        }
    }

    // Read header
    int32 frames_count = reader.GetLEUInt16();
    int32 cycles_count = reader.GetUInt8();
    auto compr_color = reader.GetUInt8();
    auto frames_offset = reader.GetLEUInt32();
    auto palette_offset = reader.GetLEUInt32();
    auto lookup_table_offset = reader.GetLEUInt32();

    // Count whole frames
    if (need_cycle >= cycles_count) {
        need_cycle = 0;
    }

    reader.SetCurPos(frames_offset + frames_count * 12 + need_cycle * 4);
    int32 cycle_frames = reader.GetLEUInt16();
    int32 table_index = reader.GetLEUInt16();

    if (specific_frame != -1 && specific_frame >= numeric_cast<int32>(cycle_frames)) {
        specific_frame = 0;
    }

    // Create animation
    FrameCollection collection;
    collection.SequenceSize = numeric_cast<uint16>(specific_frame == -1 ? cycle_frames : 1);
    collection.AnimTicks = 100;
    collection.Main.Frames.resize(collection.SequenceSize);

    // Palette
    ucolor palette[256] = {};
    reader.SetCurPos(palette_offset);
    reader.CopyData(palette, numeric_cast<size_t>(256) * 4);

    // Find in lookup table
    for (int32 i = 0; i < cycle_frames; i++) {
        if (specific_frame != -1 && specific_frame != numeric_cast<int32>(i)) {
            continue;
        }

        // Get need index
        reader.SetCurPos(lookup_table_offset + table_index * 2);
        table_index++;
        int32 frame_index = reader.GetLEUInt16();

        // Get frame data
        reader.SetCurPos(frames_offset + frame_index * 12);
        int32 width = reader.GetLEUInt16();
        int32 height = reader.GetLEUInt16();
        int32 ox = reader.GetLEUInt16();
        int32 oy = reader.GetLEUInt16();
        auto data_offset = reader.GetLEUInt32();
        auto rle = (data_offset & 0x80000000) == 0;
        data_offset &= 0x7FFFFFFF;

        // Allocate data
        vector<uint8> data(numeric_cast<size_t>(width) * height * 4);
        auto* ptr = reinterpret_cast<ucolor*>(data.data());

        // Fill it
        reader.SetCurPos(data_offset);

        for (int32 k = 0, l = width * height; k < l;) {
            auto index = reader.GetUInt8();
            auto color = palette[index];

            if (color.comp.b == 255) {
                color = ucolor::clear;
            }
            else {
                color.comp.a = 255;
            }

            std::swap(color.comp.r, color.comp.b);

            if (rle && index == compr_color) {
                int32 copies = reader.GetUInt8();

                for (int32 m = 0; m <= copies; m++, k++) {
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
        shot.Width = numeric_cast<uint16>(width);
        shot.Height = numeric_cast<uint16>(height);
        shot.NextX = numeric_cast<int16>(-ox + width / 2);
        shot.NextY = numeric_cast<int16>(-oy + height);
        shot.Data = std::move(data);

        if (specific_frame != -1) {
            break;
        }
    }

    return collection;
}

auto ImageBaker::LoadPng(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(fname);
    ignore_unused(opt);
    ignore_unused(files);

    int32 width = 0;
    int32 height = 0;
    auto data = PngLoad(reader.GetBuf(), width, height);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames.resize(collection.SequenceSize);
    collection.Main.Frames[0].Width = numeric_cast<uint16>(width);
    collection.Main.Frames[0].Height = numeric_cast<uint16>(height);
    collection.Main.Frames[0].Data = std::move(data);

    return collection;
}

auto ImageBaker::LoadTga(string_view fname, string_view opt, FileReader reader, const FileCollection& files) const -> FrameCollection
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(fname);
    ignore_unused(opt);
    ignore_unused(files);

    int32 width = 0;
    int32 height = 0;
    auto data = TgaLoad(reader.GetBuf(), reader.GetSize(), width, height);

    FrameCollection collection;
    collection.SequenceSize = 1;
    collection.AnimTicks = 100;
    collection.Main.Frames.resize(collection.SequenceSize);
    collection.Main.Frames[0].Width = numeric_cast<uint16>(width);
    collection.Main.Frames[0].Height = numeric_cast<uint16>(height);
    collection.Main.Frames[0].Data = std::move(data);

    return collection;
}

static auto PngLoad(const uint8* data, int32& result_width, int32& result_height) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    auto* png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    FO_RUNTIME_ASSERT(png_ptr);

    try {
        struct PngMessage
        {
            [[noreturn]] static void Error(png_structp png_ptr, png_const_charp error_msg)
            {
                ignore_unused(png_ptr);
                throw ImageBakerException("PNG loading error", error_msg);
            }

            static void Warning(png_structp png_ptr, png_const_charp error_msg)
            {
                ignore_unused(png_ptr);
                ignore_unused(error_msg);
                // WriteLog("PNG loading warning: {}", error_msg);
            }
        };

        png_set_error_fn(png_ptr, png_get_error_ptr(png_ptr), &PngMessage::Error, &PngMessage::Warning);

        auto* info_ptr = png_create_info_struct(png_ptr);
        FO_RUNTIME_ASSERT(info_ptr);

        struct PngReader
        {
            static void Read(png_structp png_ptr, png_bytep png_data, png_size_t length)
            {
                auto** io_ptr = static_cast<uint8**>(png_get_io_ptr(png_ptr));
                MemCopy(png_data, *io_ptr, length);
                *io_ptr += length;
            }
        };

        // Get info
        png_set_read_fn(png_ptr, static_cast<png_voidp>(&data), &PngReader::Read);
        png_read_info(png_ptr, info_ptr);

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
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0) {
            png_set_expand(png_ptr);
        }

        png_set_filler(png_ptr, 0x000000ff, PNG_FILLER_AFTER);
        png_read_update_info(png_ptr, info_ptr);

        // Read
        vector<png_bytep> row_pointers;
        row_pointers.resize(height);

        vector<uint8> result;
        result.resize(numeric_cast<size_t>(width) * height * 4);

        for (png_uint_32 i = 0; i < height; i++) {
            row_pointers[i] = result.data() + numeric_cast<size_t>(i) * width * 4;
        }

        png_read_image(png_ptr, row_pointers.data());
        png_read_end(png_ptr, info_ptr);
        png_destroy_read_struct(&png_ptr, &info_ptr, static_cast<png_infopp>(nullptr));

        result_width = numeric_cast<int32>(width);
        result_height = numeric_cast<int32>(height);

        return result;
    }
    catch (...) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);

        throw;
    }
}

static auto TgaLoad(const uint8* data, size_t data_size, int32& result_width, int32& result_height) -> vector<uint8>
{
    FO_STACK_TRACE_ENTRY();

    size_t cur_pos = 0;

    const auto read_tga = [&](void* ptr, size_t len) {
        if (cur_pos + len <= data_size) {
            MemCopy(ptr, data + cur_pos, len);
            cur_pos += (len);
        }
        else {
            throw ImageBakerException("TGA reading error");
        }
    };

    // Load header
    uint8 type = 0;
    uint8 pixel_depth = 0;
    int16 width = 0;
    int16 height = 0;
    uint8 unused_char = 0;
    int16 unused_short = 0;
    read_tga(&unused_char, 1);
    read_tga(&unused_char, 1);
    read_tga(&type, 1);
    read_tga(&unused_short, 2);
    read_tga(&unused_short, 2);
    read_tga(&unused_char, 1);
    read_tga(&unused_short, 2);
    read_tga(&unused_short, 2);
    read_tga(&width, 2);
    read_tga(&height, 2);
    read_tga(&pixel_depth, 1);
    read_tga(&unused_char, 1);

    // Check if the image is color indexed
    if (type == 1) {
        throw ImageBakerException("Indexed TGA is not supported");
    }

    // Check for TrueColor
    if (type != 2 && type != 10) {
        throw ImageBakerException("TGA TrueColor is not supported");
    }

    // Check for RGB(A)
    if (pixel_depth != 24 && pixel_depth != 32) {
        throw ImageBakerException("TGA support only 24 and 32 bpp");
    }

    // Read
    const auto bpp = pixel_depth / 8;
    const int32 read_size = height * width * bpp;
    vector<uint8> read_data;
    read_data.resize(read_size);

    if (type == 2) {
        read_tga(read_data.data(), read_size);
    }
    else {
        int32 bytes_read = 0;
        uint8 header = 0;
        uint8 color[4];

        while (bytes_read < read_size) {
            read_tga(&header, 1);

            if ((header & 0x00000080) != 0) {
                header &= ~0x00000080;
                read_tga(color, bpp);
                const int32 run_len = (header + 1) * bpp;

                for (int32 i = 0; i < run_len; i += bpp) {
                    for (auto c = 0; c < bpp && bytes_read + i + c < read_size; c++) {
                        read_data[bytes_read + i + c] = color[c];
                    }
                }

                bytes_read += run_len;
            }
            else {
                const int32 run_len = (header + 1) * bpp;
                int32 to_read;

                if (bytes_read + run_len > read_size) {
                    to_read = read_size - bytes_read;
                }
                else {
                    to_read = run_len;
                }

                read_tga(read_data.data() + bytes_read, to_read);
                bytes_read += run_len;

                if (bytes_read + run_len > read_size) {
                    cur_pos += run_len - to_read;
                }
            }
        }
    }

    // Copy data
    vector<uint8> result;
    result.resize(numeric_cast<size_t>(width) * height * 4);

    for (int16 y = 0; y < height; y++) {
        for (int16 x = 0; x < width; x++) {
            const auto i = (height - y - 1) * width + x;
            const auto j = y * width + x;
            result[i * 4 + 0] = read_data[j * bpp + 2];
            result[i * 4 + 1] = read_data[j * bpp + 1];
            result[i * 4 + 2] = read_data[j * bpp + 0];
            result[i * 4 + 3] = bpp == 4 ? read_data[j * bpp + 3] : 0xFF;
        }
    }

    // Return data
    result_width = width;
    result_height = height;

    return result;
}

FO_END_NAMESPACE();
