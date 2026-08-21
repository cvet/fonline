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
if(NOT _optionCount EQUAL 44 OR NOT _stageCount EQUAL 10 OR NOT _helperCount EQUAL 7)
	message(FATAL_ERROR "Unexpected project-interface shape: ${_optionCount} options, ${_stageCount} stages, ${_helperCount} helpers")
endif()

math(EXPR _lastHelperIndex "${_helperCount} - 1")
foreach(_helperIndex RANGE 0 ${_lastHelperIndex})
	string(JSON _helperName GET "${FO_PROJECT_INTERFACE_JSON}" helpers ${_helperIndex} name)
	if(NOT COMMAND ${_helperName})
		message(FATAL_ERROR "Missing public project helper command: ${_helperName}")
	endif()
endforeach()

if(FO_PROJECT_LIBRARY_TEST_INVALID_ROLE)
	AddProjectLibraries(ROLES EDITOR LIBRARIES InvalidProjectLibrary)
	message(FATAL_ERROR "Unknown project library role unexpectedly passed")
endif()

AddProjectLibraries(
	ROLES COMMON SERVER
	LIBRARIES ProjectShared ProjectNetwork)
AddProjectLibraries(
	ROLES MAPPER
	LIBRARIES ProjectMapper)
if(NOT "ProjectShared" IN_LIST FO_COMMON_LIBS OR NOT "ProjectNetwork" IN_LIST FO_COMMON_LIBS)
	message(FATAL_ERROR "COMMON project libraries were not routed to FO_COMMON_LIBS: ${FO_COMMON_LIBS}")
endif()
if(NOT "ProjectShared" IN_LIST FO_SERVER_LIBS OR NOT "ProjectNetwork" IN_LIST FO_SERVER_LIBS)
	message(FATAL_ERROR "SERVER project libraries were not routed to FO_SERVER_LIBS: ${FO_SERVER_LIBS}")
endif()
if(NOT "ProjectMapper" IN_LIST FO_MAPPER_LIBS)
	message(FATAL_ERROR "MAPPER project library was not routed to FO_MAPPER_LIBS: ${FO_MAPPER_LIBS}")
endif()

execute_process(
	COMMAND "${CMAKE_COMMAND}"
		-DFO_PROJECT_LIBRARY_TEST_INVALID_ROLE=ON
		-P "${CMAKE_CURRENT_LIST_FILE}"
	RESULT_VARIABLE _invalidProjectLibraryRoleResult
	OUTPUT_VARIABLE _invalidProjectLibraryRoleOutput
	ERROR_VARIABLE _invalidProjectLibraryRoleError)
if(_invalidProjectLibraryRoleResult EQUAL 0)
	message(FATAL_ERROR "Unknown project library role validation unexpectedly passed")
endif()
set(_invalidProjectLibraryRoleCombined "${_invalidProjectLibraryRoleOutput}\n${_invalidProjectLibraryRoleError}")
if(NOT _invalidProjectLibraryRoleCombined MATCHES "unknown project library role 'EDITOR'")
	message(FATAL_ERROR "Unknown project library role diagnostic is missing: ${_invalidProjectLibraryRoleCombined}")
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
