// FOnline Engine
// SPDX-License-Identifier: MIT

#pragma once

#include "Common.h"

FO_BEGIN_NAMESPACE

FO_DECLARE_EXCEPTION(Direct3DLevel9Exception);

// Adds the Aon9 chunk, the only code Direct3D 11 runs on a feature level 9.x device, to a Shader Model 4.0 container, built at
// level 9.3 from the vs_2_a or ps_2_0 bytecode vkd3d-shader compiled from the same source; throws when the shader does not fit
auto AddDirect3DLevel9Code(const_span<uint8_t> sm4_container, const_span<uint8_t> sm2_bytecode, bool is_vertex, string_view name) -> vector<uint8_t>;

FO_END_NAMESPACE
