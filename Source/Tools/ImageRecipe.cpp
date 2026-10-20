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

#include "ImageRecipe.h"
#include "AnyData.h"
#include "ImageImport.h"
#include "Settings.h"

FO_BEGIN_NAMESPACE

auto ParseImageRecipe(string_view fname, string_view text) -> ImageRecipe
{
    ImageRecipe recipe {ConfigFile {string {text}}};
    recipe.References.emplace(string {}, vector<string> {});
    const ConfigFile& fofrm = recipe.Values;

    auto is_indexed_key = [](string_view key, string_view prefix) {
        if (!key.starts_with(prefix)) {
            return false;
        }

        string_view index = key.substr(prefix.size());

        return !index.empty() && std::all_of(index.begin(), index.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
    };

    string_view direction_prefix = FO_GEOMETRY == 1 ? "HexDir" : "SquareDir";

    for (const auto& [section, entries] : fofrm.GetOrderedSections()) {
        if (section.starts_with("Dir_") || section.starts_with("dir_") || section.starts_with("DIR_") || section.starts_with("Dir ")) {
            FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");
            throw ImageBakerException("Legacy FOFRM section; migrate direction zero to the unnamed section and use HexDir/SquareDir indexes", fname, section);
        }

        bool valid_section = section.empty();

        for (int32_t dir = 1; !valid_section && dir < GameSettings::MAP_DIR_COUNT; dir++) {
            valid_section = section == strex("{} {}", direction_prefix, dir).str();
        }

        if (!valid_section) {
            throw ImageBakerException("FOFRM section requires HexDir/SquareDir and a supported nonzero index; direction zero is unnamed", fname, section);
        }

        for (const auto& [key, value] : *entries) {
            ignore_unused(value);

            if (key == "Alias") {
                FO_TEMPORARY_COMPAT(FofrmWholeCollectionSource, "2026-12-31");
                throw ImageBakerException("FOFRM Alias was replaced by Source", fname, section);
            }

            if (key == "Effect") {
                FO_TEMPORARY_COMPAT(FofrmRetiredEffect, "2026-12-31");
                throw ImageBakerException("FOFRM Effect was removed; select the effect at the resource consumer", fname, section);
            }

            if (key == "Fps" && !section.empty()) {
                throw ImageBakerException("FOFRM Fps is allowed only in the unnamed section", fname, section);
            }

            if (key == "Count" || key == "OffsetX" || key == "OffsetY" || key == "Frm" || key == "fps" || key == "count" || key == "offs_x" || key == "offs_y" || key == "effect" || key == "frm" || key == "alias" || key == "offsetX" || key == "offsetY" || is_indexed_key(key, "Frm_") || is_indexed_key(key, "frm_") || is_indexed_key(key, "next_x_") || is_indexed_key(key, "next_y_") || is_indexed_key(key, "nextX_") || is_indexed_key(key, "nextY_") || is_indexed_key(key, "NextX_") || is_indexed_key(key, "NextY_")) {
                FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31");
                throw ImageBakerException("Legacy FOFRM key; migrate to Frame, Frames, Offset and FrameOffsets", fname, section, key);
            }

            if (key != "Duration" && key != "FrameDurations" && key != "Resample" && key != "PixelUsage" && key != "ColorSpace" && key != "Flip" && key != "FrameFlips" && key != "Rotate" && key != "FrameRotations" && key != "Canvas" && key != "FrameCanvases" && key != "CanvasOffset" && key != "FrameCanvasOffsets" && key != "Move" && key != "FrameMoves" && key != "InputDir" && key != "FrameInputDirs" && key != "FrameIndices" && key != "Crop" && key != "FrameCrops" && key != "Grid" && key != "Source" && key != "Fps" && key != "Offset" && key != "FrameOffsets" && key != "Size" && key != "FrameSizes" && key != "Frame" && key != "Frames") {
                throw ImageBakerException("FOFRM key requires a supported PascalCase name", fname, section, key);
            }
        }
    }

    bool has_source = fofrm.HasKey("", "Source");

    if (has_source) {
        recipe.Source = string {fofrm.GetAsStr("", "Source")};
        FO_VERIFY_AND_THROW(!recipe.Source.empty(), "FOFRM Source must be nonempty", fname);
    }

    for (const auto& [section, entries] : fofrm.GetOrderedSections()) {
        ignore_unused(entries);

        int32_t timeline_forms = (fofrm.HasKey(section, "Duration") ? 1 : 0) + (fofrm.HasKey(section, "FrameDurations") ? 1 : 0) + (fofrm.HasKey(section, "Fps") ? 1 : 0);
        FO_VERIFY_AND_THROW(timeline_forms <= 1, "FOFRM Duration, FrameDurations and Fps are mutually exclusive in one section", fname, section);

        bool has_frame = fofrm.HasKey(section, "Frame");
        bool has_frames = fofrm.HasKey(section, "Frames");
        FO_VERIFY_AND_THROW(section.empty() || !fofrm.HasKey(section, "Source"), "FOFRM Source is allowed only in the unnamed section", fname, section);
        FO_VERIFY_AND_THROW(!has_source || !(has_frame || has_frames), "FOFRM Source and Frame/Frames are mutually exclusive", fname, section);
        FO_VERIFY_AND_THROW(!(has_frame && has_frames), "FOFRM Frame and Frames are mutually exclusive", fname, section);

        vector<string> references;

        if (has_frame) {
            string_view reference = fofrm.GetAsStr(section, "Frame");

            if (!reference.empty()) {
                references.emplace_back(reference);
            }
        }
        else if (has_frames) {
            auto parsed = AnyData::ParseValue(string {fofrm.GetAsStr(section, "Frames")}, false, true, AnyData::ValueType::String);

            for (const auto& reference : parsed.AsArray()) {
                references.emplace_back(reference.AsString());
            }
        }

        FO_VERIFY_AND_THROW(!(has_frame || has_frames) || !references.empty(), "FOFRM Frame or Frames must be nonempty", fname, section);

        recipe.References.insert_or_assign(string {section}, std::move(references));
    }

    if (!has_source && recipe.References.size() > 1) {
        for (int32_t direction = 1; direction < GameSettings::MAP_DIR_COUNT; direction++) {
            string section = strex("{} {}", direction_prefix, direction).str();
            FO_VERIFY_AND_THROW(fofrm.HasSection(section) && !recipe.References.at(section).empty(), "FOFRM composition requires a complete direction table", fname, section);
        }
    }

    return recipe;
}

FO_END_NAMESPACE
