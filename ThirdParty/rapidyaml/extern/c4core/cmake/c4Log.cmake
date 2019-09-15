if(NOT _c4_log_included)
set(_c4_log_included ON)

macro(_c4_log)
    if(C4_PROJ_LOG_ENABLED)
        message(STATUS "c4: ${ARGN}")
    endif()
endmacro()

endif(NOT _c4_log_included)
