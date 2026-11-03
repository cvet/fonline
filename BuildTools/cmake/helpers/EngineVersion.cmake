include_guard()

function(CollectGitHeadDependencies gitRoot outputVariable)
    set(gitDependencies "")
    if(EXISTS "${gitRoot}/.git")
        execute_process(COMMAND git -C "${gitRoot}" symbolic-ref -q HEAD
            OUTPUT_VARIABLE branch OUTPUT_STRIP_TRAILING_WHITESPACE)
        foreach(ref HEAD "${branch}" packed-refs)
            if(ref STREQUAL "")
                continue()
            endif()
            execute_process(COMMAND git -C "${gitRoot}" rev-parse --git-path "${ref}"
                RESULT_VARIABLE gitResult OUTPUT_VARIABLE gitPath OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(gitResult STREQUAL "0")
                get_filename_component(gitPath "${gitPath}" ABSOLUTE BASE_DIR "${gitRoot}")
                if(EXISTS "${gitPath}")
                    list(APPEND gitDependencies "${gitPath}")
                elseif(ref STREQUAL branch)
                    # Watch creation before Visual Studio loads target projects; exported dependencies remain files
                    get_filename_component(refDirectory "${gitPath}" DIRECTORY)
                    while(NOT IS_DIRECTORY "${refDirectory}")
                        get_filename_component(refDirectory "${refDirectory}" DIRECTORY)
                    endwhile()
                    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${refDirectory}")
                    file(GLOB looseBranchRef CONFIGURE_DEPENDS "${gitPath}")
                    list(APPEND gitDependencies ${looseBranchRef})
                endif()
            endif()
        endforeach()
    endif()
    set(${outputVariable} "${gitDependencies}" PARENT_SCOPE)
endfunction()

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
    CollectGitHeadDependencies("${engineRoot}" gitDependencies)
    set(${gitDependenciesVariable} "${gitDependencies}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${engineRoot}/VERSION" "${engineRoot}/BuildTools/engine_version.py" ${gitDependencies})
endfunction()

