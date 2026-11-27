# Checks that the mail core stays free of KDE desktop services.
#
# Usage: cmake -DSOURCE_DIR=<tree> -DLINKS_FILE=<file> [-DDESKTOP_SOURCES=a.cpp|b.cpp] -P CheckArchitectureBoundaries.cmake
#
# The mail core is the IMAP, SMTP, composer, crypto, OAuth and plugin-interface code together with UiUtils. It may
# use Qt, the mail libraries and two KDE Frameworks that are plain libraries rather than desktop services:
# KItemModels and KFileMetaData. Everything that talks to the desktop (KConfig, KIO, KNotifications, KWindowSystem,
# Akonadi, KDE PIM, D-Bus) belongs to DesktopServices, IPC, DesktopGui or a plugin.
#
# LINKS_FILE has one line per core target: "<target>=<library>|<library>|...". DESKTOP_SOURCES lists the files,
# relative to SOURCE_DIR, which live in src/UiUtils but are built into DesktopServices; their headers are exempt too.

if(NOT SOURCE_DIR OR NOT LINKS_FILE)
    message(FATAL_ERROR "SOURCE_DIR and LINKS_FILE are required")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)

set(core_dirs AppVersion Common Composer Cryptography Imap MSA OAuth Streams UiUtils qwwsmtpclient)
set(integration_targets DesktopGui DesktopServices IPC WebEngineMessageRenderer)
set(allowed_frameworks KF6::ItemModels KF6::FileMetaData)
# Headers of desktop services; <KDescendantsProxyModel> (KItemModels) and <KFileMetaData/...> are allowed
set(forbidden_include "^[ \t]*#[ \t]*include[ \t]*[<\"](K(Config|SharedConfig|Desktop|Notification|StatusNotifier|WindowSystem|Wallet|Runner|IO|Service|Localized|AuthorizedCryptographer|Crash|DBusService|Job|Process|Calendar|Contacts|Identity|Mime|Pim|Sonnet|XmlGui|Action|StandardAction|Shortcut|Global|Icon|ColorScheme|Wayland|X11)[A-Za-z]*|kio/|KIO/|Akonadi|akonadi|KCalendarCore/|KContacts/|KPim|Sonnet/|QDBus|QtDBus|KF6/)")

set(problems "")

# Links of the core targets
file(STRINGS "${LINKS_FILE}" link_lines)
foreach(line ${link_lines})
    string(REGEX MATCH "^([^=]+)=(.*)$" _ "${line}")
    set(target "${CMAKE_MATCH_1}")
    string(REPLACE "|" ";" libraries "${CMAKE_MATCH_2}")
    foreach(library ${libraries})
        list(FIND allowed_frameworks "${library}" allowed)
        list(FIND integration_targets "${library}" integration)
        if(integration GREATER -1)
            list(APPEND problems "${target} links the integration target ${library}")
        elseif(allowed EQUAL -1 AND library MATCHES "^(KF[0-9]+::|KPim[0-9]*::|Qt[0-9]*::DBus$)")
            list(APPEND problems "${target} links the desktop service ${library}")
        endif()
    endforeach()
endforeach()

# Includes in the core sources
string(REPLACE "|" ";" desktop_sources "${DESKTOP_SOURCES}")
set(exempt "")
foreach(source ${desktop_sources})
    list(APPEND exempt "${source}")
    string(REGEX REPLACE "\\.cpp$" ".h" header "${source}")
    list(APPEND exempt "${header}")
endforeach()

set(patterns "")
foreach(dir ${core_dirs})
    list(APPEND patterns "${SOURCE_DIR}/src/${dir}/*.h" "${SOURCE_DIR}/src/${dir}/*.cpp")
endforeach()
# The plugin interfaces are core; the plugins in its subdirectories are where services may be used
list(APPEND patterns "${SOURCE_DIR}/src/Plugins/*.h" "${SOURCE_DIR}/src/Plugins/*.cpp")
file(GLOB_RECURSE sources RELATIVE "${SOURCE_DIR}" LIST_DIRECTORIES false ${patterns})
list(FILTER sources EXCLUDE REGEX "^src/Plugins/[^/]+/")

set(checked 0)
foreach(source ${sources})
    list(FIND exempt "${source}" is_exempt)
    if(is_exempt GREATER -1)
        continue()
    endif()
    math(EXPR checked "${checked} + 1")
    file(STRINGS "${SOURCE_DIR}/${source}" includes REGEX "^[ \t]*#[ \t]*include")
    foreach(include ${includes})
        if(include MATCHES "${forbidden_include}")
            string(STRIP "${include}" include)
            list(APPEND problems "${source} includes a desktop service: ${include}")
        endif()
    endforeach()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "No core sources found under ${SOURCE_DIR}")
endif()

if(problems)
    list(LENGTH problems count)
    string(REPLACE ";" "\n  " listed "${problems}")
    message(FATAL_ERROR "${count} architecture boundary problem(s):\n  ${listed}\n"
        "Move code that uses KDE desktop services to DesktopServices, IPC, DesktopGui or a plugin; "
        "see docs/integrations/kde-plasma.md#service-boundaries.")
endif()
message(STATUS "Mail core is free of desktop services: ${checked} sources checked")
