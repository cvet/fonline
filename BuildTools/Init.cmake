cmake_minimum_required(VERSION 3.22)

# Option setter
function(SetOption var value)
	if(NOT DEFINED ${var})
		set(${var} ${value} PARENT_SCOPE)
	endif()
endfunction()

macro(SetValue)
	set(${ARGV})
endmacro()

SetValue(FO_BUILDTOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}")

macro(IncludeFile)
	include(${ARGV})
endmacro()

IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/helpers/Commands.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/helpers/Options.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/helpers/Build.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/helpers/State.cmake")

macro(IncludeBuildTool)
	foreach(buildTool ${ARGV})
		IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/${buildTool}.cmake")
	endforeach()
endmacro()

# ---------------------------------------------------------------------------
# find_package interception
#
# On Windows we route every find_package() call through an explicit registry
# of handler macros so that no third-party CMakeLists.txt accidentally pulls
# a system library install. Each looked-up package either gets remapped onto
# a bundled in-tree dependency or is explicitly allowed to fall through to
# CMake's stock search via PassThroughFindPackage.
#
# Use:
#   RegisterFindPackageHandler(<Name> <macro-name>) — register a handler
#                                                     for find_package(<Name>)
#   PassThroughFindPackage                          — built-in handler that
#                                                     forwards to the original
#                                                     find_package() (use for
#                                                     build tools / system
#                                                     packages we accept)
#
# The handler macro receives the original find_package() arguments, with the
# package name as the first positional argument. Variables set inside the
# handler land in the find_package() caller's scope, mirroring CMake's
# normal find_package semantics.
#
# The interceptor itself is installed at the top of the ThirdParty stage,
# before any AddSubdirectory() reaches a third-party tree.
# ---------------------------------------------------------------------------

macro(RegisterFindPackageHandler packageName handlerMacroName)
	set(_FO_FIND_PKG_HANDLER_${packageName} "${handlerMacroName}")
endmacro()

macro(PassThroughFindPackage)
	_find_package(${ARGV})
endmacro()

# Built-in handler: report the package as missing without reaching the host
# system. If the consumer marked it REQUIRED, abort configure with a clear
# diagnostic. Use for optional probes that we deliberately don't ship
# (e.g. SDL's LibUSB, glslang's SPIRV-Tools-opt, ...).
macro(NotFoundFindPackage _fo_nf_pkg)
	list(FIND ARGN "REQUIRED" _fo_nf_required_idx)
	if(NOT _fo_nf_required_idx EQUAL -1)
		AbortMessage("find_package(${_fo_nf_pkg}) is REQUIRED but the project routes it to NotFoundFindPackage. Either bundle the dependency and register a real handler, or stop the consumer from requesting it.")
	endif()
	set(${_fo_nf_pkg}_FOUND FALSE)
endmacro()

# ---------------------------------------------------------------------------
# Pipeline stages
#
# The build is structured as a strict sequence of named stages. Each stage
# is exposed via a public macro and must be invoked exactly once, in the
# canonical order. Stage helpers validate ordering: invoking a stage out of
# order, twice, or skipping it produces a hard error during CMake configure.
#
# Stage entry-point macros (call in this order, each exactly once):
#   1. StartProjectGeneration       (Init)
#   2. RegisterProjectOptions       (ProjectOptions)
#   3. AddThirdPartyLibraries       (ThirdParty)
#   4. RegisterEngineSources        (EngineSources)
#   5. SetupCodeGeneration          (Codegen)
#   6. BuildCoreLibraries           (CoreLibs)
#   7. BuildApplications            (Applications)
#   8. SetupScriptsAndBaking        (ScriptsAndBaking)
#   9. BuildPackages                (Packages)
#  10. FinalizeProjectGeneration    (Finalize)
#
# Each stage exposes Pre and Post hook lists. Hooks are macros (or functions)
# invoked in registration order at the boundaries of the stage. Register a
# hook with AddStageHook(<Stage> Pre|Post <macro-name>).
# ---------------------------------------------------------------------------

set(FO_KNOWN_STAGES
	Init
	ProjectOptions
	ThirdParty
	EngineSources
	Codegen
	CoreLibs
	Applications
	ScriptsAndBaking
	Packages
	Finalize)

set(FO_STAGES_EXECUTED "")

macro(AddStageHook stage when hookName)
	if(NOT "${stage}" IN_LIST FO_KNOWN_STAGES)
		AbortMessage("AddStageHook: unknown stage '${stage}'. Known: ${FO_KNOWN_STAGES}")
	endif()
	if(NOT "${when}" STREQUAL "Pre" AND NOT "${when}" STREQUAL "Post")
		AbortMessage("AddStageHook: 'when' must be Pre or Post (got '${when}')")
	endif()
	if("${stage}" IN_LIST FO_STAGES_EXECUTED)
		AbortMessage("AddStageHook: stage '${stage}' has already executed; hooks must be registered before the stage runs")
	endif()
	list(APPEND FO_HOOKS_${stage}_${when} "${hookName}")
endmacro()

macro(InvokeStageHooks stage when)
	foreach(_hook IN LISTS FO_HOOKS_${stage}_${when})
		cmake_language(CALL ${_hook})
	endforeach()
endmacro()

macro(_RunStage stage)
	# Validate stage name.
	list(FIND FO_KNOWN_STAGES "${stage}" _stage_index)
	if(_stage_index EQUAL -1)
		AbortMessage("Pipeline: unknown stage '${stage}'. Known: ${FO_KNOWN_STAGES}")
	endif()

	# Each stage runs exactly once.
	if("${stage}" IN_LIST FO_STAGES_EXECUTED)
		AbortMessage("Pipeline: stage '${stage}' has already been executed; each stage must run exactly once")
	endif()

	# All preceding stages must have run.
	if(_stage_index GREATER 0)
		math(EXPR _last_required "${_stage_index} - 1")
		foreach(_idx RANGE 0 ${_last_required})
			list(GET FO_KNOWN_STAGES ${_idx} _prev_stage)
			if(NOT "${_prev_stage}" IN_LIST FO_STAGES_EXECUTED)
				AbortMessage("Pipeline: stage '${stage}' invoked before '${_prev_stage}'. The canonical order is: ${FO_KNOWN_STAGES}")
			endif()
		endforeach()
	endif()

	InvokeStageHooks(${stage} Pre)
	IncludeBuildTool(stages/${stage})
	InvokeStageHooks(${stage} Post)
	list(APPEND FO_STAGES_EXECUTED "${stage}")
endmacro()

# Public stage entry points — strict, no auto-cascade. Calling a stage out of
# sequence, twice, or skipping any predecessor aborts CMake configure.
macro(StartProjectGeneration)
	_RunStage(Init)
endmacro()

macro(RegisterProjectOptions)
	_RunStage(ProjectOptions)
endmacro()

macro(AddThirdPartyLibraries)
	_RunStage(ThirdParty)
endmacro()

macro(RegisterEngineSources)
	_RunStage(EngineSources)
endmacro()

macro(SetupCodeGeneration)
	_RunStage(Codegen)
endmacro()

macro(BuildCoreLibraries)
	_RunStage(CoreLibs)
endmacro()

macro(BuildApplications)
	_RunStage(Applications)
endmacro()

macro(SetupScriptsAndBaking)
	_RunStage(ScriptsAndBaking)
endmacro()

macro(BuildPackages)
	_RunStage(Packages)
endmacro()

macro(FinalizeProjectGeneration)
	_RunStage(Finalize)

	# Sanity check: by the time Finalize finishes, every stage must have run.
	foreach(_stage IN LISTS FO_KNOWN_STAGES)
		if(NOT "${_stage}" IN_LIST FO_STAGES_EXECUTED)
			AbortMessage("Pipeline: stage '${_stage}' was never executed. Project must call every stage in order before FinalizeProjectGeneration().")
		endif()
	endforeach()
endmacro()
