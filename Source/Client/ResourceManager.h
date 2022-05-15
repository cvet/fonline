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

#pragma once

#include "Common.h"

#include "3dStuff.h"
#include "SpriteManager.h"

class ResourceManager final
{
public:
    ResourceManager() = delete;
    ResourceManager(FileSystem& file_sys, SpriteManager& spr_mngr, AnimationResolver& anim_name_resolver, NameResolver& name_resolver);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = delete;
    auto operator=(const ResourceManager&) = delete;
    auto operator=(ResourceManager&&) noexcept = delete;
    ~ResourceManager() = default;

    [[nodiscard]] auto GetAnim(hstring name, AtlasType atlas_type) -> AnyFrames*;
    [[nodiscard]] auto GetIfaceAnim(hstring name) -> AnyFrames* { return GetAnim(name, AtlasType::Static); }
    [[nodiscard]] auto GetInvAnim(hstring name) -> AnyFrames* { return GetAnim(name, AtlasType::Static); }
    [[nodiscard]] auto GetSkDxAnim(hstring name) -> AnyFrames* { return GetAnim(name, AtlasType::Static); }
    [[nodiscard]] auto GetItemAnim(hstring name) -> AnyFrames* { return GetAnim(name, AtlasType::Dynamic); }
    [[nodiscard]] auto GetCritterAnim(hstring model_name, uint anim1, uint anim2, uchar dir) -> AnyFrames*;
    [[nodiscard]] auto GetCritterModel(hstring model_name, uint anim1, uint anim2, uchar dir, int* layers3d) -> ModelInstance*;
    [[nodiscard]] auto GetCritterSprId(hstring model_name, uint anim1, uint anim2, uchar dir, int* layers3d) -> uint;
    [[nodiscard]] auto GetRandomSplash() -> AnyFrames*;
    [[nodiscard]] auto GetSoundNames() -> map<string, string>& { return _soundNames; }

    void FreeResources(AtlasType atlas_type);
    void ReinitializeDynamicAtlas();

    AnyFrames* ItemHexDefaultAnim {};
    AnyFrames* CritterDefaultAnim {};

private:
    struct LoadedAnim
    {
        AtlasType ResType {};
        AnyFrames* Anim {};
    };

    [[nodiscard]] auto LoadFalloutAnim(hstring model_name, uint anim1, uint anim2) -> AnyFrames*;
    [[nodiscard]] auto LoadFalloutAnimSpr(hstring model_name, uint anim1, uint anim2) -> AnyFrames*;

    void FixAnimOffs(AnyFrames* frames_base, AnyFrames* stay_frm_base);
    void FixAnimOffsNext(AnyFrames* frames_base, AnyFrames* stay_frm_base);

    FileSystem& _fileSys;
    SpriteManager& _sprMngr;
    AnimationResolver& _animNameResolver;
    NameResolver& _nameResolver;
    map<uint, string> _namesHash {};
    map<hstring, LoadedAnim> _loadedAnims {};
    map<uint, AnyFrames*> _critterFrames {};
    map<hstring, ModelInstance*> _critterModels {};
    vector<string> _splashNames {};
    map<string, string> _soundNames {};
    AnyFrames* _splash {};
    bool _nonConstHelper {};
};
