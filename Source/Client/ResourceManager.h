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

#include "3dStuff.h"
#include "ClientScripting.h"
#include "SpriteManager.h"

class ResourceManager final
{
public:
    ResourceManager() = delete;
    ResourceManager(FileManager& file_mngr, SpriteManager& spr_mngr, ClientScriptSystem& script_sys);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = delete;
    auto operator=(const ResourceManager&) = delete;
    auto operator=(ResourceManager&&) noexcept = delete;
    ~ResourceManager() = default;

    void FreeResources(AtlasType atlas_type);
    void ReinitializeDynamicAtlas();
    [[nodiscard]] auto GetAnim(hash name_hash, AtlasType atlas_type) -> AnyFrames*;
    [[nodiscard]] auto GetIfaceAnim(hash name_hash) -> AnyFrames* { return GetAnim(name_hash, AtlasType::Static); }
    [[nodiscard]] auto GetInvAnim(hash name_hash) -> AnyFrames* { return GetAnim(name_hash, AtlasType::Static); }
    [[nodiscard]] auto GetSkDxAnim(hash name_hash) -> AnyFrames* { return GetAnim(name_hash, AtlasType::Static); }
    [[nodiscard]] auto GetItemAnim(hash name_hash) -> AnyFrames* { return GetAnim(name_hash, AtlasType::Dynamic); }
    [[nodiscard]] auto GetCrit2dAnim(hash model_name, uint anim1, uint anim2, int dir) -> AnyFrames*;
    [[nodiscard]] auto GetCrit3dAnim(hash model_name, uint anim1, uint anim2, int dir, int* layers3d) -> Animation3d*;
    [[nodiscard]] auto GetCritSprId(hash model_name, uint anim1, uint anim2, int dir, int* layers3d) -> uint;
    [[nodiscard]] auto GetRandomSplash() -> AnyFrames*;
    [[nodiscard]] auto GetSoundNames() -> StrMap& { return _soundNames; }

    AnyFrames* ItemHexDefaultAnim {};
    AnyFrames* CritterDefaultAnim {};

private:
    struct LoadedAnim
    {
        AtlasType ResType {};
        AnyFrames* Anim {};
    };

    [[nodiscard]] auto LoadFalloutAnim(hash model_name, uint anim1, uint anim2) -> AnyFrames*;
    [[nodiscard]] auto LoadFalloutAnimSpr(hash model_name, uint anim1, uint anim2) -> AnyFrames*;
    void FixAnimOffs(AnyFrames* frames_base, AnyFrames* stay_frm_base);
    void FixAnimOffsNext(AnyFrames* frames_base, AnyFrames* stay_frm_base);

    FileManager& _fileMngr;
    SpriteManager& _sprMngr;
    ClientScriptSystem& _scriptSys;
    EventUnsubscriber _eventUnsubscriber {};
    UIntStrMap _namesHash {};
    map<hash, LoadedAnim> _loadedAnims {};
    AnimMap _critterFrames {};
    map<hash, Animation3d*> _critter3d {};
    StrVec _splashNames {};
    StrMap _soundNames {};
    AnyFrames* _splash {};
    int _dummy {};
};
