include_guard()

# Quiet all non-error messages instead ourself
function(message mode)
	if(${mode} STREQUAL "FATAL_ERROR")
		_message(FATAL_ERROR ${ARGN})
	elseif(${mode} STREQUAL "SEND_ERROR")
		_message(SEND_ERROR ${ARGN})
	elseif(FO_VERBOSE_BUILD)
		_message(${mode} ${ARGN})
	endif()
endfunction()

function(StatusMessage)
	_message(STATUS ${ARGN})
endfunction()

function(AbortMessage)
	_message(FATAL_ERROR ${ARGN})
endfunction()

# Skip all install rules
function(install)
endfunction()

function(export)
endfunction()

set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Forced by FOnline" FORCE)

# Disable warnings in third-party libs
function(DisableLibWarnings)
	foreach(lib ${ARGV})
		# -w suppresses ordinary warnings, but Clang 20+ promoted several legacy-C diagnostics to errors BY DEFAULT
		# (the C23 transition); -w does not downgrade those, so vendored C code (LibreSSL, mongo-c, ...) would fail to
		# build under clang/clang-cl. Demote that family back to warnings (then -w hides them) so third-party stays
		# silent on every toolchain.
		target_compile_options(${lib} PRIVATE
			$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:-w>
			$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-error=incompatible-pointer-types
				-Wno-error=incompatible-function-pointer-types
				-Wno-error=int-conversion
				-Wno-error=implicit-function-declaration
				-Wno-error=implicit-int>
			$<$<CXX_COMPILER_ID:MSVC>:/W0>)
	endforeach()
endfunction()

macro(AddCompileOptionsCCXX)
	foreach(option ${ARGV})
		add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:${option}>)
	endforeach()
endmacro()

macro(AddCompileOptionsList)
	foreach(option ${ARGN})
		AddCompileOptionsCCXX(${option})
	endforeach()
endmacro()

macro(AddLinkOptionsList)
	foreach(option ${ARGN})
		add_link_options(${option})
	endforeach()
endmacro()

macro(AddCompileDefinitionsList)
	add_compile_definitions(${ARGN})
endmacro()

macro(SetTargetsFolder folder)
	if(ARGN)
		set_property(TARGET ${ARGN} PROPERTY FOLDER "${folder}")
	endif()
endmacro()

macro(AddSubdirectory sourceDir)
	set(_fo_add_subdirectory_binary_dir "")
	set(_fo_add_subdirectory_folder "")
	set(_fo_add_subdirectory_extra_args "")
	set(_fo_add_subdirectory_args ${ARGN})

	if(_fo_add_subdirectory_args)
		ListGet(_fo_add_subdirectory_args 0 _fo_add_subdirectory_first_arg)

		if(NOT _fo_add_subdirectory_first_arg STREQUAL "EXCLUDE_FROM_ALL" AND
			NOT _fo_add_subdirectory_first_arg STREQUAL "SYSTEM" AND
			NOT _fo_add_subdirectory_first_arg STREQUAL "FOLDER")
			ListPopFront(_fo_add_subdirectory_args _fo_add_subdirectory_binary_dir)
		endif()
	endif()

	while(_fo_add_subdirectory_args)
		ListPopFront(_fo_add_subdirectory_args _fo_add_subdirectory_arg)

		if(_fo_add_subdirectory_arg STREQUAL "FOLDER")
			if(NOT _fo_add_subdirectory_args)
				AbortMessage("AddSubdirectory expects folder name after FOLDER")
			endif()

			ListPopFront(_fo_add_subdirectory_args _fo_add_subdirectory_folder)
		elseif(_fo_add_subdirectory_arg STREQUAL "EXCLUDE_FROM_ALL" OR _fo_add_subdirectory_arg STREQUAL "SYSTEM")
			AppendList(_fo_add_subdirectory_extra_args ${_fo_add_subdirectory_arg})
		else()
			AbortMessage("AddSubdirectory got unexpected argument ${_fo_add_subdirectory_arg}")
		endif()
	endwhile()

	if(NOT _fo_add_subdirectory_folder STREQUAL "")
		set(_fo_prev_folder "${CMAKE_FOLDER}")
		set(CMAKE_FOLDER "${_fo_add_subdirectory_folder}")
	endif()

	if(NOT _fo_add_subdirectory_binary_dir STREQUAL "")
		add_subdirectory("${sourceDir}" "${_fo_add_subdirectory_binary_dir}" ${_fo_add_subdirectory_extra_args})
	else()
		add_subdirectory("${sourceDir}" ${_fo_add_subdirectory_extra_args})
	endif()

	if(NOT _fo_add_subdirectory_folder STREQUAL "")
		set(CMAKE_FOLDER "${_fo_prev_folder}")
	endif()
endmacro()

macro(AddIncludeDirectories)
	foreach(dir ${ARGN})
		include_directories("${dir}")
	endforeach()
endmacro()

macro(AddLinkDirectories)
	foreach(dir ${ARGN})
		link_directories("${dir}")
	endforeach()
endmacro()

macro(SetDefaultVariables defaultValue)
	foreach(var ${ARGN})
		set(${var} "${defaultValue}")
	endforeach()
endmacro()

macro(SetOptionValues)
	set(optionArgs "${ARGN}")
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 2")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("SetOptionValues expects pairs of arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionValue)
		set(_soptShadow "_FO_OPTION_DEFAULT_${optionName}")
		get_property(_soptHasCache CACHE ${optionName} PROPERTY VALUE SET)
		get_property(_soptCacheType CACHE ${optionName} PROPERTY TYPE)

		# Shadow default: if the current cache value differs from the default we last applied, the value
		# was explicitly overridden (-D / preset cacheVariables / gui) -> keep it. On a first configure,
		# untyped -D / preset cache entries have no shadow yet, so preserve those as initial overrides too.
		# Otherwise (unset, or still our default) -> (re)apply the current cmake-file default with FORCE.
		# This keeps the cmake file authoritative against stale-cache drift (e.g. FO_EFFECT_SCRIPT_VALUES no
		# longer sticks at an old number) while still honoring real overrides.
		if(DEFINED ${optionName} AND DEFINED ${_soptShadow} AND NOT "${${optionName}}" STREQUAL "${${_soptShadow}}")
			set(_soptResolved "${${optionName}}")
		elseif(_soptHasCache AND _soptCacheType STREQUAL "UNINITIALIZED" AND NOT DEFINED ${_soptShadow})
			set(_soptResolved "${${optionName}}")
		else()
			set(_soptResolved "${optionValue}")
		endif()

		string(TOUPPER "${_soptResolved}" _soptUpper)
		if(_soptUpper STREQUAL "ON" OR _soptUpper STREQUAL "OFF" OR _soptUpper STREQUAL "TRUE" OR
			_soptUpper STREQUAL "FALSE" OR _soptUpper STREQUAL "YES" OR _soptUpper STREQUAL "NO" OR
			_soptUpper STREQUAL "1" OR _soptUpper STREQUAL "0")
			set(${optionName} ${_soptResolved} CACHE BOOL "Forced by FOnline (override via -D / preset / gui)" FORCE)
		else()
			set(${optionName} "${_soptResolved}" CACHE STRING "Forced by FOnline (override via -D / preset / gui)" FORCE)
		endif()

		set(${_soptShadow} "${optionValue}" CACHE INTERNAL "Last FOnline-applied default for ${optionName}")
		set(${optionName} "${_soptResolved}")
	endwhile()

	unset(_soptShadow)
	unset(_soptHasCache)
	unset(_soptCacheType)
	unset(_soptResolved)
	unset(_soptUpper)
endmacro()

# Force-set a list of CMake cache variables, auto-detecting BOOL vs STRING entries:
# values matching CMake's boolean spellings (ON/OFF/TRUE/FALSE/YES/NO/1/0,
# case-insensitive) become CACHE BOOL, everything else CACHE STRING. Empty string
# is treated as STRING (so OPENSSL_INCLUDE_DIR="" stays a path setting).
macro(SetCacheValues)
	set(optionArgs "${ARGN}")
	ListLength(optionArgs optionArgsCount)
	MathExpr(optionArgsRemainder "${optionArgsCount} % 2")

	if(NOT optionArgsRemainder EQUAL 0)
		AbortMessage("SetCacheValues expects pairs of arguments")
	endif()

	while(optionArgs)
		ListPopFront(optionArgs optionName optionValue)
		string(TOUPPER "${optionValue}" _setCacheValueUpper)

		if(_setCacheValueUpper STREQUAL "ON" OR
			_setCacheValueUpper STREQUAL "OFF" OR
			_setCacheValueUpper STREQUAL "TRUE" OR
			_setCacheValueUpper STREQUAL "FALSE" OR
			_setCacheValueUpper STREQUAL "YES" OR
			_setCacheValueUpper STREQUAL "NO" OR
			_setCacheValueUpper STREQUAL "1" OR
			_setCacheValueUpper STREQUAL "0")
			set(${optionName} ${optionValue} CACHE BOOL "Forced by FOnline" FORCE)
		else()
			set(${optionName} "${optionValue}" CACHE STRING "Forced by FOnline" FORCE)
		endif()

		set(${optionName} "${optionValue}")
	endwhile()

	unset(_setCacheValueUpper)
endmacro()

macro(AddConfiguration name parent)
	StringToUpper(${name} nameUpper)
	StringToUpper(${parent} parentUpper)

	AppendList(FO_CONFIGURATION_TYPES ${name})

	if(FO_MULTICONFIG)
		set(
			CMAKE_CONFIGURATION_TYPES
			"${CMAKE_CONFIGURATION_TYPES};${name}"
			CACHE STRING "Forced by FOnline" FORCE)
	endif()

	CopyConfigurationType(${parent} ${name})
endmacro()

macro(ResolveContributedFiles)
	set(resolvedFiles "")

	foreach(file ${ARGN})
		FileGlob(globFiles LIST_DIRECTORIES TRUE "${FO_CONTRIBUTION_DIR}/${file}")

		foreach(globFile ${globFiles})
			GetFilenameComponent(globFile ${globFile} ABSOLUTE)
			AppendList(resolvedFiles ${globFile})
		endforeach()
	endforeach()
endmacro()

macro(AddEngineSource target)
	ResolveContributedFiles(${ARGN})

	foreach(resolvedFile ${resolvedFiles})
		StatusMessage("Add engine source (${target}): ${resolvedFile}")
		AppendList(FO_${target}_SOURCE ${resolvedFile})
		AppendList(FO_SOURCE_META_FILES ${resolvedFile})

		StringRegexMatch("\\.h$" isHeader "${resolvedFile}")

		if(${target} STREQUAL "COMMON" AND isHeader)
			AppendList(FO_ADDED_COMMON_HEADERS ${resolvedFile})
		endif()
	endforeach()
endmacro()

macro(AddNativeIncludeDir)
	if(FO_NATIVE_SCRIPTING)
		foreach(dir ${ARGN})
			StatusMessage("Add native include dir: ${FO_CONTRIBUTION_DIR}/${dir}")
			AddIncludeDirectories("${FO_CONTRIBUTION_DIR}/${dir}")
		endforeach()
	endif()
endmacro()

macro(AddEngineSources)
	set(sourceArgs ${ARGN})
	ListLength(sourceArgs sourceArgsCount)
	MathExpr(sourceArgsRemainder "${sourceArgsCount} % 2")

	if(NOT sourceArgsRemainder EQUAL 0)
		AbortMessage("AddEngineSources expects pairs of arguments")
	endif()

	while(sourceArgs)
		ListPopFront(sourceArgs sourceTarget sourcePath)
		AddEngineSource(${sourceTarget} ${sourcePath})
	endwhile()
endmacro()


macro(CreatePackage package)
	AppendList(FO_PACKAGES ${package})
	set(Package_${package}_Config "")
	set(Package_${package}_Parts "")
endmacro()

macro(AddToPackage package binary platform arch packType)
	AppendList(Package_${package}_Parts "${binary},${platform},${arch},${packType},${ARGN}")
endmacro()

macro(AddPackageOption package optionName optionValue)
	set(Package_${package}_Option_${optionName} "${optionValue}")
endmacro()

macro(DefinePackage package)
	CreatePackage(${package})
	set(packageArgs "${ARGN}")

	while(packageArgs)
		ListPopFront(packageArgs packageKeyword)

		if(packageKeyword STREQUAL "CONFIG")
			if(NOT packageArgs)
				AbortMessage("DefinePackage ${package} CONFIG expects a value")
			endif()

			ListPopFront(packageArgs packageConfig)
			set(Package_${package}_Config "${packageConfig}")
		elseif(packageKeyword STREQUAL "BINARY")
			ListLength(packageArgs packageArgsCount)
			if(packageArgsCount LESS 4)
				AbortMessage("DefinePackage ${package} BINARY expects target, platform, arch, and pack type")
			endif()

			ListPopFront(packageArgs binary platform arch packType)
			AddToPackage(${package} ${binary} ${platform} ${arch} ${packType})
		elseif(packageKeyword STREQUAL "OPTION")
			ListLength(packageArgs packageArgsCount)
			if(packageArgsCount LESS 2)
				AbortMessage("DefinePackage ${package} OPTION expects name and value")
			endif()

			ListPopFront(packageArgs optionName optionValue)
			AddPackageOption(${package} ${optionName} "${optionValue}")
		else()
			AbortMessage("DefinePackage ${package} got unexpected keyword: ${packageKeyword}")
		endif()
	endwhile()

	if("${Package_${package}_Config}" STREQUAL "")
		AbortMessage("DefinePackage ${package} requires CONFIG")
	endif()
endmacro()

macro(EnableCoreLibraryIfAny outputVar)
	foreach(condition ${ARGN})
		if(${condition})
			set(${outputVar} ON)
			break()
		endif()
	endforeach()
endmacro()

macro(AddNativeOptimizationFlags)
	AddCompileOptionsCCXX($<${expr_DebugInfo}:-g>)
	AddCompileOptionsCCXX($<${expr_FullOptimization}:-O3>)
	AddCompileOptionsCCXX($<${expr_FullOptimization}:-flto>)
	add_link_options($<${expr_FullOptimization}:-flto>)
endmacro()

macro(SetBinaryOutputPath outputVar targetName)
	set(_fo_binary_output_postfix "")
	if(NOT "${FO_BINARY_OUTPUT_POSTFIX}" STREQUAL "")
		set(_fo_binary_output_postfix "-${FO_BINARY_OUTPUT_POSTFIX}")
	endif()

	set(
		${outputVar}
		"${FO_OUTPUT_PATH}/Binaries/${targetName}-${FO_BUILD_PLATFORM}$<${expr_PrefixConfig}:-$<CONFIG>>${_fo_binary_output_postfix}")
endmacro()

macro(WriteBuildHash target)
	if(NOT FO_BUILD_LIBRARY)
		GetTargetProperty(outputDir ${target} RUNTIME_OUTPUT_DIRECTORY)
	else()
		GetTargetProperty(outputDir ${target} LIBRARY_OUTPUT_DIRECTORY)
	endif()

	set(buildHashCommand
		${CMAKE_COMMAND}
		-DHASH_FILE="${outputDir}/${target}.build-hash"
		-DGIT_ROOT="${FO_GIT_ROOT}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/${FO_ENGINE_ROOT}/BuildTools/cmake/helpers/WriteBuildHash.cmake")
	set(removeBuildHashCommand
		${CMAKE_COMMAND}
		-E remove -f "${outputDir}/${target}.build-hash")

	AddCustomCommand(TARGET ${target} PRE_BUILD
		COMMAND ${removeBuildHashCommand})
	AddCustomCommand(TARGET ${target} POST_BUILD
		COMMAND ${buildHashCommand})
endmacro()

macro(SetBuildPlatformInfo buildPlatform monoOs monoArch)
	set(FO_BUILD_PLATFORM "${buildPlatform}")
	set(FO_MONO_OS "${monoOs}")
	set(FO_MONO_ARCH "${monoArch}")
endmacro()

macro(FindAndAppendLibraries outputVar)
	foreach(libraryName ${ARGN})
		FindLibrary(foundLibrary ${libraryName})
		AppendList(${outputVar} ${foundLibrary})
		unset(foundLibrary)
	endforeach()
endmacro()

macro(RequireNonEmptyVariables)
	foreach(var ${ARGN})
		if("${${var}}" STREQUAL "")
			AbortMessage("${var} not specified")
		endif()
	endforeach()
endmacro()

macro(DetectProcessorArchitecture outputVar processorName)
	if("${${processorName}}" STREQUAL "AMD64" OR "${${processorName}}" STREQUAL "x86_64")
		set(${outputVar} "x64")
	elseif("${${processorName}}" STREQUAL "i386" OR
		"${${processorName}}" STREQUAL "i486" OR
		"${${processorName}}" STREQUAL "i586" OR
		"${${processorName}}" STREQUAL "i686")
		set(${outputVar} "x86")
	elseif("${${processorName}}" STREQUAL "armv7-a")
		set(${outputVar} "arm")
	elseif("${${processorName}}" STREQUAL "aarch64")
		set(${outputVar} "arm64")
	else()
		set(${outputVar} ${${processorName}})
	endif()
endmacro()

macro(AddStaticThirdPartyLibrary target)
	set(options)
	set(oneValueArgs SOURCE_LIST APPEND_TO)
	set(multiValueArgs INCLUDE_DIRS LINK_LIBS)
	ParseArguments(STATIC_LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	StatusMessage("+ ${target}")

	if(STATIC_LIB_INCLUDE_DIRS)
		AddIncludeDirectories(${STATIC_LIB_INCLUDE_DIRS})
	endif()

	set(_fo_prev_folder "${CMAKE_FOLDER}")
	set(CMAKE_FOLDER "ThirdParty")
	AddLibrary(${target} STATIC EXCLUDE_FROM_ALL ${${STATIC_LIB_SOURCE_LIST}})
	set(CMAKE_FOLDER "${_fo_prev_folder}")
	SetTargetProperty(${target} FOLDER "ThirdParty")

	if(STATIC_LIB_LINK_LIBS)
		TargetLinkLibraries(${target} ${STATIC_LIB_LINK_LIBS})
	endif()

	if(STATIC_LIB_APPEND_TO)
		AppendList(${STATIC_LIB_APPEND_TO} ${target})
	endif()

	DisableLibWarnings(${target})
endmacro()

macro(AddCommandTarget target)
	set(options)
	set(oneValueArgs WORKING_DIRECTORY COMMENT)
	set(multiValueArgs COMMAND_ARGS DEPENDS SOURCES)
	ParseArguments(CMD_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	set(commandTargetArgs "")

	if(CMD_TARGET_COMMAND_ARGS)
		AppendList(commandTargetArgs ${CMD_TARGET_COMMAND_ARGS})
	endif()

	if(CMD_TARGET_DEPENDS)
		AppendList(commandTargetArgs DEPENDS ${CMD_TARGET_DEPENDS})
	endif()

	if(CMD_TARGET_SOURCES)
		AppendList(commandTargetArgs SOURCES ${CMD_TARGET_SOURCES})
	endif()

	if(DEFINED CMD_TARGET_WORKING_DIRECTORY)
		AppendList(commandTargetArgs WORKING_DIRECTORY ${CMD_TARGET_WORKING_DIRECTORY})
	endif()

	if(DEFINED CMD_TARGET_COMMENT)
		AppendList(commandTargetArgs COMMENT "${CMD_TARGET_COMMENT}")
	endif()

	set(_fo_prev_folder "${CMAKE_FOLDER}")
	set(CMAKE_FOLDER "Commands")
	AddCustomTarget(${target} ${commandTargetArgs})
	set(CMAKE_FOLDER "${_fo_prev_folder}")
	SetTargetProperty(${target} FOLDER "Commands")
	AppendList(FO_COMMANDS_GROUP ${target})
endmacro()

macro(AddCoreStaticLibrary target sourceList)
	set(options)
	set(oneValueArgs APPEND_TO_GROUP)
	set(multiValueArgs LINK_LIBS)
	ParseArguments(CORE_LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	StatusMessage("+ ${target}")
	if(CORE_LIB_APPEND_TO_GROUP STREQUAL "FO_CORE_LIBS_GROUP")
		set(_fo_prev_folder "${CMAKE_FOLDER}")
		set(CMAKE_FOLDER "CoreLibs")
	endif()
	AddLibrary(${target} STATIC EXCLUDE_FROM_ALL ${${sourceList}})
	if(CORE_LIB_APPEND_TO_GROUP STREQUAL "FO_CORE_LIBS_GROUP")
		set(CMAKE_FOLDER "${_fo_prev_folder}")
		SetTargetProperty(${target} FOLDER "CoreLibs")
	endif()
	AddDependencies(${target} ${FO_GEN_DEPENDENCIES})

	if(CORE_LIB_LINK_LIBS)
		TargetLinkLibraries(${target} ${CORE_LIB_LINK_LIBS})
	endif()

	if(CORE_LIB_APPEND_TO_GROUP)
		AppendList(${CORE_LIB_APPEND_TO_GROUP} ${target})
	endif()
endmacro()

macro(SetupApplicationTarget target)
	set(options WRITE_BUILD_HASH)
	set(oneValueArgs OUTPUT_NAME TESTING_APP HEADLESS_APP)
	set(multiValueArgs PROPERTIES LINK_LIBS DEPENDS)
	ParseArguments(APP_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	StatusMessage("+ ${target}")
	AppendList(FO_APPLICATIONS_GROUP "${target}")
	SetTargetProperty(${target} FOLDER "Applications")

	if(APP_TARGET_DEPENDS)
		AddDependencies(${target} ${APP_TARGET_DEPENDS})
	endif()

	if(APP_TARGET_PROPERTIES)
		SetTargetProperties(${target} ${APP_TARGET_PROPERTIES})
	endif()

	if(DEFINED APP_TARGET_OUTPUT_NAME)
		SetTargetProperties(${target} OUTPUT_NAME "${APP_TARGET_OUTPUT_NAME}")
	endif()

	if(NOT APP_TARGET_TESTING_APP)
		set(APP_TARGET_TESTING_APP 0)
	endif()
	if(NOT APP_TARGET_HEADLESS_APP)
		set(APP_TARGET_HEADLESS_APP 0)
	endif()
	TargetCompileDefinitions(${target} PRIVATE
		"FO_TESTING_APP=${APP_TARGET_TESTING_APP}"
		"FO_HEADLESS_APP=${APP_TARGET_HEADLESS_APP}")

	if(APP_TARGET_LINK_LIBS)
		TargetLinkLibraries(${target} ${APP_TARGET_LINK_LIBS})
	endif()

	if(APP_TARGET_WRITE_BUILD_HASH)
		WriteBuildHash(${target})
	endif()
endmacro()

macro(AddExecutableApplication target sourceFile)
	set(options WIN32 WRITE_BUILD_HASH)
	set(oneValueArgs OUTPUT_DIR WORKING_DIRECTORY OUTPUT_NAME TESTING_APP HEADLESS_APP)
	set(multiValueArgs LINK_LIBS DEPENDS EXTRA_SOURCES)
	ParseArguments(APP_EXEC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if(APP_EXEC_WIN32)
		set(_fo_prev_folder "${CMAKE_FOLDER}")
		set(CMAKE_FOLDER "Applications")
		AddExecutable(${target} WIN32 "${sourceFile}" ${APP_EXEC_EXTRA_SOURCES})
	else()
		set(_fo_prev_folder "${CMAKE_FOLDER}")
		set(CMAKE_FOLDER "Applications")
		AddExecutable(${target} "${sourceFile}" ${APP_EXEC_EXTRA_SOURCES})
	endif()
	set(CMAKE_FOLDER "${_fo_prev_folder}")

	set(appExecArgs
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${APP_EXEC_OUTPUT_DIR}
		VS_DEBUGGER_WORKING_DIRECTORY ${APP_EXEC_WORKING_DIRECTORY}
		OUTPUT_NAME ${APP_EXEC_OUTPUT_NAME}
		TESTING_APP ${APP_EXEC_TESTING_APP}
		HEADLESS_APP ${APP_EXEC_HEADLESS_APP}
		LINK_LIBS ${APP_EXEC_LINK_LIBS}
		DEPENDS ${APP_EXEC_DEPENDS})

	if(APP_EXEC_WRITE_BUILD_HASH)
		AppendList(appExecArgs WRITE_BUILD_HASH)
	endif()

	SetupApplicationTarget(${target} ${appExecArgs})
endmacro()

macro(AddSharedApplication target sourceFile)
	set(options WRITE_BUILD_HASH NO_PREFIX)
	set(oneValueArgs OUTPUT_DIR OUTPUT_NAME TESTING_APP HEADLESS_APP)
	set(multiValueArgs LINK_LIBS DEPENDS EXTRA_PROPERTIES)
	ParseArguments(APP_SHARED "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	set(_fo_prev_folder "${CMAKE_FOLDER}")
	set(CMAKE_FOLDER "Applications")
	AddLibrary(${target} SHARED "${sourceFile}")
	set(CMAKE_FOLDER "${_fo_prev_folder}")

	set(appSharedProperties
		LIBRARY_OUTPUT_DIRECTORY ${APP_SHARED_OUTPUT_DIR})

	if(APP_SHARED_EXTRA_PROPERTIES)
		AppendList(appSharedProperties ${APP_SHARED_EXTRA_PROPERTIES})
	endif()

	set(appSharedArgs
		PROPERTIES ${appSharedProperties}
		OUTPUT_NAME ${APP_SHARED_OUTPUT_NAME}
		TESTING_APP ${APP_SHARED_TESTING_APP}
		HEADLESS_APP ${APP_SHARED_HEADLESS_APP}
		LINK_LIBS ${APP_SHARED_LINK_LIBS}
		DEPENDS ${APP_SHARED_DEPENDS})

	if(APP_SHARED_WRITE_BUILD_HASH)
		AppendList(appSharedArgs WRITE_BUILD_HASH)
	endif()

	SetupApplicationTarget(${target} ${appSharedArgs})

	if(APP_SHARED_NO_PREFIX)
		set_target_properties(${target} PROPERTIES PREFIX "")
	endif()
endmacro()

# Copy a runtime file (DLL, .so, .dylib, ...) next to the target's output
# binary as a POST_BUILD step. Silently does nothing if runtimePath is empty,
# the file does not exist, or the target has not been declared — so callers
# can wire it up unconditionally for optional dependencies (Steamworks SDK,
# Mono runtime, plugin DLLs, ...).
function(CopyRuntimeToTarget targetName runtimePath)
	if(NOT runtimePath OR NOT EXISTS "${runtimePath}" OR NOT TARGET ${targetName})
		return()
	endif()

	GetFilenameComponent(runtimeFileName "${runtimePath}" NAME)
	AddCustomCommand(TARGET ${targetName} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runtimePath}" "$<TARGET_FILE_DIR:${targetName}>"
		COMMENT "Copy ${runtimeFileName} runtime for ${targetName}")
endfunction()

# Wire one CMake target's output binary to be copied next to another's output
# binary as a POST_BUILD step, and ensure the producer is built first. Useful
# for shared libraries / plugins that must sit next to a host executable
# (e.g. Baker shared lib next to the Server, AngelScript debugger plugin next
# to the editor, ...). Silently does nothing if either target is missing.
function(CopyTargetRuntimeToTarget consumerTarget producerTarget)
	if(NOT TARGET ${consumerTarget} OR NOT TARGET ${producerTarget})
		return()
	endif()

	add_dependencies(${consumerTarget} ${producerTarget})
	AddCustomCommand(TARGET ${consumerTarget} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${producerTarget}>" "$<TARGET_FILE_DIR:${consumerTarget}>"
		COMMENT "Copy ${producerTarget} runtime for ${consumerTarget}")
endfunction()
