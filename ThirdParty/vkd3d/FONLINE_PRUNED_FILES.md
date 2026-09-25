FOnline ThirdParty pruning notes

The upstream release lives in vkd3d/, and the engine's own build files for it sit
beside that folder. This vendored copy is intentionally trimmed for the engine
build. When updating from upstream, replace vkd3d/ with the new release and
remove these paths again; paths below are relative to vkd3d/.

Removed paths:
- bin/
- crosslibs/
- demos/
- libs/vkd3d/
- libs/vkd3d-utils/
- m4/
- programs/
- tests/
- aclocal.m4
- configure
- configure.ac
- Doxyfile.in
- INSTALL
- Makefile.am
- Makefile.in
- include/config.h.in
- include/*.idl
- include/vkd3d.h
- include/vkd3d_d3d10_1shader.h
- include/vkd3d_d3d10shader.h
- include/vkd3d_d3d11shader.h
- include/vkd3d_d3d12.h
- include/vkd3d_d3d12sdklayers.h
- include/vkd3d_d3d12shader.h
- include/vkd3d_d3dcompiler.h
- include/vkd3d_d3dcompiler_types.h
- include/vkd3d_dxgi.h
- include/vkd3d_dxgi1_2.h
- include/vkd3d_dxgi1_3.h
- include/vkd3d_dxgi1_4.h
- include/vkd3d_dxgibase.h
- include/vkd3d_dxgiformat.h
- include/vkd3d_dxgitype.h
- include/vkd3d_utils.h
- include/private/appkit.json
- include/private/foundation.json
- include/private/quartzcore.json
- include/private/spirv.core.grammar.json
- include/private/vkd3d_blob.h
- include/private/vkd3d_test.h
- libs/vkd3d-common/blob.c
- libs/vkd3d-shader/libvkd3d-shader.pc.in
- libs/vkd3d-shader/make_spirv
- libs/vkd3d-shader/vkd3d_shader.map

Generated files added:
The release tarball ships the grammars but not the parsers built from them, and the
engine build must not depend on bison and flex being installed. Regenerate these after
every update, from vkd3d/libs/vkd3d-shader/, with GNU Bison 3.8.2 and flex 2.6.4 (on Windows
the win_flex_bison 2.5.25 bundle carries both), keeping the machine's paths out of the
output:
- libs/vkd3d-shader/hlsl.tab.c, hlsl.tab.h: bison -d -l -o hlsl.tab.c hlsl.y
- libs/vkd3d-shader/preproc.tab.c, preproc.tab.h: bison -d -l -o preproc.tab.c preproc.y
- libs/vkd3d-shader/hlsl.yy.c: flex -L --nounistd -o hlsl.yy.c hlsl.l
- libs/vkd3d-shader/preproc.yy.c: flex -L --nounistd -o preproc.yy.c preproc.l

Engine files beside vkd3d/ (upstream never ships them):
- vkd3d.cmake: the libvkd3d-shader source list and the SPIR-V header staging.
- config.h: replaces the autoconf-generated include/config.h.
- vkd3d_version.h: replaces the header the upstream build derives from git.
