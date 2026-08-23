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

# Route find_package through registered remaps or explicit PassThroughFindPackage handlers.
# ThirdParty installs interception before any vendored AddSubdirectory call

macro(RegisterFindPackageHandler packageName handlerMacroName)
	set(_FO_FIND_PKG_HANDLER_${packageName} "${handlerMacroName}")
endmacro()

macro(PassThroughFindPackage)
	_find_package(${ARGV})
endmacro()

# Report a package missing without probing the host; abort clearly when REQUIRED
macro(NotFoundFindPackage _fo_nf_pkg)
	list(FIND ARGN "REQUIRED" _fo_nf_required_idx)
	if(NOT _fo_nf_required_idx EQUAL -1)
		AbortMessage("find_package(${_fo_nf_pkg}) is REQUIRED but the project routes it to NotFoundFindPackage. Either bundle the dependency and register a real handler, or stop the consumer from requesting it.")
	endif()
	set(${_fo_nf_pkg}_FOUND FALSE)
endmacro()

# Invoke every public pipeline stage exactly once in its validated canonical order.
# AddStageHook registers ordered Pre or Post extensions at stage boundaries

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
	# Validate stage name
	list(FIND FO_KNOWN_STAGES "${stage}" _stage_index)
	if(_stage_index EQUAL -1)
		AbortMessage("Pipeline: unknown stage '${stage}'. Known: ${FO_KNOWN_STAGES}")
	endif()

	# Each stage runs exactly once
	if("${stage}" IN_LIST FO_STAGES_EXECUTED)
		AbortMessage("Pipeline: stage '${stage}' has already been executed; each stage must run exactly once")
	endif()

	# All preceding stages must have run
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
# sequence, twice, or skipping any predecessor aborts CMake configure
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

	# Finalize requires every stage to have run
	foreach(_stage IN LISTS FO_KNOWN_STAGES)
		if(NOT "${_stage}" IN_LIST FO_STAGES_EXECUTED)
			AbortMessage("Pipeline: stage '${_stage}' was never executed. Project must call every stage in order before FinalizeProjectGeneration().")
		endif()
	endforeach()
endmacro()
