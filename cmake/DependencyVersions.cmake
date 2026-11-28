# Copyright (C) 2026 Plasma Impresario contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Writes the versions of the dependencies this configuration found, with the
# minimum the project requires where it declares one, to
# dependency-versions.txt in the build directory. The release gate,
# tools/dev-container/release-gate.sh, reads it.

function(impresario_write_dependency_versions)
    set(lines "# name\tfound\tminimum\n")
    set(lines "${lines}CMake\t${CMAKE_VERSION}\t3.16\n")
    set(lines "${lines}${CMAKE_CXX_COMPILER_ID}\t${CMAKE_CXX_COMPILER_VERSION}\t-\n")
    # Each entry is the package name, then the minimum or - when none is declared
    set(packages
        ECM ${KF_MIN_VERSION}
        Qt6 ${QT_REQUIRED_VERSION}
        Qt6WebEngineWidgets ${QT_REQUIRED_VERSION}
        KF6ItemModels ${KF_MIN_VERSION}
        KF6ColorScheme ${KF_MIN_VERSION}
        KF6Config ${KF_MIN_VERSION}
        KF6CoreAddons ${KF_MIN_VERSION}
        KF6I18n ${KF_MIN_VERSION}
        KF6KIO ${KF_MIN_VERSION}
        KF6WidgetsAddons ${KF_MIN_VERSION}
        KF6WindowSystem ${KF_MIN_VERSION}
        KF6DocTools ${KF_MIN_VERSION}
        KF6Sonnet ${SONNET_LIB_VERSION}
        KF6Notifications -
        KF6FileMetaData -
        KF6Service -
        KPim6AkonadiCalendar -
        KPim6AkonadiContactCore -
        Qt6Keychain -
        Gpgmepp 1.24.0
        QGpgmeQt6 1.24.0
    )
    while(packages)
        list(POP_FRONT packages name minimum)
        if(DEFINED ${name}_VERSION AND NOT "${${name}_VERSION}" STREQUAL "")
            set(lines "${lines}${name}\t${${name}_VERSION}\t${minimum}\n")
        endif()
    endwhile()
    file(WRITE "${CMAKE_BINARY_DIR}/dependency-versions.txt" "${lines}")
endfunction()
