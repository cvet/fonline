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

IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/Commands.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/Options.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/Build.cmake")
IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/State.cmake")

macro(IncludeBuildTool)
	foreach(buildTool ${ARGV})
		if(EXISTS "${FO_BUILDTOOLS_DIR}/${buildTool}.cmake")
			IncludeFile("${FO_BUILDTOOLS_DIR}/${buildTool}.cmake")
		else()
			IncludeFile("${FO_BUILDTOOLS_DIR}/cmake/${buildTool}.cmake")
		endif()
	endforeach()
endmacro()

macro(StartProjectGeneration)
	IncludeBuildTool(StartGeneration)
endmacro()

macro(FinalizeProjectGeneration)
	IncludeBuildTool(FinalizeGeneration)
endmacro()
