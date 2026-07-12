cmake_minimum_required(VERSION 3.22)

# === Stage: CoreLibs ===
# Auto-extracted from FinalizeGeneration.cmake by the staged-pipeline refactor.
# Add or override behaviour via AddStageHook(CoreLibs Pre|Post <macro-name>).

# Core libs
StatusMessage("Core libs:")

AddCoreStaticLibrary(EssentialsLib FO_ESSENTIALS_SOURCE
    APPEND_TO_GROUP FO_CORE_LIBS_GROUP
    LINK_LIBS ${FO_ESSENTIALS_SYSTEM_LIBS} ${FO_ESSENTIALS_LIBS})

if(FO_BUILD_COMMON_LIB)
    SetValue(FO_APP_HEADLESS_SOURCE
        "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationHeadless.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationStub.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Null.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
        "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h")
    AddCoreStaticLibrary(AppHeadless FO_APP_HEADLESS_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP)

    if(NOT FO_HEADLESS_ONLY)
        SetValue(FO_APP_FRONTEND_SOURCE
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationInit.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Application.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/ApplicationStub.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Null.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering.h"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Direct3D.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-OpenGL.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-Vulkan.cpp"
            "${FO_ENGINE_ROOT}/Source/Frontend/Rendering-SDLGpu.cpp")
        AddCoreStaticLibrary(AppFrontend FO_APP_FRONTEND_SOURCE
            APPEND_TO_GROUP FO_CORE_LIBS_GROUP
            LINK_LIBS ${FO_RENDER_SYSTEM_LIBS} ${FO_RENDER_LIBS})
        # Vulkan builds against the headers vendored with SDL3 (no external SDK); the loader is resolved
        # dynamically at runtime, so only these headers are needed at build time. target_include_directories
        # requires an absolute path, so anchor the relative FO_SDL_DIR at the project source root.
        TargetIncludeDirectories(AppFrontend SYSTEM PUBLIC $<$<BOOL:${FO_HAVE_VULKAN}>:${CMAKE_SOURCE_DIR}/${FO_SDL_DIR}/src/video/khronos>)
    endif()

    AddCoreStaticLibrary(CommonLib FO_COMMON_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS EssentialsLib ${FO_COMMON_SYSTEM_LIBS} ${FO_COMMON_LIBS})
endif()

if(FO_ANGELSCRIPT_SCRIPTING)
    SetValue(FO_ANGELSCRIPT_SCRIPTING_DIR
        "${FO_ENGINE_ROOT}/Source/Scripting/AngelScript")
    SetValue(FO_ANGELSCRIPT_SCRIPTING_SOURCE
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptArray.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptArray.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptAttributes.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptAttributes.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptBackend.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptBackend.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptCall.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptCall.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptContext.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptContext.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDict.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDict.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDebugger.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptDebugger.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptEntity.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptEntity.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptGlobals.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptGlobals.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptHelpers.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptHelpers.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptMath.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptMath.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptReflection.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptReflection.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptRemoteCalls.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptRemoteCalls.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptScripting.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptScripting.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptString.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptString.h"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptTypes.cpp"
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}/AngelScriptTypes.h")
    AddCoreStaticLibrary(AngelScriptScripting FO_ANGELSCRIPT_SCRIPTING_SOURCE
         APPEND_TO_GROUP FO_CORE_LIBS_GROUP
         LINK_LIBS CommonLib AngelScriptCore AngelScriptPreprocessor)
    TargetIncludeDirectories(AngelScriptScripting PUBLIC
        "${FO_ANGELSCRIPT_SCRIPTING_DIR}")
endif()

if(FO_BUILD_CLIENT_LIB)
    AddCoreStaticLibrary(ClientLib FO_CLIENT_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting>)
endif()

if(FO_BUILD_SERVER_LIB)
    AddCoreStaticLibrary(ServerLib FO_SERVER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting>)
endif()

if(FO_BUILD_MAPPER_LIB)
    AddCoreStaticLibrary(MapperLib FO_MAPPER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting>)
endif()

if(FO_BUILD_BAKER_LIB)
    AddCoreStaticLibrary(BakerLib FO_BAKER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting>)
endif()
