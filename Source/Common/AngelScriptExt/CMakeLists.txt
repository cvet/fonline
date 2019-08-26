cmake_minimum_required( VERSION 2.8 )
project( AngelscriptExt )

set( "ANGEL_SCRIPT_SDK" "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/AngelScript/sdk" )

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}" )
include_directories( "${ANGEL_SCRIPT_SDK}/angelscript/include" )
include_directories( "${ANGEL_SCRIPT_SDK}/angelscript/source" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptstdstring" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptarray" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptdictionary" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptfile" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptany" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/datetime" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scriptmath" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/weakref" )
include_directories( "${ANGEL_SCRIPT_SDK}/add_on/scripthelper" )

file( GLOB AS_SOURCE "*.*" )
file( GLOB AS_STRING_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptstdstring/*.*" )
file( GLOB AS_ARRAY_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptarray/*.*" )
file( GLOB AS_DICTIONARY_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptdictionary/*.*" )
file( GLOB AS_FILE_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptfile/*.*" )
file( GLOB AS_ANY_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptany/*.*" )
file( GLOB AS_DATE_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/datetime/*.*" )
file( GLOB AS_MATH_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scriptmath/*.*" )
file( GLOB AS_WEAKREF_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/weakref/*.*" )
file( GLOB AS_HELPER_SOURCE "${ANGEL_SCRIPT_SDK}/add_on/scripthelper/*.*" )

add_definitions( "-DAS_NO_EXCEPTIONS -DAS_NO_THREADS" )

add_library( AngelscriptExt ${AS_SOURCE} ${AS_MATH_SOURCE} ${AS_WEAKREF_SOURCE} ${AS_HELPER_SOURCE}
	${AS_STRING_SOURCE} ${AS_ARRAY_SOURCE} ${AS_DICTIONARY_SOURCE} ${AS_FILE_SOURCE}
	${AS_ANY_SOURCE} ${AS_DATE_SOURCE} )
