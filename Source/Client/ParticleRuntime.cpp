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

#include "ParticleRuntime.h"
#include "Application.h"
#include "EffectManager.h"
#include "EffekseerExtension.h"
#include "SparkExtension.h"

FO_BEGIN_NAMESPACE

static constexpr ucolor PARTICLE_WIREFRAME_COLOR {255, 0, 255, 255};

auto ParticleRuntimeSystem::GetBakedBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::nullopt;
}

auto ParticleRuntimeSystem::GetLiveBounds() const noexcept -> optional<ParticleBounds3D>
{
    FO_NO_STACK_TRACE_ENTRY();

    return std::nullopt;
}

void ParticleRuntimeSystem::RebaseWorldParticles(vec3 delta) noexcept
{
    FO_NO_STACK_TRACE_ENTRY();

    ignore_unused(delta);
}

auto CreateParticleRuntimeBackends(const ParticleRuntimeServices& services) -> vector<unique_ptr<ParticleRuntimeBackend>>
{
    FO_STACK_TRACE_ENTRY();

    ignore_unused(services);

    vector<unique_ptr<ParticleRuntimeBackend>> backends;

#if FO_SPARK_PARTICLES
    backends.emplace_back(SafeAlloc::MakeUnique<SparkParticleRuntimeBackend>(services));
#endif
#if FO_EFFEKSEER_PARTICLES
    backends.emplace_back(SafeAlloc::MakeUnique<EffekseerParticleRuntimeBackend>(services));
#endif

    return backends;
}

void DrawParticleBufferWireframe(ptr<EffectManager> effect_mngr, ptr<IAppRender> render, unique_nptr<RenderDrawBuffer>& overlay_buf, const RenderDrawBuffer& source_buf, size_t index_count, const mat44& proj_matrix)
{
    FO_STACK_TRACE_ENTRY();

    FO_VERIFY_AND_THROW(index_count % 3 == 0, "Particle wireframe source indices are not grouped by triangles", index_count);
    FO_VERIFY_AND_THROW(index_count <= source_buf.IndCount, "Particle wireframe source range is outside the index buffer", index_count, source_buf.IndCount);

    if (index_count == 0) {
        return;
    }

    nptr<RenderEffect> effect = effect_mngr->Effects.Primitive;
    FO_VERIFY_AND_THROW(effect, "Primitive effect is unavailable for particle wireframe rendering");

    if (!overlay_buf) {
        overlay_buf = render->CreateDrawBuffer(false);
    }

    size_t vertex_count = index_count * 2;
    overlay_buf->CheckAllocBuf(vertex_count, vertex_count);
    size_t out_index = 0;

    for (size_t i = 0; i < index_count; i += 3) {
        const vindex_t triangle_indices[3] = {
            source_buf.Indices[i],
            source_buf.Indices[i + 1],
            source_buf.Indices[i + 2],
        };

        for (size_t edge = 0; edge < 3; edge++) {
            vindex_t from_index = triangle_indices[edge];
            vindex_t to_index = triangle_indices[(edge + 1) % 3];
            FO_VERIFY_AND_THROW(from_index < source_buf.VertCount && to_index < source_buf.VertCount, "Particle wireframe index is outside the vertex buffer", from_index, to_index, source_buf.VertCount);

            for (vindex_t vertex_index : {from_index, to_index}) {
                Vertex2D vertex = source_buf.Vertices[vertex_index];
                vertex.Color = PARTICLE_WIREFRAME_COLOR;
                vertex.TexU = vertex.TexV = 0.0f;
                vertex.EggFlags[0] = vertex.EggFlags[1] = 0.0f;
                overlay_buf->Vertices[out_index] = vertex;
                overlay_buf->Indices[out_index] = numeric_cast<vindex_t>(out_index);
                out_index++;
            }
        }
    }

    overlay_buf->VertCount = out_index;
    overlay_buf->IndCount = out_index;
    overlay_buf->PrimType = RenderPrimitiveType::LineList;
    overlay_buf->Upload(EffectUsage::Primitive, out_index, out_index);

    effect->ProjBuf = RenderEffect::ProjBuffer();
    MemCopy(effect->ProjBuf->ProjMatrix, glm::value_ptr(proj_matrix), sizeof(effect->ProjBuf->ProjMatrix));
    effect->DrawBuffer(overlay_buf, 0, out_index);

    overlay_buf->VertCount = 0;
    overlay_buf->IndCount = 0;
}

FO_END_NAMESPACE
