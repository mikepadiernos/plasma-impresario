file(READ ${SOURCE_DIR}/src/plasma-impresario-version IMPRESARIO_VERSION)
string(REPLACE "\n" "" IMPRESARIO_VERSION ${IMPRESARIO_VERSION})

if(GIT_EXECUTABLE AND EXISTS ${SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --dirty --long --always
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE IMPRESARIO_GIT_VERSION)
    string(REPLACE "\n" "" IMPRESARIO_GIT_VERSION ${IMPRESARIO_GIT_VERSION})
endif()

# Windows resources using four 16bit integers as version number, so prepare these numbers in VERNUMX variables from VERSION
macro(set_win_rc_vernum VERSION VERNUM1 VERNUM2 VERNUM3 VERNUM4)
    string(REGEX REPLACE "[^0-9-]" "_" VERNUM ${VERSION})
    if(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)_+([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} ${CMAKE_MATCH_3})
        set(${VERNUM4} ${CMAKE_MATCH_4})
    elseif(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} ${CMAKE_MATCH_3})
        set(${VERNUM4} "0")
    elseif(VERNUM MATCHES "^_*([0-9]+)_+([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} ${CMAKE_MATCH_2})
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    elseif(VERNUM MATCHES "^_*([0-9]+)")
        set(${VERNUM1} ${CMAKE_MATCH_1})
        set(${VERNUM2} "0")
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    else()
        set(${VERNUM1} "0")
        set(${VERNUM2} "0")
        set(${VERNUM3} "0")
        set(${VERNUM4} "0")
    endif()
endmacro()

set_win_rc_vernum(${IMPRESARIO_VERSION} IMPRESARIO_VERNUM1 IMPRESARIO_VERNUM2 IMPRESARIO_VERNUM3 IMPRESARIO_VERNUM4)
set(IMPRESARIO_VERSION_H
    "#define IMPRESARIO_VERSION \"${IMPRESARIO_VERSION}\"\n"
    "#define IMPRESARIO_VERNUM1 ${IMPRESARIO_VERNUM1}\n"
    "#define IMPRESARIO_VERNUM2 ${IMPRESARIO_VERNUM2}\n"
    "#define IMPRESARIO_VERNUM3 ${IMPRESARIO_VERNUM3}\n"
    "#define IMPRESARIO_VERNUM4 ${IMPRESARIO_VERNUM4}\n"
)

if(IMPRESARIO_GIT_VERSION)
    set_win_rc_vernum(${IMPRESARIO_GIT_VERSION} IMPRESARIO_GIT_VERNUM1 IMPRESARIO_GIT_VERNUM2 IMPRESARIO_GIT_VERNUM3 IMPRESARIO_GIT_VERNUM4)
    set(IMPRESARIO_GIT_VERSION_H
        "#define IMPRESARIO_GIT_VERSION \"${IMPRESARIO_GIT_VERSION}\"\n"
        "#define IMPRESARIO_GIT_VERNUM1 ${IMPRESARIO_GIT_VERNUM1}\n"
        "#define IMPRESARIO_GIT_VERNUM2 ${IMPRESARIO_GIT_VERNUM2}\n"
        "#define IMPRESARIO_GIT_VERNUM3 ${IMPRESARIO_GIT_VERNUM3}\n"
        "#define IMPRESARIO_GIT_VERNUM4 ${IMPRESARIO_GIT_VERNUM4}\n"
    )
else()
    set(IMPRESARIO_GIT_VERSION_H "")
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-version.h.in ${IMPRESARIO_VERSION_H})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-version.h)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-git-version.h.in ${IMPRESARIO_GIT_VERSION_H})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-git-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-git-version.h)

if(NSIS)
    if(IMPRESARIO_GIT_VERSION)
        string(REPLACE "GIT_" "" IMPRESARIO_VERSION_NSI ${IMPRESARIO_GIT_VERSION_H})
    else()
        set(IMPRESARIO_VERSION_NSI ${IMPRESARIO_VERSION_H})
    endif()
    string(REPLACE "#" "!" IMPRESARIO_VERSION_NSI ${IMPRESARIO_VERSION_NSI})
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi.in ${IMPRESARIO_VERSION_NSI})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi.in ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi)
endif()
