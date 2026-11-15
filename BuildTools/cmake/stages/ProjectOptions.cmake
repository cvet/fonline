cmake_minimum_required(VERSION 3.22)

# Register project options and extend through AddStageHook(ProjectOptions Pre|Post <macro-name>)

if(NOT ${FO_FORCE_ENABLE_3D} STREQUAL "")
    SetValue(FO_ENABLE_3D ${FO_FORCE_ENABLE_3D})
endif()

# Configuration checks
if(NOT "${FO_UNITY_BUILD_BATCH_SIZE}" MATCHES "^[1-9][0-9]*$")
    AbortMessage("FO_UNITY_BUILD_BATCH_SIZE must be a positive integer; got '${FO_UNITY_BUILD_BATCH_SIZE}'")
endif()

# A map side stops at the int16 coordinate range, or at 4194304 with wide coordinates: the client sprite draw-order
# key holds a cell X in 22 bits and a screen row in 24 bits (GameSettings::MAP_SIZE_LIMIT)
if(FO_MAP_WIDE_COORDS)
    SetValue(foMapSizeLimit 4194304)
    SetValue(foMapSizeLimitReason "the widest map side the sprite draw-order key orders")
else()
    SetValue(foMapSizeLimit 32767)
    SetValue(foMapSizeLimitReason "the int16 map coordinate range; enable FO_MAP_WIDE_COORDS for wider maps")
endif()

if(NOT "${FO_MAP_MAX_SIZE}" MATCHES "^[1-9][0-9]*$" OR FO_MAP_MAX_SIZE LESS 200 OR FO_MAP_MAX_SIZE GREATER foMapSizeLimit)
    AbortMessage("FO_MAP_MAX_SIZE must be an integer from 200 to ${foMapSizeLimit} (${foMapSizeLimitReason}); got '${FO_MAP_MAX_SIZE}'")
endif()

if(FO_CODE_COVERAGE AND (
    FO_BUILD_CLIENT OR
    FO_BUILD_SERVER OR
    FO_BUILD_MAPPER OR
    FO_BUILD_ASCOMPILER OR
    FO_BUILD_BAKER OR
    FO_UNIT_TESTS))
    AbortMessage("Code coverage build can not be mixed with other builds")
endif()

if(FO_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING)
    AbortMessage("AngelScript compiler build can not be without AngelScript scripting enabled")
endif()
