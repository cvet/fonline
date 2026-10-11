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

#include "ImageBaker-Fofrm.h"

#include "ConfigFile.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

namespace ImageBakerFofrm
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(opt);

        FrameCollection collection;

        ConfigFile fofrm(reader.GetStr());

        if (string alias {fofrm.GetAsStr("", "Alias")}; !alias.empty()) {
            FO_VERIFY_AND_THROW(strex(alias).get_file_extension() != "fofrm", "FOFRM Alias must reference an image source", fname, alias);

            FrameCollection aliased = context.Resolve(strex(fname).extract_dir().combine_path(alias).str());
            aliased.NewName = string {fname};
            return aliased;
        }

        int32_t frm_fps = fofrm.GetAsInt("", "fps", 10);
        frm_fps = fofrm.GetAsInt("", "Fps", frm_fps);
        int32_t frm_count = fofrm.GetAsInt("", "count", 1);
        frm_count = fofrm.GetAsInt("", "Count", frm_count);
        FO_VERIFY_AND_THROW(frm_count > 0, "Frame count must be positive", frm_count);

        int32_t ox = fofrm.GetAsInt("", "offs_x", 0);
        ox = fofrm.GetAsInt("", "OffsetX", ox);
        int32_t oy = fofrm.GetAsInt("", "offs_y", 0);
        oy = fofrm.GetAsInt("", "OffsetY", oy);

        collection.EffectName = fofrm.GetAsStr("", "effect");
        collection.EffectName = fofrm.GetAsStr("", "Effect", collection.EffectName);

        for (int32_t dir = 0; dir < GameSettings::MAP_DIR_COUNT; dir++) {
            vector<tuple<FrameCollection, int32_t, int32_t>> sub_collections;
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

            int32_t frames = 0;
            bool no_info = false;
            bool load_fail = false;

            for (int32_t frm = 0; frm < frm_count; frm++) {
                string_view frm_name = fofrm.GetAsStr(dir_str, strex("frm_{}", frm));

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

                FrameCollection sub_collection;

                try {
                    sub_collection = context.Resolve(strex(frm_dir).combine_path(frm_name).str());
                }
                catch (const std::exception& ex) {
                    throw ImageBakerException("FOFRM nested image loading failed", fname, frm_name, ex.what());
                }

                frames += sub_collection.SequenceSize;

                int32_t next_x = fofrm.GetAsInt(dir_str, strex("next_x_{}", frm), 0);
                next_x = fofrm.GetAsInt(dir_str, strex("NextX_{}", frm), next_x);
                int32_t next_y = fofrm.GetAsInt(dir_str, strex("next_y_{}", frm), 0);
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
                collection.SequenceSize = numeric_cast<uint16_t>(frames);
                collection.AnimTicks = numeric_cast<uint16_t>(frm_fps != 0 ? 1000 * frm_count / frm_fps : 0);
            }
            else {
                collection.HaveDirs = true;
            }

            auto& sequence = dir == 0 ? collection.Main : collection.Dirs[dir - 1];
            sequence.OffsX = numeric_cast<int16_t>(ox);
            sequence.OffsY = numeric_cast<int16_t>(oy);
            sequence.Frames.resize(collection.SequenceSize);

            // Merge many animations in one
            int32_t frm = 0;

            for (auto&& [sub_collection, next_x, next_y] : sub_collections) {
                for (int32_t i = 0; i < sub_collection.SequenceSize; i++, frm++) {
                    auto& shot = sequence.Frames[frm];

                    if (sub_collection.Main.Frames[i].Shared) {
                        throw ImageBakerException("FOFRM file invalid data (shared index)", fname);
                    }

                    shot.Width = sub_collection.Main.Frames[i].Width;
                    shot.Height = sub_collection.Main.Frames[i].Height;
                    shot.Data = sub_collection.Main.Frames[i].Data;
                    shot.NextX = numeric_cast<int16_t>(sub_collection.Main.Frames[i].NextX + next_x);
                    shot.NextY = numeric_cast<int16_t>(sub_collection.Main.Frames[i].NextY + next_y);
                }
            }
        }

        return collection;
    }
}

FO_END_NAMESPACE
