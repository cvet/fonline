# FOnline builds FreeType with the TrueType driver, the automatic hinter and the grey and monochrome rasterizers.
# The module list and the build options live in include/freetype/config/ beside this file, ahead of the upstream headers
SetValue(FO_FREETYPE_SOURCE
    "${FO_FREETYPE_DIR}/include/freetype/config/ftmodule.h"
    "${FO_FREETYPE_DIR}/include/freetype/config/ftoption.h"
    "${FO_FREETYPE_DIR}/freetype/src/base/ftbase.c"
    "${FO_FREETYPE_DIR}/freetype/src/base/ftbitmap.c"
    "${FO_FREETYPE_DIR}/freetype/src/base/ftdebug.c"
    "${FO_FREETYPE_DIR}/freetype/src/base/ftinit.c"
    "${FO_FREETYPE_DIR}/freetype/src/base/ftsystem.c"
    "${FO_FREETYPE_DIR}/freetype/src/autofit/autofit.c"
    "${FO_FREETYPE_DIR}/freetype/src/psnames/psnames.c"
    "${FO_FREETYPE_DIR}/freetype/src/raster/raster.c"
    "${FO_FREETYPE_DIR}/freetype/src/sfnt/sfnt.c"
    "${FO_FREETYPE_DIR}/freetype/src/smooth/smooth.c"
    "${FO_FREETYPE_DIR}/freetype/src/truetype/truetype.c")
