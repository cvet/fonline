cmake_minimum_required(VERSION 3.22)

# === Stage: ProjectOptions ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(ProjectOptions Pre|Post <macro-name>).

if(NOT ${FO_FORCE_ENABLE_3D} STREQUAL "")
    SetValue(FO_ENABLE_3D ${FO_FORCE_ENABLE_3D})
endif()

# Configuration checks
if(FO_CODE_COVERAGE AND (
    FO_BUILD_CLIENT OR
    FO_BUILD_SERVER OR
    FO_BUILD_MAPPER OR
    FO_BUILD_EDITOR OR
    FO_BUILD_ASCOMPILER OR
    FO_BUILD_BAKER OR
    FO_UNIT_TESTS))
    AbortMessage("Code coverage build can not be mixed with other builds")
endif()

if(FO_BUILD_ASCOMPILER AND NOT FO_ANGELSCRIPT_SCRIPTING)
    AbortMessage("AngelScript compiler build can not be without AngelScript scripting enabled")
endif()
