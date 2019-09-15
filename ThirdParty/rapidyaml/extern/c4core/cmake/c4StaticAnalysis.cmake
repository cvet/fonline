include(PVS-Studio)
include(GetFlags)
include(c4GetTargetPropertyRecursive)


function(c4_setup_static_analysis pfx umbrella_option)
    set(prefix ${pfx})
    if(prefix)
        set(prefix "${prefix}_")
    endif()
    if(WIN32)
        message(STATUS "${pfx}: no static analyzer available in WIN32")
        return()
    endif()
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
        return()
    endif()
    # option to turn lints on/off
    cmake_dependent_option(${prefix}LINT "add static analyzer targets" ON ${umbrella_option} OFF)
    cmake_dependent_option(${prefix}LINT_TESTS "add tests to run static analyzer targets" ON ${umbrella_option} OFF)
    # options for individual lints - contingent on linting on/off
    cmake_dependent_option(${prefix}LINT_CLANG_TIDY "use the clang-tidy static analyzer" ON "${prefix}LINT" OFF)
    cmake_dependent_option(${prefix}LINT_PVS_STUDIO "use the PVS-Studio static analyzer https://www.viva64.com/en/b/0457/" OFF "${prefix}LINT" OFF)
    if(${prefix}LINT_PVS_STUDIO)
        set(${prefix}LINT_PVS_STUDIO_FORMAT "errorfile" CACHE STRING "PVS-Studio output format. Choices: xml,csv,errorfile(like gcc/clang),tasklist(qtcreator)")
    endif()
    #
    set(sa)
    if(${prefix}LINT_CLANG_TIDY)
        set(sa "clang_tidy")
    endif()
    if(${prefix}LINT_PVS_STUDIO)
        set(sa "${sa} PVS-Studio")
    endif()
    if(sa)
        message(STATUS "${pfx}: enabled static analyzers: ${sa}")
    endif()
endfunction()


function(c4_static_analysis_target prefix target_name folder generated_targets)
    string(TOUPPER ${prefix} uprefix)
    if(uprefix)
        set(uprefix "${uprefix}_")
    endif()
    string(TOLOWER ${prefix} lprefix)
    if(lprefix)
        set(lprefix "${lprefix}-")
    endif()
    set(any_linter OFF)
    set(several_linters OFF)
    if(${uprefix}LINT_CLANG_TIDY OR ${uprefix}LINT_PVS_STUDIO)
        set(any_linter ON)
    endif()
    if(${uprefix}LINT_CLANG_TIDY AND ${uprefix}LINT_PVS_STUDIO)
        set(several_linters ON)
    endif()
    if(${uprefix}LINT AND any_linter)
        # umbrella target for running all linters for this particular target
        if(any_linter AND NOT TARGET ${lprefix}lint-all)
            add_custom_target(${lprefix}lint-all)
            if(folder)
                #message(STATUS "${target_name}: folder=${folder}")
                set_target_properties(${lprefix}lint-all PROPERTIES FOLDER "${folder}")
            endif()
        endif()
        if(${uprefix}LINT_CLANG_TIDY)
            c4_static_analysis_clang_tidy(${target_name}
                ${target_name}-lint-clang-tidy
                ${lprefix}lint-all-clang-tidy
                "${folder}")
            list(APPEND ${generated_targets} ${lprefix}lint-clang-tidy)
            add_dependencies(${lprefix}lint-all ${lprefix}lint-all-clang-tidy)
        endif()
        if(${uprefix}LINT_PVS_STUDIO)
            c4_static_analysis_pvs_studio(${target_name}
                ${target_name}-lint-pvs-studio
                ${lprefix}lint-all-pvs-studio
                "${folder}")
            list(APPEND ${generated_targets} ${lprefix}lint-pvs-studio)
            add_dependencies(${lprefix}lint-all ${lprefix}lint-all-pvs-studio)
        endif()
    endif()
endfunction()


function(c4_static_analysis_add_tests prefix target_name)
    string(TOUPPER ${prefix} uprefix)
    if(uprefix)
        set(uprefix "${uprefix}_")
    endif()
    if(${uprefix}LINT_CLANG_TIDY AND ${uprefix}LINT_TESTS)
        add_test(NAME ${target_name}-lint-clang-tidy-run
            COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target ${target_name}-lint-clang-tidy)
    endif()
    if(${uprefix}LINT_PVS_STUDIO AND ${uprefix}LINT_TESTS)
        add_test(NAME ${target_name}-lint-pvs-studio-run
            COMMAND
            ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target ${target_name}-lint-pvs-studio)
    endif()
endfunction()


#------------------------------------------------------------------------------
function(c4_static_analysis_clang_tidy subj_target lint_target umbrella_target folder)
    c4_static_analysis_clang_tidy_get_cmd(${subj_target} ${lint_target} cmd)
    add_custom_target(${lint_target}
        COMMAND ${cmd}
        COMMENT "clang-tidy: analyzing sources of ${subj_target}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    if(folder)
        set_target_properties(${lint_target} PROPERTIES FOLDER "${folder}")
    endif()
    if(NOT TARGET ${umbrella_target})
        add_custom_target(${umbrella_target})
    endif()
    add_dependencies(${umbrella_target} ${lint_target})
endfunction()

function(c4_static_analysis_clang_tidy_get_cmd subj_target lint_target cmd)
    get_target_property(_clt_srcs ${subj_target} SOURCES)
    get_target_property(_clt_opts ${subj_target} COMPILE_OPTIONS)
    c4_get_target_property_recursive(_clt_incs ${subj_target} INCLUDE_DIRECTORIES)
    list(REMOVE_DUPLICATES _clt_incs)
    get_include_flags(_clt_incs ${_clt_incs})
    if(NOT _clt_opts)
        set(_clt_opts)
    endif()
    separate_arguments(_clt_opts UNIX_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}} ${_clt_opts}")
    separate_arguments(_clt_incs UNIX_COMMAND "${_clt_incs}")
    set(${cmd} clang-tidy ${_clt_srcs} --config='' -- ${_clt_incs} ${_clt_opts} PARENT_SCOPE)
endfunction()


#------------------------------------------------------------------------------
function(c4_static_analysis_pvs_studio subj_target lint_target umbrella_target folder)
    c4_get_target_property_recursive(_c4al_pvs_incs ${subj_target} INCLUDE_DIRECTORIES)
    get_include_flags(_c4al_pvs_incs ${_c4al_pvs_incs})
    separate_arguments(_c4al_cxx_flags_sep UNIX_COMMAND "${CMAKE_CXX_FLAGS} ${_c4al_pvs_incs}")
    separate_arguments(_c4al_c_flags_sep UNIX_COMMAND "${CMAKE_C_FLAGS} ${_c4al_pvs_incs}")
    pvs_studio_add_target(TARGET ${lint_target}
        ALL # indicates that the analysis starts when you build the project
        #PREPROCESSOR ${_c4al_preproc}
        FORMAT tasklist
        LOG "${CMAKE_CURRENT_BINARY_DIR}/${subj_target}.pvs-analysis.tasks"
        ANALYZE ${name} #main_target subtarget:path/to/subtarget
        CXX_FLAGS ${_c4al_cxx_flags_sep}
        C_FLAGS ${_c4al_c_flags_sep}
        #CONFIG "/path/to/PVS-Studio.cfg"
        )
    if(folder)
        set_target_properties(${lint_target} PROPERTIES FOLDER "${folder}")
    endif()
    if(NOT TARGET ${umbrella_target})
        add_custom_target(${umbrella_target})
    endif()
    add_dependencies(${umbrella_target} ${lint_target})
endfunction()

function(c4_static_analysis_pvs_studio_get_cmd subj_target lint_target cmd)
    set(${cmd} $<RULE_LAUNCH_CUSTOM:${subj_target}> PARENT_SCOPE)
endfunction()
