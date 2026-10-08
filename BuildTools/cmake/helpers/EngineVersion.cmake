include_guard()

function(ReadEngineVersion engineRoot outputVariable gitDependenciesVariable)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c
            "import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); from engine_version import read_engine_version; print(read_engine_version(Path(sys.argv[2])))"
            "${engineRoot}/BuildTools" "${engineRoot}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE version
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Unable to load Engine VERSION: ${error}")
    endif()
    set(${outputVariable} "${version}" PARENT_SCOPE)
    set(gitDependencies "")
    if(EXISTS "${engineRoot}/.git")
        execute_process(COMMAND git -C "${engineRoot}" symbolic-ref -q HEAD
            OUTPUT_VARIABLE branch OUTPUT_STRIP_TRAILING_WHITESPACE)
        foreach(ref HEAD "${branch}" packed-refs)
            if(ref STREQUAL "")
                continue()
            endif()
            execute_process(COMMAND git -C "${engineRoot}" rev-parse --git-path "${ref}"
                RESULT_VARIABLE gitResult OUTPUT_VARIABLE gitPath OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(gitResult STREQUAL "0")
                get_filename_component(gitPath "${gitPath}" ABSOLUTE BASE_DIR "${engineRoot}")
                if(EXISTS "${gitPath}")
                    list(APPEND gitDependencies "${gitPath}")
                elseif(ref STREQUAL branch)
                    # A packed branch creates a loose ref on its next commit
                    get_filename_component(refDirectory "${gitPath}" DIRECTORY)
                    while(NOT EXISTS "${refDirectory}")
                        get_filename_component(refDirectory "${refDirectory}" DIRECTORY)
                    endwhile()
                    list(APPEND gitDependencies "${refDirectory}")
                endif()
            endif()
        endforeach()
    endif()
    set(${gitDependenciesVariable} "${gitDependencies}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${engineRoot}/VERSION" "${engineRoot}/BuildTools/engine_version.py" ${gitDependencies})
endfunction()
