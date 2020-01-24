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
#include "GraphicStructures.h"
#include "Settings.h"

struct EffectCollection : public NonCopyable
{
    Effect* Contour {};
    Effect* ContourDefault {};
    Effect* Generic {};
    Effect* GenericDefault {};
    Effect* Critter {};
    Effect* CritterDefault {};
    Effect* Tile {};
    Effect* TileDefault {};
    Effect* Roof {};
    Effect* RoofDefault {};
    Effect* Rain {};
    Effect* RainDefault {};
    Effect* Iface {};
    Effect* IfaceDefault {};
    Effect* Primitive {};
    Effect* PrimitiveDefault {};
    Effect* Light {};
    Effect* LightDefault {};
    Effect* Fog {};
    Effect* FogDefault {};
    Effect* FlushRenderTarget {};
    Effect* FlushRenderTargetDefault {};
    Effect* FlushRenderTargetMS {};
    Effect* FlushRenderTargetMSDefault {};
    Effect* FlushPrimitive {};
    Effect* FlushPrimitiveDefault {};
    Effect* FlushMap {};
    Effect* FlushMapDefault {};
    Effect* FlushLight {};
    Effect* FlushLightDefault {};
    Effect* FlushFog {};
    Effect* FlushFogDefault {};
    Effect* Font {};
    Effect* FontDefault {};
    Effect* Skinned3d {};
    Effect* Skinned3dDefault {};
};

class EffectManager : public NonCopyable
{
public:
    EffectManager(EffectSettings& sett, FileManager& file_mngr);
    Effect* LoadEffect(const string& effect_name, bool use_in_2d, const string& defines = "",
        const string& model_path = "", EffectDefault* defaults = nullptr, uint defaults_count = 0);
    void EffectProcessVariables(EffectPass& effect_pass, bool start, float anim_proc = 0.0f, float anim_time = 0.0f,
        MeshTexture** textures = nullptr);
    bool LoadMinimalEffects();
    bool LoadDefaultEffects();
    bool Load3dEffects();

    EffectCollection Effects {};
    uint MaxBones {};

private:
    bool LoadEffectPass(Effect* effect, const string& fname, File& file, uint pass, bool use_in_2d,
        const string& defines, EffectDefault* defaults, uint defaults_count);

    EffectSettings& settings;
    FileManager& fileMngr;
    vector<unique_ptr<Effect>> loadedEffects {};
};
