cmake_minimum_required(VERSION 3.22)

get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(FO_VERBOSE_BUILD OFF)

file(READ "${ENGINE_ROOT}/BuildTools/PackageInterface.json" PACKAGE_INTERFACE_JSON)
string(JSON PACKAGE_SCHEMA GET "${PACKAGE_INTERFACE_JSON}" schema_version)
if(NOT PACKAGE_SCHEMA EQUAL 1)
    message(FATAL_ERROR "Unexpected package interface schema: ${PACKAGE_SCHEMA}")
endif()

string(JSON PACKAGE_COMMAND GET "${PACKAGE_INTERFACE_JSON}" declaration command)
if(NOT PACKAGE_COMMAND STREQUAL "DefinePackage")
    message(FATAL_ERROR "Unexpected package declaration command: ${PACKAGE_COMMAND}")
endif()

string(JSON PACKAGE_TARGET_COUNT LENGTH "${PACKAGE_INTERFACE_JSON}" targets)
string(JSON PACKAGE_PLATFORM_COUNT LENGTH "${PACKAGE_INTERFACE_JSON}" platforms)
string(JSON PACKAGE_PACK_COUNT LENGTH "${PACKAGE_INTERFACE_JSON}" packs)
if(NOT PACKAGE_TARGET_COUNT EQUAL 6 OR NOT PACKAGE_PLATFORM_COUNT EQUAL 6 OR NOT PACKAGE_PACK_COUNT EQUAL 19)
    message(FATAL_ERROR "Unexpected package interface dimensions")
endif()

include("${ENGINE_ROOT}/BuildTools/cmake/helpers/Commands.cmake")
include("${ENGINE_ROOT}/BuildTools/cmake/helpers/Build.cmake")

DefinePackage(DocsPackageContract
    CONFIG Smoke
    BINARY Server Windows win64 Raw+Zip POSTFIX Docs
    BINARY Client Web wasm Raw+WebServer)

if(NOT FO_PACKAGES STREQUAL "DocsPackageContract")
    message(FATAL_ERROR "DefinePackage did not register the package")
endif()
if(NOT Package_DocsPackageContract_Config STREQUAL "Smoke")
    message(FATAL_ERROR "DefinePackage did not retain CONFIG")
endif()
list(LENGTH Package_DocsPackageContract_Parts PACKAGE_PART_COUNT)
if(NOT PACKAGE_PART_COUNT EQUAL 2)
    message(FATAL_ERROR "DefinePackage did not retain both BINARY clauses")
endif()
list(GET Package_DocsPackageContract_Parts 0 PACKAGE_SERVER_PART)
if(NOT PACKAGE_SERVER_PART STREQUAL "Server,Windows,win64,Raw+Zip,,Docs")
    message(FATAL_ERROR "DefinePackage did not retain per-binary POSTFIX")
endif()
list(GET Package_DocsPackageContract_Parts 1 PACKAGE_WEB_PART)
if(NOT PACKAGE_WEB_PART STREQUAL "Client,Web,wasm,Raw+WebServer,,")
    message(FATAL_ERROR "DefinePackage leaked POSTFIX into the sibling BINARY")
endif()
