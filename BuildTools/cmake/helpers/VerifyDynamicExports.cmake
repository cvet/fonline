if(NOT DEFINED NM_EXECUTABLE OR "${NM_EXECUTABLE}" STREQUAL "")
	message(FATAL_ERROR "NM_EXECUTABLE is required")
endif()
if(NOT DEFINED LIBRARY_PATH OR NOT EXISTS "${LIBRARY_PATH}")
	message(FATAL_ERROR "LIBRARY_PATH does not exist: ${LIBRARY_PATH}")
endif()
if(NOT DEFINED EXPECTED_EXPORTS OR "${EXPECTED_EXPORTS}" STREQUAL "")
	message(FATAL_ERROR "EXPECTED_EXPORTS is required")
endif()

execute_process(
	COMMAND "${NM_EXECUTABLE}" --dynamic --defined-only --extern-only --format=posix "${LIBRARY_PATH}"
	RESULT_VARIABLE nmResult
	OUTPUT_VARIABLE nmOutput
	ERROR_VARIABLE nmError)
if(NOT nmResult EQUAL 0)
	message(FATAL_ERROR "Failed to inspect dynamic exports of ${LIBRARY_PATH}: ${nmError}")
endif()

string(REPLACE "\r\n" "\n" nmOutput "${nmOutput}")
string(REPLACE "\n" ";" nmLines "${nmOutput}")
set(actualExports)
foreach(nmLine IN LISTS nmLines)
	string(STRIP "${nmLine}" nmLine)
	if("${nmLine}" STREQUAL "")
		continue()
	endif()

	string(REGEX MATCH "^([^ \t]+)[ \t]+[^ \t]+" exportMatch "${nmLine}")
	if(NOT exportMatch)
		message(FATAL_ERROR "Unexpected nm output for ${LIBRARY_PATH}: ${nmLine}")
	endif()
	list(APPEND actualExports "${CMAKE_MATCH_1}")
endforeach()

list(SORT actualExports)
list(SORT EXPECTED_EXPORTS)
if(NOT "${actualExports}" STREQUAL "${EXPECTED_EXPORTS}")
	message(FATAL_ERROR
		"Unexpected dynamic exports in ${LIBRARY_PATH}\n"
		"Expected: ${EXPECTED_EXPORTS}\n"
		"Actual: ${actualExports}")
endif()

message(STATUS "Verified dynamic exports of ${LIBRARY_PATH}: ${actualExports}")
