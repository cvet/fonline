cmake_minimum_required(VERSION 3.22)

get_filename_component(_engineSourceRoot "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${_engineSourceRoot}/BuildTools/Init.cmake")

if(FO_NATIVE_EXTENSION_TEST_ODD_ARGS)
	AddEngineSources(COMMON)
	message(FATAL_ERROR "Odd native extension role/path argument count unexpectedly passed")
endif()

set(FO_CONTRIBUTION_DIR "${_engineSourceRoot}")
file(READ "${_engineSourceRoot}/BuildTools/NativeExtensionInterface.json" _nativeInterfaceJson)
file(READ "${_engineSourceRoot}/BuildTools/cmake/ProjectInterface.json" _projectInterfaceJson)

string(JSON _roleCount LENGTH "${_nativeInterfaceJson}" roles)
string(JSON _hookCount LENGTH "${_nativeInterfaceJson}" hooks)
string(JSON _bindingCount LENGTH "${_nativeInterfaceJson}" binding_rules)
if(NOT _roleCount EQUAL 6 OR NOT _hookCount EQUAL 8 OR NOT _bindingCount EQUAL 6)
	message(FATAL_ERROR "Unexpected native extension interface shape: ${_roleCount} roles, ${_hookCount} hooks, ${_bindingCount} binding rules")
endif()

set(_documentedRoles "")
math(EXPR _lastRoleIndex "${_roleCount} - 1")
foreach(_roleIndex RANGE 0 ${_lastRoleIndex})
	string(JSON _roleName GET "${_nativeInterfaceJson}" roles ${_roleIndex} name)
	list(APPEND _documentedRoles "${_roleName}")
endforeach()
set(_expectedRoles COMMON SERVER CLIENT MAPPER BAKER TESTS)
if(NOT "${_documentedRoles}" STREQUAL "${_expectedRoles}")
	message(FATAL_ERROR "Unexpected documented native extension roles: ${_documentedRoles}")
endif()

string(JSON _helperCount LENGTH "${_projectInterfaceJson}" helpers)
set(_projectRoles "")
math(EXPR _lastHelperIndex "${_helperCount} - 1")
foreach(_helperIndex RANGE 0 ${_lastHelperIndex})
	string(JSON _helperName GET "${_projectInterfaceJson}" helpers ${_helperIndex} name)
	if(_helperName STREQUAL "AddEngineSources")
		string(JSON _projectRoleCount LENGTH "${_projectInterfaceJson}" helpers ${_helperIndex} allowed_roles)
		math(EXPR _lastProjectRoleIndex "${_projectRoleCount} - 1")
		foreach(_projectRoleIndex RANGE 0 ${_lastProjectRoleIndex})
			string(JSON _projectRole GET "${_projectInterfaceJson}" helpers ${_helperIndex} allowed_roles ${_projectRoleIndex})
			list(APPEND _projectRoles "${_projectRole}")
		endforeach()
	endif()
endforeach()
if(NOT "${_documentedRoles}" STREQUAL "${_projectRoles}")
	message(FATAL_ERROR "Native extension roles differ from ProjectInterface.json: native='${_documentedRoles}', project='${_projectRoles}'")
endif()

AddEngineSource(SERVER "Examples/MinimalProject/StarterServerExtension.cpp")
get_filename_component(_expectedServerSource "${_engineSourceRoot}/Examples/MinimalProject/StarterServerExtension.cpp" ABSOLUTE)
if(NOT "${_expectedServerSource}" IN_LIST FO_SERVER_SOURCE)
	message(FATAL_ERROR "SERVER extension source was not routed to FO_SERVER_SOURCE")
endif()
if(NOT "${_expectedServerSource}" IN_LIST FO_SOURCE_META_FILES)
	message(FATAL_ERROR "SERVER extension source was not routed to FO_SOURCE_META_FILES")
endif()

AddEngineSource(COMMON "Source/Common/Common.h")
get_filename_component(_expectedCommonHeader "${_engineSourceRoot}/Source/Common/Common.h" ABSOLUTE)
if(NOT "${_expectedCommonHeader}" IN_LIST FO_COMMON_SOURCE)
	message(FATAL_ERROR "COMMON extension header was not routed to FO_COMMON_SOURCE")
endif()
if(NOT "${_expectedCommonHeader}" IN_LIST FO_ADDED_COMMON_HEADERS)
	message(FATAL_ERROR "COMMON extension header was not routed to FO_ADDED_COMMON_HEADERS")
endif()

AddEngineSource(TESTS "Source/Tests/Test_EngineMetadata.cpp")
get_filename_component(_expectedTestSource "${_engineSourceRoot}/Source/Tests/Test_EngineMetadata.cpp" ABSOLUTE)
if(NOT "${_expectedTestSource}" IN_LIST FO_TESTS_SOURCE)
	message(FATAL_ERROR "TESTS extension source was not routed to FO_TESTS_SOURCE")
endif()
if(NOT "${_expectedTestSource}" IN_LIST FO_SOURCE_META_FILES)
	message(FATAL_ERROR "TESTS extension source was not routed to FO_SOURCE_META_FILES")
endif()

# Current CMake does not reject an arbitrary role token. It creates the
# corresponding FO_<ROLE>_SOURCE list, but only the documented roles above have
# known library/application consumers.
AddEngineSource(EDITOR "Source/Common/Common.h")
if(NOT "${_expectedCommonHeader}" IN_LIST FO_EDITOR_SOURCE)
	message(FATAL_ERROR "Current arbitrary-role routing behavior changed unexpectedly")
endif()

list(APPEND FO_TESTING_LIBS project-native-test-library)
if(NOT "project-native-test-library" IN_LIST FO_TESTING_LIBS)
	message(FATAL_ERROR "TESTS project library was not routed to FO_TESTING_LIBS")
endif()

execute_process(
	COMMAND "${CMAKE_COMMAND}"
		-DFO_NATIVE_EXTENSION_TEST_ODD_ARGS=ON
		-P "${CMAKE_CURRENT_LIST_FILE}"
	RESULT_VARIABLE _oddArgsResult
	OUTPUT_VARIABLE _oddArgsOutput
	ERROR_VARIABLE _oddArgsError)
if(_oddArgsResult EQUAL 0)
	message(FATAL_ERROR "Odd native extension role/path validation unexpectedly passed")
endif()
set(_oddArgsCombined "${_oddArgsOutput}\n${_oddArgsError}")
if(NOT _oddArgsCombined MATCHES "AddEngineSources expects pairs of arguments")
	message(FATAL_ERROR "Odd native extension argument diagnostic is missing: ${_oddArgsCombined}")
endif()

message(STATUS "Validated documented native extension interface: ${_roleCount} consumed roles, ${_hookCount} hooks, ${_bindingCount} binding rules")
