// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

#if FO_EFFEKSEER_PARTICLES

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(EffekseerCompilerException);

struct EffekseerCompilerOutput
{
    vector<uint8_t> Binary {};
    vector<string> Dependencies {};
};

auto CompileEffekseerProject(string_view project_path, const_span<uint8_t> project_data) -> EffekseerCompilerOutput;
auto GetEffekseerProjectDependencies(string_view project_path, const_span<uint8_t> project_data) -> vector<string>;

FO_END_NAMESPACE

#endif
