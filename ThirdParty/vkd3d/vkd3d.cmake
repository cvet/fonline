# FOnline builds only libvkd3d-shader, for the effect baker's HLSL to DXBC step, together with the part of
# libvkd3d-common it links. Keep this list in sync with libvkd3d_shader_la_SOURCES in the upstream Makefile.am.
SetValue(FO_VKD3D_SHADER_SOURCE
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-common/debug.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-common/error.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-common/memory.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-common/utf8.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/checksum.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/d3d_asm.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/d3dbc.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/dxbc.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/dxil.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/fx.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/glsl.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/hlsl.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/hlsl.tab.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/hlsl.yy.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/hlsl_codegen.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/hlsl_constant_ops.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/ir.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/msl.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/preproc.tab.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/preproc.yy.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/spirv.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/tpf.c"
    "${FO_VKD3D_DIR}/vkd3d/libs/vkd3d-shader/vkd3d_shader_main.c")

# The SPIR-V target includes spirv/unified1/*.h, which the engine already vendors inside SPIRV-Cross
SetValue(FO_VKD3D_SPIRV_INCLUDE_DIR "${CMAKE_BINARY_DIR}/vkd3d-spirv")
configure_file("${FO_SPIRV_CROSS_DIR}/spirv.h" "${FO_VKD3D_SPIRV_INCLUDE_DIR}/spirv/unified1/spirv.h" COPYONLY)
configure_file("${FO_SPIRV_CROSS_DIR}/GLSL.std.450.h" "${FO_VKD3D_SPIRV_INCLUDE_DIR}/spirv/unified1/GLSL.std.450.h" COPYONLY)
