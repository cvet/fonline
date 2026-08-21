cmake_minimum_required(VERSION 3.22)

get_filename_component(_engineSourceRoot "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${_engineSourceRoot}/BuildTools/Init.cmake")

set(FO_CONTRIBUTION_DIR "${_engineSourceRoot}")

if(FO_NATIVE_EXTENSION_TEST_INVALID_ROLE)
	AddEngineSource(EDITOR "Examples/MinimalProject/StarterServerExtension.cpp")
	message(FATAL_ERROR "Unknown native extension role unexpectedly passed")
endif()

set(_expectedRoles COMMON SERVER CLIENT MAPPER BAKER TESTS)
if(NOT "${FO_NATIVE_EXTENSION_ROLES}" STREQUAL "${_expectedRoles}")
	message(FATAL_ERROR "Unexpected native extension roles: ${FO_NATIVE_EXTENSION_ROLES}")
endif()

string(JSON _roleCount LENGTH "${FO_NATIVE_EXTENSION_INTERFACE_JSON}" roles)
string(JSON _hookCount LENGTH "${FO_NATIVE_EXTENSION_INTERFACE_JSON}" hooks)
string(JSON _bindingCount LENGTH "${FO_NATIVE_EXTENSION_INTERFACE_JSON}" binding_rules)
if(NOT _roleCount EQUAL 6 OR NOT _hookCount EQUAL 8 OR NOT _bindingCount EQUAL 6)
	message(FATAL_ERROR "Unexpected native extension interface shape: ${_roleCount} roles, ${_hookCount} hooks, ${_bindingCount} binding rules")
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

AddEngineSource(TESTS "Source/Tests/Test_EngineMetadata.cpp")
get_filename_component(_expectedTestSource "${_engineSourceRoot}/Source/Tests/Test_EngineMetadata.cpp" ABSOLUTE)
if(NOT "${_expectedTestSource}" IN_LIST FO_TESTS_SOURCE)
	message(FATAL_ERROR "TESTS extension source was not routed to FO_TESTS_SOURCE")
endif()
if(NOT "${_expectedTestSource}" IN_LIST FO_SOURCE_META_FILES)
	message(FATAL_ERROR "TESTS extension source was not routed to FO_SOURCE_META_FILES")
endif()

AddProjectLibraries(ROLES TESTS LIBRARIES project-native-test-library)
if(NOT "project-native-test-library" IN_LIST FO_TESTING_LIBS)
	message(FATAL_ERROR "TESTS project library was not routed to FO_TESTING_LIBS")
endif()
if(NOT "${_expectedCommonHeader}" IN_LIST FO_ADDED_COMMON_HEADERS)
	message(FATAL_ERROR "COMMON extension header was not routed to FO_ADDED_COMMON_HEADERS")
endif()

execute_process(
	COMMAND "${CMAKE_COMMAND}"
		-DFO_NATIVE_EXTENSION_TEST_INVALID_ROLE=ON
		-P "${CMAKE_CURRENT_LIST_FILE}"
	RESULT_VARIABLE _invalidRoleResult
	OUTPUT_VARIABLE _invalidRoleOutput
	ERROR_VARIABLE _invalidRoleError)
if(_invalidRoleResult EQUAL 0)
	message(FATAL_ERROR "Unknown native extension role validation unexpectedly passed")
endif()
set(_invalidRoleCombined "${_invalidRoleOutput}\n${_invalidRoleError}")
if(NOT _invalidRoleCombined MATCHES "unknown native extension role 'EDITOR'")
	message(FATAL_ERROR "Unknown native extension role diagnostic is missing: ${_invalidRoleCombined}")
endif()

message(STATUS "Validated native extension interface: ${_roleCount} roles, ${_hookCount} hooks, ${_bindingCount} binding rules")
