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
// Copyright (c) 2006 - 2026, Anton Tsvetinskiy aka cvet <cvet@tut.by>
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

#if FO_NATIVE_SCRIPTING

#include "EngineBase.h"
#include "NativeScriptCore.h"
#include "ScriptSystem.h"

FO_BEGIN_NAMESPACE

// Public entry: register the Native scripting backend on the given engine
// and run the role-specific module dispatcher. Each wired startup site
// (Server.cpp / Client.cpp / Mapper.cpp) passes its own
// dispatcher pointer — LF_NativeScriptSynth emits `RegisterNativeScriptModules_<Role>`
// into every `NativeBindings-<Role>.cpp`. Pass nullptr when no user code is
// linked in.
//
// `isBaker` marks the MasterBaker-owned metadata/script context. Baker has no
// runtime BaseEngine, so its context exposes Meta and ScriptSys while Engine
// remains null.
void InitNativeScripting(EngineMetadata* meta, const ScriptSettings& settings, const FileSystem& resources, NativeScripts::Detail::RegisterModulesFn registerModules, bool isBaker = false);

FO_END_NAMESPACE

#endif
