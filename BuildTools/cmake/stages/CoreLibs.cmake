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

if(FO_NATIVE_SCRIPTING)
    SetValue(FO_NATIVE_SCRIPTING_DIR
        "${FO_ENGINE_ROOT}/Source/Scripting/Native")
    SetValue(FO_NATIVE_SCRIPTING_SOURCE
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScriptCore.h"
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScriptCore.cpp"
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScriptBackend.h"
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScriptBackend.cpp"
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScripting.h"
        "${FO_NATIVE_SCRIPTING_DIR}/NativeScripting.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi_ContextRpcMethods.h")
    AddCoreStaticLibrary(NativeScripting FO_NATIVE_SCRIPTING_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib)
    TargetIncludeDirectories(NativeScripting PUBLIC
        "${FO_NATIVE_SCRIPTING_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")

    # `NativeScriptSynth` — code-emitters for the native scripting
    # surface (`NativeApi.<Role>.cppm`,
    # `NativeApi_ContextRpcMethods.h`, `NativeBindings-<Role>.cpp`).
    # Lives in its own static lib rather than `BakerLib` so the
    # standalone build-time tool (`LF_NativeScriptSynth`) can link it
    # without dragging `BakerLib` → `NativeScripting`. Depends only on
    # `CommonLib`'s `EngineMetadata`.
    SetValue(FO_NATIVE_SCRIPT_SYNTH_SOURCE
        "${FO_ENGINE_ROOT}/Source/Tools/NativeScriptSynth.h"
        "${FO_ENGINE_ROOT}/Source/Tools/NativeScriptSynth.cpp")
    AddCoreStaticLibrary(NativeScriptSynth FO_NATIVE_SCRIPT_SYNTH_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib)
    TargetIncludeDirectories(NativeScriptSynth PUBLIC
        "${FO_ENGINE_ROOT}/Source/Tools")
endif()

if(FO_BUILD_CLIENT_LIB)
    AddCoreStaticLibrary(ClientLib FO_CLIENT_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_CLIENT_SYSTEM_LIBS} ${FO_CLIENT_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting> $<$<BOOL:${FO_NATIVE_SCRIPTING}>:NativeScripting>)
endif()

if(FO_BUILD_SERVER_LIB)
    AddCoreStaticLibrary(ServerLib FO_SERVER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS CommonLib ${FO_SERVER_SYSTEM_LIBS} ${FO_SERVER_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting> $<$<BOOL:${FO_NATIVE_SCRIPTING}>:NativeScripting>)
endif()

if(FO_BUILD_MAPPER_LIB)
    AddCoreStaticLibrary(AnimationViewerLib FO_ANIMATION_VIEWER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib)

    AddCoreStaticLibrary(ParticleViewerLib FO_PARTICLE_VIEWER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib)

    AddCoreStaticLibrary(MapperLib FO_MAPPER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS AnimationViewerLib ParticleViewerLib ClientLib CommonLib $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting> $<$<BOOL:${FO_NATIVE_SCRIPTING}>:NativeScripting>)
endif()

if(FO_BUILD_BAKER_LIB)
    AddCoreStaticLibrary(BakerLib FO_BAKER_SOURCE
        APPEND_TO_GROUP FO_CORE_LIBS_GROUP
        LINK_LIBS ClientLib CommonLib ${FO_BAKER_SYSTEM_LIBS} ${FO_BAKER_LIBS} $<$<BOOL:${FO_ANGELSCRIPT_SCRIPTING}>:AngelScriptScripting> $<$<BOOL:${FO_NATIVE_SCRIPTING}>:NativeScripting>)
endif()

# Native scripting: user module files placed under ${FO_NATIVE_SCRIPTS_DIR} are
# globbed per role in EngineSources stage (Common/Server/Client/Mapper/Baker)
# so they can be picked up by codegen as metadata sources. Here we wrap each
# non-empty role into its own static library and link it into the matching
# engine lib. Each user module imports its generated NativeApi.<Role> module.
if(FO_NATIVE_SCRIPTING AND FO_NATIVE_SCRIPTS_DIR)
    StatusMessage("Native scripts dir: ${FO_NATIVE_SCRIPTS_DIR}")

    foreach(role IN ITEMS Common Server Client Mapper Baker)
        StringToUpper(${role} roleUpper)
        SetValue(roleLibTarget "NativeScripts_${role}")

        # Always include the synth-emitted per-role dispatcher TU even when
        # no user sources exist — engine startup glue (Server.cpp etc.)
        # references `RegisterNativeScriptModules_<Role>` unconditionally, so
        # the symbol has to be present. The dispatcher body is empty when
        # there are no user modules in this role.
        SetValue(FO_NATIVE_SCRIPTS_${role}_SOURCE
            ${FO_NATIVE_SCRIPTS_${role}_FILES}
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeBindings-${role}.cpp")

        AddCoreStaticLibrary(${roleLibTarget} FO_NATIVE_SCRIPTS_${role}_SOURCE
            APPEND_TO_GROUP FO_CORE_LIBS_GROUP
            LINK_LIBS CommonLib NativeScripting)
        TargetIncludeDirectories(${roleLibTarget} PUBLIC
            "${FO_NATIVE_SCRIPTING_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource")
        TargetCompileDefinitions(${roleLibTarget} PRIVATE
            NATIVE_SCRIPTS_TARGET_${roleUpper}=1)

        # C++20 module interface units (.ixx / .cppm) need FILE_SET
        # CXX_MODULES so CMake runs the dependency scanner and generates
        # BMIs before consumer TUs compile. AddCoreStaticLibrary above
        # treats them as regular sources, which works for MSVC's
        # auto-detection but breaks module dependency tracking under
        # MSBuild — explicit FILE_SET enables both. Empty file list is a
        # no-op (skipped via the if-guard so older CMake versions don't
        # error on empty FILES).
        # C++20 module interface units land in the same `cxx_modules`
        # FILE_SET from two sources:
        #   - User authored: `NativeScripts/<role>/*.ixx` / `*.cppm`
        #     (collected into FO_NATIVE_SCRIPTS_${role}_MODULE_FILES by
        #     EngineSources.cmake).
        #   - synth-emitted: `GeneratedSource/NativeApi.<Role>.cppm`
        #     (complete per-target wrapper surface). Each role picks up
        #     exactly its own file — no cross-target sharing,
        #     to enforce the per-target isolation guarantee that user
        #     `import NativeApi.Server;` only sees server-flavored
        #     symbols.
        SetValue(FO_NATIVE_SCRIPTS_${role}_GENERATED_MODULES
            "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource/NativeApi.${role}.cppm")

        if(FO_NATIVE_SCRIPTS_${role}_MODULE_FILES OR FO_NATIVE_SCRIPTS_${role}_GENERATED_MODULES)
            # FILE_SET name must start with a lowercase letter (CMake
            # convention enforced since 3.23) — `cxx_modules` is the
            # canonical name for the CXX_MODULES type set. BASE_DIRS
            # lists both the user-source root and the codegen output
            # dir so files from either resolve correctly inside the
            # set.
            target_sources(${roleLibTarget} PUBLIC
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS
                    "${FO_NATIVE_SCRIPTS_DIR}/${role}"
                    "${CMAKE_CURRENT_BINARY_DIR}/GeneratedSource"
                FILES
                    ${FO_NATIVE_SCRIPTS_${role}_MODULE_FILES}
                    ${FO_NATIVE_SCRIPTS_${role}_GENERATED_MODULES})
            set_target_properties(${roleLibTarget} PROPERTIES
                CXX_SCAN_FOR_MODULES ON)
        endif()

        # Anchor the static archive at the final link by joining the matching
        # engine lib's transitive link list. The synth-emitted dispatcher
        # `RegisterNativeScriptModules_<Role>` (forward-declared and called
        # from Server.cpp / Client.cpp / Mapper.cpp) acts as the external
        # symbol that drags `NativeBindings-<Role>.obj` in; that obj in turn
        # imports each user module and calls its exported initializer so every
        # user TU joins the link too. No WHOLE_ARCHIVE needed.
        if(role STREQUAL "Common")
            # `NativeScripts_Common` would naturally go on `CommonLib`,
            # but that propagates transitively to every CommonLib
            # consumer — including build-time tools like
            # `LF_NativeScriptSynth` that don't need the dispatcher and
            # whose sources GENERATE `NativeApi.Common.cppm` (the
            # very file `NativeScripts_Common` lists as a source).
            # Form a target cycle. Instead, link the Common
            # dispatcher into each role-specific engine lib —
            # `RegisterNativeScriptModules_Common(ctx)` is called from
            # Server.cpp / Client.cpp / Mapper.cpp and MasterBaker's
            # session setup. All of these live downstream of one of the
            # role-specific engine libraries.
            foreach(targetLib IN ITEMS ServerLib ClientLib MapperLib BakerLib)
                if(TARGET ${targetLib})
                    TargetLinkLibraries(${targetLib} ${roleLibTarget})
                endif()
            endforeach()
        elseif(role STREQUAL "Server")
            if(TARGET ServerLib)
                TargetLinkLibraries(ServerLib ${roleLibTarget})
            endif()
        elseif(role STREQUAL "Client")
            if(TARGET ClientLib)
                TargetLinkLibraries(ClientLib ${roleLibTarget})
            endif()
        elseif(role STREQUAL "Mapper")
            if(TARGET MapperLib)
                TargetLinkLibraries(MapperLib ${roleLibTarget})
            endif()
        elseif(role STREQUAL "Baker")
            if(TARGET BakerLib)
                TargetLinkLibraries(BakerLib ${roleLibTarget})
            endif()
        endif()
    endforeach()
endif()
