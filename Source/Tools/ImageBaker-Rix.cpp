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

#include "ImageBaker-Rix.h"

#if FO_IMAGE_RIX

FO_BEGIN_NAMESPACE

namespace ImageBakerRix
{
    using ImageImport::FrameCollection;
    using ImageImport::FrameSequence;
    using ImageImport::FrameShot;

    auto Load(string_view fname, string_view opt, FileReader reader, const ImageImport::Context& context) -> FrameCollection
    {
        FO_TRACE_ZONE(Baking);

        ignore_unused(fname);
        ignore_unused(opt);
        ignore_unused(context);

        reader.SetCurPos(0x4);
        uint16_t w = 0;
        reader.ReadObject(w);
        uint16_t h = 0;
        reader.ReadObject(h);

        reader.SetCurPos(0xA);
        const_span<uint8_t> palette = reader.GetCurDataSpan(256 * 3);

        vector<uint8_t> data(numeric_cast<size_t>(w) * h * 4);
        span<ucolor> pixels = bytes_to_objects<ucolor>(span {data});
        reader.SetCurPos(0xA + 256 * 3);

        for (int32_t i = 0; i < w * h; i++) {
            int32_t index = numeric_cast<int32_t>(reader.GetUInt8()) * 3;
            auto r = numeric_cast<uint8_t>(palette[index + 2] * 4);
            auto g = numeric_cast<uint8_t>(palette[index + 1] * 4);
            auto b = numeric_cast<uint8_t>(palette[index + 0] * 4);
            pixels[i] = ucolor {r, g, b};
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
}

FO_END_NAMESPACE

#endif
