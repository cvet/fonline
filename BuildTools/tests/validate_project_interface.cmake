cmake_minimum_required(VERSION 3.22)

get_filename_component(_engineSourceRoot "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${_engineSourceRoot}/BuildTools/Init.cmake")

set(_expectedStages
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

if(NOT "${FO_KNOWN_STAGES}" STREQUAL "${_expectedStages}")
	message(FATAL_ERROR "Unexpected project-interface stage order: ${FO_KNOWN_STAGES}")
endif()

foreach(_stage IN LISTS FO_KNOWN_STAGES)
	set(_entrypoint "${FO_STAGE_ENTRYPOINT_${_stage}}")
	if(NOT COMMAND ${_entrypoint})
		message(FATAL_ERROR "Missing generated stage entrypoint ${_entrypoint} for ${_stage}")
	endif()
	if(NOT "${FO_STAGE_HOOKS_${_stage}}" STREQUAL "Pre;Post")
		message(FATAL_ERROR "Unexpected hook surface for ${_stage}: ${FO_STAGE_HOOKS_${_stage}}")
	endif()
endforeach()

string(JSON _optionCount LENGTH "${FO_PROJECT_INTERFACE_JSON}" options)
string(JSON _stageCount LENGTH "${FO_PROJECT_INTERFACE_JSON}" stages)
string(JSON _helperCount LENGTH "${FO_PROJECT_INTERFACE_JSON}" helpers)
if(NOT _optionCount EQUAL 43 OR NOT _stageCount EQUAL 10 OR NOT _helperCount EQUAL 5)
	message(FATAL_ERROR "Unexpected project-interface shape: ${_optionCount} options, ${_stageCount} stages, ${_helperCount} helpers")
endif()

set(FO_ENGINE_ROOT ".")
DeclareProjectInterfaceOptions(FO_PROJECT_INTERFACE_JSON)
math(EXPR _lastOptionIndex "${_optionCount} - 1")
foreach(_optionIndex RANGE 0 ${_lastOptionIndex})
	string(JSON _optionName GET "${FO_PROJECT_INTERFACE_JSON}" options ${_optionIndex} name)
	if(NOT DEFINED ${_optionName})
		message(FATAL_ERROR "Project-interface option was not declared: ${_optionName}")
	endif()
endforeach()

message(STATUS "Validated FOnline project interface: ${_optionCount} options, ${_stageCount} stages, ${_helperCount} helpers")
