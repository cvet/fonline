FOnline ThirdParty pruning notes

The upstream release lives in freetype/, and the engine's own build files for it sit
beside that folder. This vendored copy is intentionally trimmed for the engine
build. When updating from upstream, replace freetype/ with the new release and
remove these paths again; paths below are relative to freetype/.

Removed paths:
- builds/
- devel/
- objs/
- subprojects/
- tests/
- src/bdf/
- src/bzip2/
- src/cache/
- src/cff/
- src/cid/
- src/dlg/
- src/gxvalid/
- src/gzip/
- src/lzw/
- src/otvalid/
- src/pcf/
- src/pfr/
- src/psaux/
- src/pshinter/
- src/sdf/
- src/svg/
- src/tools/
- src/type1/
- src/type42/
- src/winfonts/
- docs/ except docs/FTL.TXT and docs/GPLv2.TXT
- autogen.sh
- ChangeLog
- CMakeLists.txt
- configure
- Makefile
- meson.build
- meson_options.txt
- modules.cfg
- MSBuild.rsp
- MSBuild.sln
- README.git
- vms_make.com

Engine files beside freetype/ (upstream never ships them):
- freetype.cmake: the source list of the modules the engine builds.
- include/freetype/config/ftmodule.h: the module list, replacing the upstream one that registers every module.
- include/freetype/config/ftoption.h: a copy of the upstream file whose changed lines are marked (FOnline Patch);
  re-apply those lines to the new upstream copy after every update.
