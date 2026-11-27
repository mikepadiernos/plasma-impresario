# Checks that every source file which sends something out of the application is in the reviewed register.
#
# Usage: cmake -DSOURCE_DIR=<tree> -DREGISTER=<markdown file> -P CheckExternalExposure.cmake
#
# An outbound call is a network request or connection, a DNS lookup, a URL or file handed to another program, a
# started process, a D-Bus call, signal or exported object, or a desktop notification. Each .cpp file under
# src/ which makes one must be named in the "## Register" section of REGISTER, which says what it sends, where
# and after which user action; a file named there must still make one. Comment lines are ignored.
# See docs/integrations/external-services.md.

if(NOT SOURCE_DIR OR NOT REGISTER)
    message(FATAL_ERROR "SOURCE_DIR and REGISTER are required")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)

set(outbound "QNetworkRequest[ \t]+[A-Za-z_]+[ \t]*[(,;{]|QNetworkRequest[(]|new[ \t]+QDnsLookup|connectToHost|connectToServer[(]")
string(APPEND outbound "|QDesktopServices::openUrl|KIO::(get|storedGet|put|storedPut|copy|copyAs|file_copy|move|del)[(]")
string(APPEND outbound "|KIO::(OpenUrlJob|ApplicationLauncherJob|CommandLauncherJob|OpenFileManagerWindowJob)[(]")
string(APPEND outbound "|new[ \t]+QProcess|QProcess::(startDetached|execute)|QDBusMessage::create|QDBusInterface[ \t(&]")
string(APPEND outbound "|registerObject[(]|registerService[(]|new[ \t]+KNotification[ \t(]|KNotification::event")
string(APPEND outbound "|QWebEngineUrlRequestInterceptor|QNetworkAccessManager::(get|post|put|sendCustomRequest)[(]")

# The register: every `src/...cpp` path in its "## Register" section
file(READ "${REGISTER}" register_text)
string(FIND "${register_text}" "\n## Register\n" register_start)
if(register_start EQUAL -1)
    message(FATAL_ERROR "${REGISTER} has no \"## Register\" section")
endif()
string(SUBSTRING "${register_text}" ${register_start} -1 register_text)
string(SUBSTRING "${register_text}" 1 -1 register_rest)
string(FIND "${register_rest}" "\n## " register_end)
if(register_end GREATER -1)
    string(SUBSTRING "${register_rest}" 0 ${register_end} register_text)
endif()
string(REGEX MATCHALL "`src/[^`]+[.]cpp`" registered "${register_text}")
list(TRANSFORM registered REPLACE "`" "")
list(REMOVE_DUPLICATES registered)

file(GLOB_RECURSE sources RELATIVE "${SOURCE_DIR}" LIST_DIRECTORIES false "${SOURCE_DIR}/src/*.cpp")
list(FILTER sources EXCLUDE REGEX "^src/3rdparty/")
list(SORT sources)

set(problems "")
set(found "")
foreach(source ${sources})
    file(READ "${SOURCE_DIR}/${source}" text)
    list(FIND registered "${source}" listed)
    if(NOT text MATCHES "${outbound}")
        continue()
    endif()
    # Keep each line one list item: semicolons, brackets and backslashes would split or join them
    string(REPLACE "\\" "/" text "${text}")
    string(REPLACE ";" "," text "${text}")
    string(REPLACE "[" "(" text "${text}")
    string(REPLACE "]" ")" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(first "")
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        if(line MATCHES "^[ \t]*(//|/[*]|[*])")
            continue()
        endif()
        if(line MATCHES "${outbound}")
            string(STRIP "${line}" line)
            set(first "line ${number}: ${line}")
            break()
        endif()
    endforeach()
    if(first)
        list(APPEND found "${source}")
        if(listed EQUAL -1)
            list(APPEND problems "${source} sends something out of the application (${first}) but is not in the register")
        endif()
    endif()
endforeach()

foreach(source ${registered})
    list(FIND found "${source}" outbound_file)
    if(outbound_file EQUAL -1)
        list(APPEND problems "${source} is in the register but sends nothing out of the application")
    endif()
endforeach()

list(LENGTH sources checked)
if(checked EQUAL 0)
    message(FATAL_ERROR "No sources found under ${SOURCE_DIR}/src")
endif()

if(problems)
    list(LENGTH problems count)
    string(REPLACE ";" "\n  " listed "${problems}")
    message(FATAL_ERROR "${count} external exposure problem(s):\n  ${listed}\n"
        "Add what a new outbound call sends, where and after which user action to the register in "
        "docs/integrations/external-services.md, and keep message content to the account's own servers.")
endif()
list(LENGTH found outbound_count)
message(STATUS "External exposure register matches the sources: ${outbound_count} of ${checked} sources send something out")
