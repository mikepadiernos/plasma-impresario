cmake_minimum_required(VERSION 3.16)

project(trojita)

set(QT_REQUIRED_VERSION "6.7.0")
set(KF_MIN_VERSION "6.26.0")

set(SONNET_LIB_VERSION "6.21.0")

find_package(ECM REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${ECM_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set a default build type if none was specified. This was shamelessly stolen
# from VTK's cmake setup because these guys produce both CMake and a project that
# manipulates this variable, and the web is full of posts where people say that
# it is apparently evil to just set the build type in a way an earlier version of
# this patch did. Oh, and the location of this check/update matters, apparently.
#
# Yes, this is just plain crazy.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# KDEInstallDirs and Qt's versionless targets depend on these being set first.
# This project only supports Qt 6.
set(QT_MAJOR_VERSION 6 CACHE STRING "Qt major version to use" FORCE)
set(QT_DEFAULT_MAJOR_VERSION 6)

include(KDEInstallDirs)
include(ImpresarioOption)

include(ECMDeprecationSettings)
include(ECMPoQmTools)

impresario_option(WITH_DESKTOP "Build desktop version" ON)
impresario_option(WITH_DBUS "Build with DBus library" AUTO)
impresario_option(WITH_RAGEL "Build with Ragel library" AUTO)
impresario_option(WITH_ZLIB "Build with zlib library" AUTO)
impresario_option(WITH_SHARED_PLUGINS "Enable shared dynamic plugins" ON)
impresario_option(BUILD_TESTING "Build tests" ON)
impresario_option(WITH_MIMETIC "Build with client-side MIME parsing" AUTO)
impresario_option(WITH_GPGMEPP "Use GpgME's native C++ bindings" AUTO)
option(WITH_WEBENGINE "Build with Qt WebEngine message rendering" ON)

if(WIN32)
    impresario_option(WITH_NSIS "Build Windows NSIS installer" AUTO "WITH_DESKTOP")
endif()

if(UNIX AND NOT APPLE)
    set(QTKEYCHAIN_DEPENDS ";WITH_DBUS")
else()
    set(QTKEYCHAIN_DEPENDS "")
endif()

find_package(Qt6 ${QT_REQUIRED_VERSION} REQUIRED
    COMPONENTS
        Core
        Core5Compat
        DBus
        Network
        Sql
        Svg
        Test
        Widgets
)

find_package(KF6 REQUIRED
    COMPONENTS
        ItemModels
)

if(WITH_DESKTOP)
    find_package(KF6 REQUIRED COMPONENTS ColorScheme Config CoreAddons I18n KIO WidgetsAddons WindowSystem)
    ki18n_install(po)
endif()

if(WITH_WEBENGINE)
    find_package(Qt6 ${QT_REQUIRED_VERSION} REQUIRED COMPONENTS WebEngineWidgets)
endif()

impresario_find_package(KPim6AkonadiContactCore "" "" "" "Required for building Akonadi address book plugin")
impresario_find_package(KPim6AkonadiContactWidgets "" "" "" "Required for opening and adding contacts in the Akonadi address book plugin")
impresario_plugin_option(WITH_AKONADIADDRESSBOOK_PLUGIN "Build AkonadiAddressbook plugin" KPim6AkonadiContactCore_FOUND KPim6AkonadiContactWidgets_FOUND)

impresario_plugin_option(WITH_ABOOKADDRESSBOOK_PLUGIN "Build AbookAddressbook plugin" STATIC)
impresario_plugin_option(WITH_CLEARTEXT_PLUGIN "Build Cleartext password plugin" STATIC)
impresario_plugin_option(WITH_QTKEYCHAIN_PLUGIN "Build Qtkeychain password plugin" "${QTKEYCHAIN_DEPENDS}")
impresario_plugin_option(WITH_SONNET_PLUGIN "Build a plugin for spellchecking via KDE's Sonnet" "WITH_DESKTOP")
impresario_option(WITH_KNOTIFICATIONS "Report background account errors as desktop notifications" AUTO "WITH_DESKTOP")
impresario_option(WITH_KFILEMETADATA "Index the text of downloaded attachments through KFileMetaData" AUTO)
impresario_option(WITH_AKONADI_CALENDAR "Show KDE PIM events and tasks in the My Day panel" AUTO "WITH_DESKTOP")
impresario_option(WITH_PLASMA_WIDGETS "Build the optional Plasma widgets" AUTO "WITH_DESKTOP;WITH_DBUS")

impresario_find_package(Git "" "" "" "")

find_package(KF6DocTools ${KF_MIN_VERSION})
set_package_properties(KF6DocTools PROPERTIES
    URL "https://invent.kde.org/frameworks/kdoctools/"
    DESCRIPTION "Tools to generate documentation from docbook files"
    TYPE OPTIONAL
)

impresario_find_package(Mimetic "" "http://www.codesink.org/mimetic_mime_library.html" "C++ MIME Library" "Required for client-side MIME parsing" WITH_MIMETIC)
impresario_find_package(Gpgmepp "1.24.0" "https://gnupg.org/related_software/gpgme/index.html" "C++ bindings for gpgme" "Needed for encrypted/signed e-mails" WITH_GPGMEPP)
impresario_find_package(QGpgmeQt6 "1.24.0" "https://gnupg.org/related_software/gpgme/index.html" "Qt 6 bindings for gpgme" "Needed for encrypted/signed e-mails" WITH_GPGMEPP)

impresario_option(WITH_CRYPTO_MESSAGES "Enable support for encrypted messages" AUTO "WITH_MIMETIC;WITH_GPGMEPP")

if(WIN32)
    impresario_find_package(MakeNSIS "" "http://nsis.sourceforge.net" "Nullsoft Scriptable Install System" "Needed for building Windows installer" WITH_NSIS)
endif()

# Add support for Mingw RC compiler
if(WIN32)
    enable_language(RC)
    include(CMakeDetermineRCCompiler)

    if(MINGW)
        set(CMAKE_RC_COMPILER_INIT windres)
        set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -I${CMAKE_CURRENT_BINARY_DIR} -i <SOURCE> -o <OBJECT>")
    endif()
endif()

impresario_find_package(Qt6Keychain QUIET "https://github.com/frankosterfeld/qtkeychain" "QtKeychain library (Qt6 version)" "Needed for QtKeychain password plugin" WITH_QTKEYCHAIN_PLUGIN)
impresario_find_package(KF6Sonnet ${SONNET_LIB_VERSION} "https://projects.kde.org/projects/frameworks/sonnet" "Qt6 Spell Checking Library from KDE" "Spell checking support" WITH_SONNET_PLUGIN)
impresario_find_package(KF6Notifications "" "https://invent.kde.org/frameworks/knotifications" "KDE desktop notifications" "Report background account errors as desktop notifications" WITH_KNOTIFICATIONS)
impresario_find_package(KF6FileMetaData "" "https://invent.kde.org/frameworks/kfilemetadata" "KDE file metadata and text extraction" "Index the text of downloaded attachments" WITH_KFILEMETADATA)
impresario_find_package(KPim6AkonadiCalendar "" "https://invent.kde.org/pim/akonadi-calendar" "KDE PIM calendars" "Events and tasks in My Day" WITH_AKONADI_CALENDAR)
impresario_find_package(Qt6Quick "" "https://doc.qt.io/qt-6/qtquick-index.html" "Qt Quick" "The Plasma widgets' shared views" WITH_PLASMA_WIDGETS)
impresario_find_package(Qt6QuickTest "" "https://doc.qt.io/qt-6/qtquicktest-index.html" "Qt Quick Test" "Tests for the Plasma widgets' views")
impresario_find_package(KF6Service "" "https://invent.kde.org/frameworks/kservice" "KDE service lookup" "Starting the application from the Plasma widgets" WITH_PLASMA_WIDGETS)
if(Qt6Keychain_FOUND OR QtKeychain_FOUND)
    message(STATUS "Found QtKeychain library (includes at ${QTKEYCHAIN_INCLUDE_DIRS}, lib at ${QTKEYCHAIN_LIBRARIES})")
else()
    message(STATUS "Could not find QtKeychain library")
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib${LIB_SUFFIX}")
endif()
mark_as_advanced(CMAKE_INSTALL_LIBDIR)

if(NOT CMAKE_INSTALL_PLUGIN_DIR)
    set(CMAKE_INSTALL_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/trojita")
endif()
mark_as_advanced(CMAKE_INSTALL_PLUGIN_DIR)

if(NOT PLUGIN_DIR)
    if(IS_ABSOLUTE ${CMAKE_INSTALL_PLUGIN_DIR})
        set(PLUGIN_DIR "${CMAKE_INSTALL_PLUGIN_DIR}")
    else()
        set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_PLUGIN_DIR}")
    endif()
endif()
mark_as_advanced(PLUGIN_DIR)

include(GNUInstallDirs)

# When manipulating CXXFLAGS, we put the user's CXXFLAGS *after* that so that they take priority.
if(MSVC)
    # See below for some reationale for these optimizations
    set(CMAKE_CXX_FLAGS "/O2 ${CMAKE_CXX_FLAGS}")

    # We have no information about the warnings and their usefullness. Reports are welcome.
    # We might enable warnings on MSVC in future.
else()
    # -Werror is not a default for sanity reasons (one cannot know what warnings a future compiler
    # might bring along), but it's a default in debug mode. The idea is that developers should care
    # about a warning-free build, and that this is easier than messing with yet another configure option.
    # TODO Reenable once the project is warning free
    # set(CMAKE_CXX_FLAGS_DEBUG "-Werror ${CMAKE_CXX_FLAGS_DEBUG}")

    # Optimizations are enabled unconditionally because they make a big difference in the speed of the
    # resulting binaries, and that it is better to allow an opt-out from them by adjusting CXXFLAGS through
    # an env var at cmake time if needed.
    # The reason for not manipulating just CMAKE_CXX_FLAGS_DEBUG is that unrecognized build types ("DebugFull")
    # should still benefit from these optimizations. Yup, it would be even better if CMake did a sane thing
    # and warned when users set an unrecognized and unused build type, but that just isn't the case.
    set(CMAKE_CXX_FLAGS "-O2 ${CMAKE_CXX_FLAGS}")

    # Build warnings are useful tools (and Trojita should be warning-free anyway), enable them on all
    # configurations. They are warnings, not errors.
    set(CMAKE_CXX_FLAGS "-Wall -Wsign-compare ${CMAKE_CXX_FLAGS}")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
# The following is required so that the moc_*.cpp and ui_*.h are found
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DQT_USE_QSTRINGBUILDER)
add_definitions(-DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DQT_USE_FAST_CONCATENATION)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)
ecm_set_disabled_deprecation_versions(Qt ${QT_REQUIRED_VERSION})

if(NOT MSVC)
  # We're using C++11's threading features (std::async in particular), and that requires "some threading". With GCC and
  # Clang, this is implemented through the -pthread build flag. Without using these bits, linking fails on Fedora 23,
  # and this is apparently a slightly different failure than the Kf5::Gpgmepp-pthread one in commit
  # 12e41101070f7073caec653185c0504763672ee7.
  #
  # Apparently, there's been various methods on how to enable this in the most-cmakeish-way throughout the years, with
  # cmake-3.1+ supporting some magic linking via the Threads::Threads option on a per-library basis. However, I am not
  # really looking into that wonderful fun of mixing -pthread and non-pthread translation units, so let's use a big
  # hammer and set it unconditionally on platforms which use it anyway.
  #
  # And because we also support MinGW and its `windres` compiler, we have to avoid passing -pthread to *that* thing, so,
  # well, let's cheat and put it into the CXXFLAGS.
  set(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")
endif()

# Make sure that plugins not export all symbols, only that which are explicitly marked
include(GenerateExportHeader)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_AUTOMOC True)
set(CMAKE_AUTORCC True)
set(CMAKE_AUTOUIC True)

impresario_find_package(RagelForImpresario "" "" "" "" WITH_RAGEL)
impresario_find_package(ZLIB "" "" "" "" WITH_ZLIB)

if(WITH_MIMETIC)
  set(IMPRESARIO_HAVE_MIMETIC True)
else()
  set(IMPRESARIO_HAVE_MIMETIC False)
endif()

if(WITH_GPGMEPP)
  set(IMPRESARIO_HAVE_GPGMEPP True)
else()
  set(IMPRESARIO_HAVE_GPGMEPP False)
endif()

if(WITH_CRYPTO_MESSAGES)
  set(IMPRESARIO_HAVE_CRYPTO_MESSAGES True)
else()
  set(IMPRESARIO_HAVE_CRYPTO_MESSAGES False)
endif()

if(WITH_ZLIB)
    set(IMPRESARIO_HAVE_ZLIB True)
    message(STATUS "Support for COMPRESS=DEFLATE enabled")
else()
    set(IMPRESARIO_HAVE_ZLIB False)
    message(STATUS "Disabling COMPRESS=DEFLATE, zlib is not available")
endif()

if(WITH_KNOTIFICATIONS)
    set(IMPRESARIO_HAVE_KNOTIFICATIONS True)
else()
    set(IMPRESARIO_HAVE_KNOTIFICATIONS False)
endif()

if(WITH_KFILEMETADATA)
    set(IMPRESARIO_HAVE_KFILEMETADATA True)
else()
    set(IMPRESARIO_HAVE_KFILEMETADATA False)
endif()

if(WITH_AKONADI_CALENDAR)
    set(IMPRESARIO_HAVE_AKONADI_CALENDAR True)
else()
    set(IMPRESARIO_HAVE_AKONADI_CALENDAR False)
endif()

if (WITH_WEBENGINE)
    set(IMPRESARIO_HAVE_WEBENGINE True)
    message(STATUS "Support for viewing mails via Qt WebEngine is enabled")
else()
    set(IMPRESARIO_HAVE_WEBENGINE False)
    message(STATUS "Building without message viewing; Qt WebEngine is disabled")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/configure.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/configure.cmake.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/configure-plugins.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/configure-plugins.cmake.h)

feature_summary(FATAL_ON_MISSING_REQUIRED_PACKAGES INCLUDE_QUIET_PACKAGES DESCRIPTION "\n" WHAT ALL)

set(path_Common ${CMAKE_CURRENT_SOURCE_DIR}/src/Common)
set(libCommon_SOURCES
    ${path_Common}/AccountRegistry.cpp
    ${path_Common}/AccountStatus.cpp
    ${path_Common}/TrustedCertificates.cpp
    ${path_Common}/Application.cpp
    ${path_Common}/ConnectionId.cpp
    ${path_Common}/FileLogger.cpp
    ${path_Common}/MetaTypes.cpp
    ${path_Common}/Paths.cpp
    ${path_Common}/PrivateFiles.cpp
    ${path_Common}/SettingsNames.cpp
    ${path_Common}/StashingReverseIterator.h
)

set(path_Plugins ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins)
set(libPlugins_SOURCES
    ${path_Plugins}/AddressbookPlugin.cpp
    ${path_Plugins}/PasswordPlugin.cpp
    ${path_Plugins}/PluginJob.cpp
    ${path_Plugins}/PluginManager.cpp
    ${path_Plugins}/SpellcheckerPlugin.cpp
)

set(path_OAuth ${CMAKE_CURRENT_SOURCE_DIR}/src/OAuth)
set(libOAuth_SOURCES
    ${path_OAuth}/OAuth.cpp
    ${path_OAuth}/Session.cpp
    ${path_OAuth}/TokenSource.cpp
    ${path_OAuth}/TokenStore.cpp
)

set(path_UiUtils ${CMAKE_CURRENT_SOURCE_DIR}/src/UiUtils)
set(libUiUtils_SOURCES
    ${path_UiUtils}/AccountNotices.cpp
    ${path_UiUtils}/ActivationToken.cpp
    ${path_UiUtils}/ActivityLog.cpp
    ${path_UiUtils}/Agenda.cpp
    ${path_UiUtils}/AttachmentCollector.cpp
    ${path_UiUtils}/ClamdScanner.cpp
    ${path_UiUtils}/Color.cpp
    ${path_UiUtils}/Conversations.cpp
    ${path_UiUtils}/FolderColors.cpp
    ${path_UiUtils}/Formatting.cpp
    ${path_UiUtils}/IconLoader.cpp
    ${path_UiUtils}/Keymap.cpp
    ${path_UiUtils}/LinkSafety.cpp
    ${path_UiUtils}/MailRules.cpp
    ${path_UiUtils}/MailSafety.cpp
    ${path_UiUtils}/MailTabs.cpp
    ${path_UiUtils}/MailToProductivity.cpp
    ${path_UiUtils}/MailingLists.cpp
    ${path_UiUtils}/MessageId.cpp
    ${path_UiUtils}/MessageIdLookup.cpp
    ${path_UiUtils}/MessageSecurity.cpp
    ${path_UiUtils}/NewMail.cpp
    ${path_UiUtils}/NewMailWatcher.cpp
    ${path_UiUtils}/OutputSurfaces.cpp
    ${path_UiUtils}/PasswordWatcher.cpp
    ${path_UiUtils}/PlainTextFormatter.cpp
    ${path_UiUtils}/QaimDfsIterator.cpp
    ${path_UiUtils}/QuickFilters.cpp
    ${path_UiUtils}/RecentFolders.cpp
    ${path_UiUtils}/RelatedMessages.cpp
    ${path_UiUtils}/RelatedSearch.cpp
    ${path_UiUtils}/Relevance.cpp
    ${path_UiUtils}/SearchBuilder.cpp
    ${path_UiUtils}/SearchQuery.cpp
    ${path_UiUtils}/SearchFacets.cpp
    ${path_UiUtils}/SearchScope.cpp
    ${path_UiUtils}/SearchSuggestions.cpp
    ${path_UiUtils}/SelectionAfterRemoval.cpp
    ${path_UiUtils}/SettingsSchema.cpp
    ${path_UiUtils}/ServerDiscovery.cpp
    ${path_UiUtils}/ServerDiscoveryNetwork.cpp
    ${path_UiUtils}/ShortcutOverview.cpp
    ${path_UiUtils}/StyleSheets.cpp
    ${path_UiUtils}/Theme.cpp
    ${path_UiUtils}/ThemeProfiles.cpp
    ${path_UiUtils}/UnreadFolders.cpp
    ${path_UiUtils}/ViewStateStore.cpp
    ${path_UiUtils}/Workspaces.cpp
)

if(WITH_WEBENGINE)
    set(libUiUtils_SOURCES
        ${libUiUtils_SOURCES}
        ${path_UiUtils}/PartLoadingOptions.h
        ${path_UiUtils}/PartVisitor.h
        ${path_UiUtils}/PartWalker.h
        ${path_UiUtils}/PartWalker_impl.h
    )
endif()

# Helpers backed by KDE services (KConfig, Akonadi) form their own library, so that the mail core and UiUtils
# stay free of desktop services; test_ArchitectureBoundaries keeps it that way.
set(libDesktopServices_SOURCES
    ${path_UiUtils}/BackgroundRunning.cpp
    ${path_UiUtils}/DesktopFeedback.cpp
    ${path_UiUtils}/DestructiveActions.cpp
    ${path_UiUtils}/FileSearch.cpp
    ${path_UiUtils}/MailRulesSettings.cpp
    ${path_UiUtils}/MailSafetySettings.cpp
    ${path_UiUtils}/MailStatus.cpp
    ${path_UiUtils}/NewMailSettings.cpp
    ${path_UiUtils}/PlasmaSearch.cpp
    ${path_UiUtils}/SavedSearches.cpp
    ${path_UiUtils}/SettingsStore.cpp
    ${path_UiUtils}/SleepMonitor.cpp
)

if(WITH_AKONADI_CALENDAR)
    set(libDesktopServices_SOURCES
        ${libDesktopServices_SOURCES}
        ${path_UiUtils}/AkonadiAgendaSource.cpp
    )
endif()

set(path_IPC ${CMAKE_CURRENT_SOURCE_DIR}/src/IPC)

if(WITH_DBUS)
    set(libIPC_SOURCES
        ${path_IPC}/DBusInterface.cpp
        ${path_IPC}/KRunnerBridge.cpp
        ${path_IPC}/MailStatusBridge.cpp
        ${path_IPC}/MainWindowBridge.cpp
    )
else()
    set(libIPC_SOURCES
        ${path_IPC}/None.cpp
    )
endif()

set(path_Composer ${CMAKE_CURRENT_SOURCE_DIR}/src/Composer)
set(libComposer_SOURCES
    ${path_Composer}/AbstractComposer.cpp
    ${path_Composer}/AccountIdentities.cpp
    ${path_Composer}/AttachmentRequests.cpp
    ${path_Composer}/ComposerAttachments.cpp
    ${path_Composer}/ExistingMessageComposer.cpp
    ${path_Composer}/HeaderSafety.cpp
    ${path_Composer}/Mailto.cpp
    ${path_Composer}/MessageComposer.cpp
    ${path_Composer}/QuoteText.cpp
    ${path_Composer}/Recipients.cpp
    ${path_Composer}/ReplaceSignature.cpp
    ${path_Composer}/SenderIdentitiesModel.cpp
    ${path_Composer}/SubjectMangling.cpp
    ${path_Composer}/Submission.cpp
)

set(path_MSA ${CMAKE_CURRENT_SOURCE_DIR}/src/MSA)
set(libMSA_SOURCES
    ${path_MSA}/AbstractMSA.cpp
    ${path_MSA}/Account.cpp
    ${path_MSA}/FakeMSA.cpp
    ${path_MSA}/ImapSubmit.cpp
    ${path_MSA}/SMTP.cpp
    ${path_MSA}/Sendmail.cpp
)

set(path_Streams ${CMAKE_CURRENT_SOURCE_DIR}/src/Streams)
set(libStreams_SOURCES
    ${path_Streams}/DeletionWatcher.cpp
    ${path_Streams}/FakeSocket.cpp
    ${path_Streams}/IODeviceSocket.cpp
    ${path_Streams}/Socket.cpp
    ${path_Streams}/SocketFactory.cpp
)

set(path_LocalMail ${CMAKE_CURRENT_SOURCE_DIR}/src/LocalMail)
set(libLocalMail_SOURCES
    ${path_LocalMail}/ImapSession.cpp
    ${path_LocalMail}/LocalSocket.cpp
    ${path_LocalMail}/Mime.cpp
    ${path_LocalMail}/Store.cpp
)

set(path_Pop3 ${CMAKE_CURRENT_SOURCE_DIR}/src/Pop3)
set(libPop3_SOURCES
    ${path_Pop3}/FetchJob.cpp
)

set(path_Jmap ${CMAKE_CURRENT_SOURCE_DIR}/src/Jmap)
set(libJmap_SOURCES
    ${path_Jmap}/Client.cpp
    ${path_Jmap}/Settings.cpp
    ${path_Jmap}/Submission.cpp
    ${path_Jmap}/SyncJob.cpp
)

set(path_Cryptography ${CMAKE_CURRENT_SOURCE_DIR}/src/Cryptography)
set(libCryptography_SOURCES
    ${path_Cryptography}/MessageModel.cpp
    ${path_Cryptography}/MessagePart.cpp
    ${path_Cryptography}/PartReplacer.cpp
)
if(WITH_MIMETIC)
    set(libCryptography_SOURCES
        ${libCryptography_SOURCES}
        ${path_Cryptography}/LocalMimeParser.cpp
        ${path_Cryptography}/MimeticUtils.cpp
    )
endif()
if(WITH_CRYPTO_MESSAGES)
    set(libCryptography_SOURCES
        ${libCryptography_SOURCES}
        ${path_Cryptography}/GpgMe++.cpp
    )
endif()

if(WITH_ZLIB)
    set(libStreams_SOURCES ${libStreams_SOURCES}
        ${path_Streams}/3rdparty/rfc1951.cpp)
endif()

set(path_DesktopGui ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui)
set(libDesktopGui_SOURCES
    ${path_DesktopGui}/AccountSidebar.cpp
    ${path_DesktopGui}/AddAccountDialog.cpp
    ${path_DesktopGui}/ActivityDialog.cpp
    ${path_DesktopGui}/AgendaPanel.cpp
    ${path_DesktopGui}/AttachmentPanel.cpp
    ${path_DesktopGui}/CapabilityActionGate.cpp
    ${path_DesktopGui}/ContactPanel.cpp
    ${path_DesktopGui}/ColoredItemDelegate.cpp
    ${path_DesktopGui}/ComposeWidget.cpp
    ${path_DesktopGui}/ComposerAttachmentsList.cpp
    ${path_DesktopGui}/ComposerTextEdit.cpp
    ${path_DesktopGui}/DensityDelegate.cpp
    ${path_DesktopGui}/DensityMenu.cpp
    ${path_DesktopGui}/EmbeddedPanel.cpp
    ${path_DesktopGui}/EmbeddedPanelsMenu.cpp
    ${path_DesktopGui}/ExternalElementsWidget.cpp
    ${path_DesktopGui}/FileChooser.cpp
    ${path_DesktopGui}/FlowLayout.cpp
    ${path_DesktopGui}/FromAddressProxyModel.cpp
    ${path_DesktopGui}/InlineNotificationBar.cpp
    ${path_DesktopGui}/LineEdit.cpp
    ${path_DesktopGui}/LinkTarget.cpp
    ${path_DesktopGui}/MailSafetyPrompt.cpp
    ${path_DesktopGui}/MailAutomation.cpp
    ${path_DesktopGui}/MailBoxTreeView.cpp
    ${path_DesktopGui}/MailboxPickerDialog.cpp
    ${path_DesktopGui}/RemoteAttachmentFetcher.cpp
    ${path_DesktopGui}/RuleEditDialog.cpp
    ${path_DesktopGui}/RulesDialog.cpp
    ${path_DesktopGui}/SearchBuilderDialog.cpp
    ${path_DesktopGui}/SavedSearchFolders.cpp
    ${path_DesktopGui}/SearchFoldersDialog.cpp
    ${path_DesktopGui}/SavedFilesSearchDialog.cpp
    ${path_DesktopGui}/StartAfterLogin.cpp
    ${path_DesktopGui}/SearchIndexDialog.cpp
    ${path_DesktopGui}/SearchResultsPanel.cpp
    ${path_DesktopGui}/MessageActionHandler.cpp
    ${path_DesktopGui}/MessageDetailsPanel.cpp
    ${path_DesktopGui}/MessageListWidget.cpp
    ${path_DesktopGui}/MessageRowsMenu.cpp
    ${path_DesktopGui}/MessageSelectionRestorer.cpp
    ${path_DesktopGui}/MsgItemDelegate.cpp
    ${path_DesktopGui}/MsgListView.cpp
    ${path_DesktopGui}/NotificationsDialog.cpp
    ${path_DesktopGui}/OnePanelAtTimeWidget.cpp
    ${path_DesktopGui}/OverlayWidget.cpp
    ${path_DesktopGui}/PasswordDialog.cpp
    ${path_DesktopGui}/JmapSyncer.cpp
    ${path_DesktopGui}/Pop3Fetcher.cpp
    ${path_DesktopGui}/ProgressPopUp.cpp
    ${path_DesktopGui}/ProtocolLoggerWidget.cpp
    ${path_DesktopGui}/ReaderActionBar.cpp
    ${path_DesktopGui}/RelatedMessagesPanel.cpp
    ${path_DesktopGui}/ReplaceCharValidator.cpp
    ${path_DesktopGui}/SettingsDialog.cpp
    ${path_DesktopGui}/SettingsGroups.cpp
    ${path_DesktopGui}/AppearanceManager.cpp
    ${path_DesktopGui}/AppearanceStyle.cpp
    ${path_DesktopGui}/ThemeGroup.cpp
    ${path_DesktopGui}/PlasmaActivities.cpp
    ${path_DesktopGui}/MailTabBar.cpp
    ${path_DesktopGui}/VimMode.cpp
    ${path_DesktopGui}/SettingsOAuthControls.cpp
    ${path_DesktopGui}/ShellHeader.cpp
    ${path_DesktopGui}/ShortcutOverviewDialog.cpp
    ${path_DesktopGui}/Spinner.cpp
    ${path_DesktopGui}/StateColors.cpp
    ${path_DesktopGui}/TagAddDialog.cpp
    ${path_DesktopGui}/TagHandler.cpp
    ${path_DesktopGui}/TagListWidget.cpp
    ${path_DesktopGui}/TagWidget.cpp
    ${path_DesktopGui}/TaskProgressIndicator.cpp
    ${path_DesktopGui}/Util.cpp
    ${path_DesktopGui}/Window.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigDialog.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutConfigWidget.cpp
    ${path_DesktopGui}/ShortcutHandler/ShortcutHandler.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/icons.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/license.qrc
)

if(WITH_WEBENGINE)
    set(libDesktopGui_SOURCES
        ${libDesktopGui_SOURCES}
        ${path_DesktopGui}/AddressRowWidget.cpp
        ${path_DesktopGui}/AttachmentView.cpp
        ${path_DesktopGui}/CompleteMessageWidget.cpp
        ${path_DesktopGui}/EmbeddedWebView.cpp
        ${path_DesktopGui}/EnvelopeView.cpp
        ${path_DesktopGui}/FindBar.cpp
        ${path_DesktopGui}/FindBarMixin.cpp
        ${path_DesktopGui}/LoadablePartWidget.cpp
        ${path_DesktopGui}/MessageHeadersWidget.cpp
        ${path_DesktopGui}/MessageSourceWidget.cpp
        ${path_DesktopGui}/MessageView.cpp
        ${path_DesktopGui}/OneEnvelopeAddress.cpp
        ${path_DesktopGui}/PartWalker.cpp
        ${path_DesktopGui}/PartWidget.cpp
        ${path_DesktopGui}/PartWidgetFactoryVisitor.cpp
        ${path_DesktopGui}/SimplePartWidget.cpp
)
endif()

set(libqwwsmtpclient_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/qwwsmtpclient/qwwsmtpclient.cpp)

set(libAppVersion_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/AppVersion/SetCoreApplication.cpp)

set(path_Imap ${CMAKE_CURRENT_SOURCE_DIR}/src/Imap)
set(libImap_SOURCES
    ${path_Imap}/ConnectionState.cpp
    ${path_Imap}/Encoders.cpp
    ${path_Imap}/Exceptions.cpp
    ${path_Imap}/Parser/3rdparty/kcodecs.cpp
    ${path_Imap}/Parser/3rdparty/rfccodecs.cpp

    ${path_Imap}/Parser/Command.cpp
    ${path_Imap}/Parser/Data.cpp
    ${path_Imap}/Parser/ListHeaders.cpp
    ${path_Imap}/Parser/LowLevelParser.cpp
    ${path_Imap}/Parser/MailAddress.cpp
    ${path_Imap}/Parser/Message.cpp
    ${path_Imap}/Parser/Parser.cpp
    ${path_Imap}/Parser/Response.cpp
    ${path_Imap}/Parser/Sequence.cpp
    ${path_Imap}/Parser/ThreadingNode.cpp

    ${path_Imap}/Network/FileDownloadManager.cpp
    ${path_Imap}/Network/ForbiddenReply.cpp
    ${path_Imap}/Network/MsgPartNetAccessManager.cpp
    ${path_Imap}/Network/MsgPartNetworkReply.cpp
    ${path_Imap}/Network/QQuickNetworkReplyWrapper.cpp

    ${path_Imap}/Model/Cache.cpp
    ${path_Imap}/Model/CombinedCache.cpp
    ${path_Imap}/Model/DragAndDrop.cpp
    ${path_Imap}/Model/DiskPartCache.cpp
    ${path_Imap}/Model/DummyNetworkWatcher.cpp
    ${path_Imap}/Model/FindInterestingPart.cpp
    ${path_Imap}/Model/FlagsOperation.cpp
    ${path_Imap}/Model/FullMessageCombiner.cpp
    ${path_Imap}/Model/AccountCapabilities.cpp
    ${path_Imap}/Model/ImapAccess.cpp
    ${path_Imap}/Model/MailboxFinder.cpp
    ${path_Imap}/Model/MailboxMetadata.cpp
    ${path_Imap}/Model/MailboxModel.cpp
    ${path_Imap}/Model/MailboxTree.cpp
    ${path_Imap}/Model/MemoryCache.cpp
    ${path_Imap}/Model/Model.cpp
    ${path_Imap}/Model/MsgListModel.cpp
    ${path_Imap}/Model/NetworkWatcher.cpp
    ${path_Imap}/Model/OneMessageModel.cpp
    ${path_Imap}/Model/FavoriteTagsModel.cpp
    ${path_Imap}/Model/GroupingMsgListModel.cpp
    ${path_Imap}/Model/ParserState.cpp
    ${path_Imap}/Model/PrettyMailboxModel.cpp
    ${path_Imap}/Model/SpecialFolder.cpp
    ${path_Imap}/Model/PrettyMsgListModel.cpp
    ${path_Imap}/Model/SpecialFlagNames.cpp
    ${path_Imap}/Model/SQLCache.cpp
    ${path_Imap}/Model/SubtreeModel.cpp
    ${path_Imap}/Model/SystemNetworkWatcher.cpp
    ${path_Imap}/Model/TaskFactory.cpp
    ${path_Imap}/Model/TaskPresentationModel.cpp
    ${path_Imap}/Model/ThreadingMsgListModel.cpp
    ${path_Imap}/Model/UnifiedMsgListModel.cpp
    ${path_Imap}/Model/SearchResultsModel.cpp
    ${path_Imap}/Model/SearchSnippets.cpp
    ${path_Imap}/Model/FolderSearch.cpp
    ${path_Imap}/Model/AttachmentTextExtractor.cpp
    ${path_Imap}/Model/DecryptedTextIndexer.cpp
    ${path_Imap}/Model/SearchIndex.cpp
    ${path_Imap}/Model/SearchIndexer.cpp
    ${path_Imap}/Model/SearchTextFetcher.cpp
    ${path_Imap}/Model/SearchSupport.cpp
    ${path_Imap}/Model/Utils.cpp
    ${path_Imap}/Model/VisibleTasksModel.cpp

    # The ModelWatcher is another debugging aid
    ${path_Imap}/Model/ModelWatcher.cpp

    ${path_Imap}/Tasks/AppendTask.cpp
    ${path_Imap}/Tasks/CopyMoveMessagesTask.cpp
    ${path_Imap}/Tasks/CreateMailboxTask.cpp
    ${path_Imap}/Tasks/DeleteMailboxTask.cpp
    ${path_Imap}/Tasks/EnableTask.cpp
    ${path_Imap}/Tasks/ExpungeMailboxTask.cpp
    ${path_Imap}/Tasks/ExpungeMessagesTask.cpp
    ${path_Imap}/Tasks/Fake_ListChildMailboxesTask.cpp
    ${path_Imap}/Tasks/Fake_OpenConnectionTask.cpp
    ${path_Imap}/Tasks/FetchGmailLabelsTask.cpp
    ${path_Imap}/Tasks/FetchMsgMetadataTask.cpp
    ${path_Imap}/Tasks/FetchMsgPartTask.cpp
    ${path_Imap}/Tasks/GenUrlAuthTask.cpp
    ${path_Imap}/Tasks/GetAnyConnectionTask.cpp
    ${path_Imap}/Tasks/IdTask.cpp
    ${path_Imap}/Tasks/IdleLauncher.cpp
    ${path_Imap}/Tasks/ImapTask.cpp
    ${path_Imap}/Tasks/KeepMailboxOpenTask.cpp
    ${path_Imap}/Tasks/ListChildMailboxesTask.cpp
    ${path_Imap}/Tasks/NoopTask.cpp
    ${path_Imap}/Tasks/NumberOfMessagesTask.cpp
    ${path_Imap}/Tasks/ObtainSynchronizedMailboxTask.cpp
    ${path_Imap}/Tasks/OfflineConnectionTask.cpp
    ${path_Imap}/Tasks/OpenConnectionTask.cpp
    ${path_Imap}/Tasks/RenameMailboxTask.cpp
    ${path_Imap}/Tasks/SortTask.cpp
    ${path_Imap}/Tasks/SubscribeUnsubscribeTask.cpp
    ${path_Imap}/Tasks/ThreadTask.cpp
    ${path_Imap}/Tasks/UidSubmitTask.cpp
    ${path_Imap}/Tasks/UnSelectTask.cpp
    ${path_Imap}/Tasks/UpdateFlagsTask.cpp
    ${path_Imap}/Tasks/UpdateFlagsOfAllMessagesTask.cpp
)

# The ModelTest is only needed when debugging manually
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND libImap_SOURCES ${path_Imap}/Model/ModelTest/modeltest.cpp)
endif()

if(WITH_RAGEL)
    message(STATUS "Using Ragel for the RFC 5322 parser")
    ragel_parser(${path_Imap}/Parser/Rfc5322HeaderParser.cpp)
    set(libImap_SOURCES ${libImap_SOURCES}
        ${CMAKE_CURRENT_BINARY_DIR}/Rfc5322HeaderParser.generated.cpp)
else()
    message(STATUS "Using pregenerated RFC 5322 parser")
    set(libImap_SOURCES ${libImap_SOURCES}
        ${path_Imap}/Parser/Rfc5322HeaderParser.generated.cpp)
endif()

set(impresario_desktop_SOURCES
    ${path_DesktopGui}/main.cpp
)

if(WIN32)
    list(APPEND impresario_desktop_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/trojita_win32.rc)
    set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/trojita_win32.rc APPEND PROPERTY OBJECT_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/plasma-impresario.ico
        ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-git-version.h
        ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-version.h
    )
endif()

ecm_install_po_files_as_qm(poqm)

set(version_files ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-version.h ${CMAKE_CURRENT_BINARY_DIR}/plasma-impresario-git-version.h)
if(WITH_NSIS)
    set(version_files ${version_files} ${CMAKE_CURRENT_BINARY_DIR}/trojita-version.nsi)
    set(NSIS TRUE)
endif()

add_custom_target(version DEPENDS version_fake_file)
add_custom_command(OUTPUT version_fake_file ${version_files}
    COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DNSIS=${NSIS} -DHOST_ARCH=${HOST_ARCH} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ImpresarioVersion.cmake)
set_source_files_properties(${version_files}
    PROPERTIES GENERATED TRUE
    HEADER_FILE_ONLY TRUE)

add_library(Common STATIC ${libCommon_SOURCES})
set_property(TARGET Common APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
add_dependencies(Common version)
target_link_libraries(Common
    PUBLIC
        Qt::Core
        Qt::Network
)

add_library(AppVersion STATIC ${libAppVersion_SOURCES})
set_property(TARGET AppVersion APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
add_dependencies(AppVersion version)
target_link_libraries(AppVersion
    PRIVATE
        Qt::Core
        Common
)

if(WITH_SHARED_PLUGINS)
    add_library(Plugins SHARED ${libPlugins_SOURCES})
    set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS BUILDING_LIBIMPRESARIO_PLUGINS)
else()
    add_library(Plugins STATIC ${libPlugins_SOURCES})
    set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS QT_STATICPLUGIN)
endif()
set_target_properties(Plugins PROPERTIES OUTPUT_NAME trojita_plugins)
set_property(TARGET Plugins APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Plugins
    PUBLIC
        Qt::Core
    PRIVATE
        Common
)

# Optional build-time OAuth client IDs; accounts can override them. Never configure a client secret here.
set(IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID "" CACHE STRING "Default OAuth client ID for Google accounts")
set(IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID "" CACHE STRING "Default OAuth client ID for Microsoft accounts")
add_library(OAuth STATIC ${libOAuth_SOURCES})
set_property(TARGET OAuth APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_compile_definitions(OAuth PRIVATE
    IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID="${IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID}"
    IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID="${IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID}"
)
target_link_libraries(OAuth
    PUBLIC
        Qt::Core
        Qt::Network
        Common
        Plugins
    PRIVATE
        Qt::Gui
)

add_library(UiUtils STATIC ${libUiUtils_SOURCES})
set_property(TARGET UiUtils APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(UiUtils
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Network
        Imap
        Plugins
)

if(WITH_DESKTOP)
    add_library(DesktopServices STATIC ${libDesktopServices_SOURCES})
    set_property(TARGET DesktopServices APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(DesktopServices
        PUBLIC
            Qt::Core
            Qt::DBus
            Imap
            UiUtils
        PRIVATE
            KF6::ConfigCore
    )
    if(WITH_AKONADI_CALENDAR)
        target_link_libraries(DesktopServices PRIVATE KPim6::AkonadiCalendar KF6::CalendarCore KPim6::AkonadiCore)
    endif()
endif()

add_library(Streams STATIC ${libStreams_SOURCES})
set_property(TARGET Streams APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
if(WITH_ZLIB)
    target_link_libraries(Streams
        PUBLIC
            ZLIB::ZLIB
    )
endif()
target_link_libraries(Streams
    PUBLIC
        Qt::Core
        Qt::Network
        Imap
)

add_library(LocalMail STATIC ${libLocalMail_SOURCES})
set_property(TARGET LocalMail APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(LocalMail
    PUBLIC
        Qt::Core
        Qt::Sql
        Streams
    PRIVATE
        Common
        Imap
)

add_library(Pop3 STATIC ${libPop3_SOURCES})
set_property(TARGET Pop3 APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Pop3
    PUBLIC
        Qt::Core
        Qt::Network
        LocalMail
    PRIVATE
        Common
)

add_library(IPC STATIC ${libIPC_SOURCES})
set_property(TARGET IPC APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
if(WITH_DBUS)
    target_link_libraries(IPC
        PUBLIC
            Qt::Core
        PRIVATE
            Qt::DBus
            DesktopGui
            DesktopServices
            KF6::ConfigCore
    )
else()
    target_link_libraries(IPC
        PRIVATE
            Qt::Core)
endif()

add_library(qwwsmtpclient STATIC ${libqwwsmtpclient_SOURCES})
target_link_libraries(qwwsmtpclient
    PUBLIC
        Qt::Core
        Qt::Network
)

add_library(Jmap STATIC ${libJmap_SOURCES})
set_property(TARGET Jmap APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Jmap
    PUBLIC
        Qt::Core
        Qt::Network
        LocalMail
        MSA
    PRIVATE
        Common
)

add_library(MSA STATIC ${libMSA_SOURCES})
set_property(TARGET MSA APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(MSA
    PUBLIC
        Qt::Core
        Common
        Imap
        OAuth
        qwwsmtpclient
    PRIVATE
        UiUtils
)

add_library(Composer STATIC ${libComposer_SOURCES})
set_property(TARGET Composer APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Composer
    PUBLIC
        Qt::Core
        Common
        Imap
    PRIVATE
        Qt::Network
        MSA
        UiUtils
)

add_library(Imap STATIC ${libImap_SOURCES})
set_property(TARGET Imap APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Imap
    PUBLIC
        Qt::Core
        Qt::Core5Compat
        Qt::Network
        Qt::Sql
        Common
        UiUtils
    PRIVATE
        Qt::Gui
        KF6::ItemModels
        LocalMail
        OAuth
        Streams
)
if(WITH_KFILEMETADATA)
    target_link_libraries(Imap PRIVATE KF6::FileMetaData)
endif()

add_library(Cryptography STATIC ${libCryptography_SOURCES})
set_property(TARGET Cryptography APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_link_libraries(Cryptography
    PUBLIC
        Qt::Core
    PRIVATE
        Common
        Imap
        UiUtils
)

if(WITH_WEBENGINE)
    add_library(WebEngineMessageRenderer STATIC
        src/Gui/WebEngineMessagePage.cpp
        src/Gui/WebEngineMessagePage.h
        src/Gui/WebEngineMessageProfile.cpp
        src/Gui/WebEngineMessageProfile.h
    )
    target_link_libraries(WebEngineMessageRenderer
        PUBLIC Qt6::WebEngineWidgets Imap
    )
    target_compile_definitions(WebEngineMessageRenderer PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
endif()

if(WITH_MIMETIC)
    target_link_libraries(Cryptography
        PRIVATE
            Mimetic::Mimetic
    )
endif()
if(WITH_CRYPTO_MESSAGES)
    if(WITH_GPGMEPP)
        target_link_libraries(Cryptography
            PRIVATE
                Gpgmepp
                QGpgmeQt6
        )
    endif()
endif()

## ClearText password plugin
if(WITH_CLEARTEXT_PLUGIN)
    impresario_add_plugin(trojita_plugin_ClearTextPasswordPlugin WITH_CLEARTEXT_PLUGIN src/Plugins/ClearTextPassword/ClearTextPassword.cpp)
endif()

## QtKeyChain plugin
if(WITH_QTKEYCHAIN_PLUGIN)
    impresario_add_plugin(trojita_plugin_QtKeychainPasswordPlugin WITH_QTKEYCHAIN_PLUGIN src/Plugins/QtKeyChain/QtKeyChainPassword.cpp)
    target_link_libraries(trojita_plugin_QtKeychainPasswordPlugin
        PRIVATE
            qt6keychain
     )
endif()

## AbookAddressbook plugin
if(WITH_ABOOKADDRESSBOOK_PLUGIN)
    set(path_AbookAddressbook ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins/AbookAddressbook)
    set(libAbookAddressbook_HEADERS
        ${path_AbookAddressbook}/AbookAddressbook.h
        ${path_AbookAddressbook}/be-contacts.h
    )
    set(libAbookAddressbook_SOURCES
        ${path_AbookAddressbook}/AbookAddressbook.cpp
        ${path_AbookAddressbook}/be-contacts.cpp
    )

    impresario_add_plugin(trojita_plugin_AbookAddressbookPlugin WITH_ABOOKADDRESSBOOK_PLUGIN ${libAbookAddressbook_SOURCES})
    set_property(TARGET trojita_plugin_AbookAddressbookPlugin APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

    target_link_libraries(trojita_plugin_AbookAddressbookPlugin
        PUBLIC
            Qt::Widgets
    )

    set(be_contacts_SOURCES
        ${path_AbookAddressbook}/main.cpp
    )

    add_executable(be.contacts WIN32 ${be_contacts_SOURCES})
    set_property(TARGET be.contacts APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(be.contacts
        PUBLIC
            Plugins
    )
    if("${WITH_ABOOKADDRESSBOOK_PLUGIN}" STREQUAL "STATIC")
        set_property(TARGET be.contacts APPEND PROPERTY COMPILE_DEFINITIONS QT_STATICPLUGIN)
        target_link_libraries(be.contacts
            PUBLIC
                trojita_plugin_AbookAddressbookPlugin
        )
    endif()
    target_link_libraries(be.contacts
        PUBLIC
            Qt::Widgets
    )
endif()

## AkonadiAddressbook plugin
if(WITH_AKONADIADDRESSBOOK_PLUGIN)
    set(path_AkonadiAddressbook ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins/AkonadiAddressbook)
    set(libAkonadiAddressbook_HEADERS
        ${path_AkonadiAddressbook}/AkonadiAddressbook.h
        ${path_AkonadiAddressbook}/AkonadiAddressbookCompletionJob.h
        ${path_AkonadiAddressbook}/AkonadiAddressbookContactJob.h
        ${path_AkonadiAddressbook}/AkonadiAddressbookNamesJob.h
    )
    set(libAkonadiAddressbook_SOURCES
        ${path_AkonadiAddressbook}/AkonadiAddressbook.cpp
        ${path_AkonadiAddressbook}/AkonadiAddressbookCompletionJob.cpp
        ${path_AkonadiAddressbook}/AkonadiAddressbookContactJob.cpp
        ${path_AkonadiAddressbook}/AkonadiAddressbookNamesJob.cpp
    )

    impresario_add_plugin(trojita_plugin_AkonadiAddressbookPlugin WITH_AKONADIADDRESSBOOK_PLUGIN ${libAkonadiAddressbook_SOURCES})
    target_link_libraries(trojita_plugin_AkonadiAddressbookPlugin
        PUBLIC
            KPim6::AkonadiContactCore
            KPim6::AkonadiContactWidgets
    )
    set_property(TARGET trojita_plugin_AkonadiAddressbookPlugin APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
endif()

if(WITH_SONNET_PLUGIN)
    impresario_add_plugin(trojita_plugin_Sonnet WITH_SONNET_PLUGIN src/Plugins/SonnetSpellchecker/SonnetSpellchecker.cpp)
    set_property(TARGET trojita_plugin_Sonnet APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(trojita_plugin_Sonnet
        PRIVATE
            KF6::SonnetCore
            KF6::SonnetUi
    )
endif()

# Generate file static_plugins.h.in
get_property(STATIC_PLUGINS GLOBAL PROPERTY IMPRESARIO_STATIC_PLUGINS)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "#include <QtPlugin>\n")
foreach(PLUGIN ${STATIC_PLUGINS})
    file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in "Q_IMPORT_PLUGIN(${PLUGIN})\n")
endforeach()
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h.in ${CMAKE_CURRENT_BINARY_DIR}/static_plugins.h)

if(WITH_DESKTOP)
    add_library(DesktopGui STATIC ${libDesktopGui_SOURCES})
    set_property(TARGET DesktopGui APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    # The following is needed for the LineEdit widget within the .ui files.
    # The ${path_DesktopGui} is needed so that the generated ui_*.h file can find the headers of the custom widgets
    set_property(TARGET DesktopGui APPEND PROPERTY INCLUDE_DIRECTORIES ${path_DesktopGui})
    target_link_libraries(DesktopGui
        PUBLIC
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Common
            Composer
            DesktopServices
            Imap
        PRIVATE
            Qt::DBus
            Qt::Network
            KF6::ColorScheme
            KF6::ConfigCore
            KF6::I18n
            KF6::KIOGui
            KF6::KIOWidgets
            KF6::WidgetsAddons
            KF6::WindowSystem
            Cryptography
            IPC
            Jmap
            Pop3
            MSA
            OAuth
            UiUtils
    )

    if(WITH_KNOTIFICATIONS)
        target_link_libraries(DesktopGui PRIVATE KF6::Notifications)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/plasma-impresario.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
    endif()

    if(WITH_WEBENGINE)
        target_link_libraries(DesktopGui
            PUBLIC
                WebEngineMessageRenderer
    )
    endif()

    ecm_create_qm_loader(impresario_desktop_SOURCES trojita_qt)

    # On Windows build a real Win32 GUI application without console window
    # On other platforms WIN32 flag is ignored
    add_executable(trojita WIN32 ${impresario_desktop_SOURCES})
    set_target_properties(trojita PROPERTIES OUTPUT_NAME plasma-impresario)
    if(UNIX AND NOT APPLE)
        if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
            set_target_properties(trojita PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
        else()
            set_target_properties(trojita PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
        endif()
    endif()
    set_property(TARGET trojita APPEND PROPERTY COMPILE_DEFINITIONS QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    target_link_libraries(trojita
        PRIVATE
            AppVersion
            Common
            DesktopGui
            IPC
            UiUtils
            KF6::ConfigCore
            KF6::CoreAddons
            KF6::I18n
            ${STATIC_PLUGINS}
    )

    if(KF6DocTools_FOUND)
        kdoctools_install(po)
        kdoctools_create_handbook(${CMAKE_CURRENT_SOURCE_DIR}/docs/old/index.docbook INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en SUBDIR trojita)
    endif()

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/docs/old/trojita.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

if(WITH_PLASMA_WIDGETS)
    # Shared views and the D-Bus client for the status and ticker widgets; see docs/integrations/kde-plasma.md
    include(ECMQmlModule)
    # Plasma loads the module as a plugin, so it is shared even though the application's libraries are static
    set(plasma_widgets_shared_libs ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS ON)
    ecm_add_qml_module(PlasmaImpresarioWidgets URI "io.github.mikepadiernos.plasmaimpresario" GENERATE_PLUGIN_SOURCE)
    set(BUILD_SHARED_LIBS ${plasma_widgets_shared_libs})
    target_sources(PlasmaImpresarioWidgets PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PlasmaWidgets/MailStatusClient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PlasmaWidgets/MailStatusClient.h)
    set(plasma_widgets_qml AccountChooser.qml MailTicker.qml StatusSummary.qml)
    foreach(plasma_widgets_file ${plasma_widgets_qml})
        set_source_files_properties(src/PlasmaWidgets/qml/${plasma_widgets_file} PROPERTIES QT_RESOURCE_ALIAS ${plasma_widgets_file})
        ecm_target_qml_sources(PlasmaImpresarioWidgets SOURCES src/PlasmaWidgets/qml/${plasma_widgets_file})
    endforeach()
    target_compile_definitions(PlasmaImpresarioWidgets PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
    # The generated type registration includes the client's header by its file name
    target_include_directories(PlasmaImpresarioWidgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/PlasmaWidgets)
    target_link_libraries(PlasmaImpresarioWidgets PRIVATE Qt6::Quick Qt6::DBus KF6::WindowSystem KF6::KIOGui KF6::Service)
    ecm_finalize_qml_module(PlasmaImpresarioWidgets DESTINATION ${KDE_INSTALL_QMLDIR})
    install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PlasmaWidgets/applets/io.github.mikepadiernos.plasma_impresario.status
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PlasmaWidgets/applets/io.github.mikepadiernos.plasma_impresario.ticker
        DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids)
endif()

if(WITH_SHARED_PLUGINS)
    install(TARGETS Plugins DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

include(SanitizedDesktopFile)

if(WITH_ABOOKADDRESSBOOK_PLUGIN)
    install(TARGETS be.contacts RUNTIME DESTINATION bin)
endif()

if(WITH_DESKTOP)
    copy_desktop_file_without_cruft("${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/io.github.mikepadiernos.plasma_impresario.desktop" "${CMAKE_CURRENT_BINARY_DIR}/io.github.mikepadiernos.plasma_impresario.desktop")
    install(TARGETS trojita RUNTIME DESTINATION bin)
    if(UNIX)
        install(CODE "execute_process(
            COMMAND \"${CMAKE_COMMAND}\" -E create_symlink \"plasma-impresario${CMAKE_EXECUTABLE_SUFFIX}\"
                \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/trojita${CMAKE_EXECUTABLE_SUFFIX}\"
            RESULT_VARIABLE alias_result)
            if(NOT alias_result STREQUAL \"0\")
                message(FATAL_ERROR \"Could not install trojita compatibility alias: \${alias_result}\")
            endif()")
    else()
        install(PROGRAMS $<TARGET_FILE:trojita> DESTINATION bin RENAME trojita${CMAKE_EXECUTABLE_SUFFIX})
    endif()
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.mikepadiernos.plasma_impresario.desktop
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/org.kde.trojita.desktop DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications/"
        COMPONENT DesktopLaunchers)
    if(UNIX AND NOT APPLE)
        configure_file(cmake/InstallDesktopLaunchers.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/InstallDesktopLaunchers.cmake" @ONLY)
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallDesktopLaunchers.cmake" COMPONENT DesktopLaunchers)
    endif()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/io.github.mikepadiernos.plasma_impresario.metainfo.xml DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo/")
    if(WITH_DBUS)
        # Plasma Search asks the running application; the setting in the application decides whether it answers
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Gui/plasma-runner-plasma-impresario.desktop
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/krunner/dbusplugins/")
    endif()
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/plasma-impresario.svg DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/plasma-impresario.svg DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/"
        RENAME io.github.mikepadiernos.plasma_impresario.svg COMPONENT ApplicationIcon)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps/")
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/icons/trojita.svg DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/")
    if(UNIX AND NOT APPLE)
        configure_file(cmake/RefreshIconCache.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/RefreshIconCache.cmake" @ONLY)
        install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/RefreshIconCache.cmake" COMPONENT ApplicationIcon)
    endif()
endif()

if(WITH_NSIS)
    include(ImpresarioNSIS)
endif()


if(BUILD_TESTING)
    set(test_LibMailboxSync_SOURCES
        tests/Utils/ModelEvents.cpp
        tests/Utils/LibMailboxSync.cpp
    )
    add_library(test_LibMailboxSync STATIC ${test_LibMailboxSync_SOURCES})
    set_property(TARGET test_LibMailboxSync APPEND PROPERTY INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utils)
    target_link_libraries(test_LibMailboxSync
        PUBLIC
            Qt::Core
            Qt::Test
            Common
            Imap
            Streams
        PRIVATE
            Streams
    )

    macro(impresario_test dir fname)
        set(test_${fname}_SOURCES tests/${dir}/test_${fname}.cpp)
        add_executable(test_${fname} ${test_${fname}_SOURCES})
        target_link_libraries(test_${fname}
            PRIVATE
                Common
                Composer
                Cryptography
                Imap
                LocalMail
                MSA
                Streams
                test_LibMailboxSync
        )
        set_property(TARGET test_${fname} APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(test_${fname} test_${fname})
        endif()
    endmacro()

    set(UBSAN_ENV_SUPPRESSIONS "UBSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/ubsan.supp")

    enable_testing()
    if(WITH_DESKTOP)
        add_executable(test_DensityMenu tests/Gui/test_DensityMenu.cpp)
        target_compile_definitions(test_DensityMenu PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
        target_link_libraries(test_DensityMenu PRIVATE DesktopGui UiUtils Qt6::Widgets Qt6::Test KF6::ConfigCore KF6::I18n)
        add_executable(test_AttachmentPanel tests/Gui/test_AttachmentPanel.cpp)
        target_link_libraries(test_AttachmentPanel PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_FileChooser tests/Gui/test_FileChooser.cpp)
        target_link_libraries(test_FileChooser PRIVATE DesktopGui Qt6::DBus Qt6::Test)
        add_executable(test_SavedFilesSearchDialog tests/Gui/test_SavedFilesSearchDialog.cpp)
        target_link_libraries(test_SavedFilesSearchDialog PRIVATE DesktopGui UiUtils Qt6::Test KF6::ConfigCore KF6::I18n)
        add_executable(test_SearchIndexDialog tests/Gui/test_SearchIndexDialog.cpp)
        target_link_libraries(test_SearchIndexDialog PRIVATE DesktopGui UiUtils Qt6::Test KF6::I18n)
        add_executable(test_TagListWidget tests/Gui/test_TagListWidget.cpp)
        target_link_libraries(test_TagListWidget PRIVATE DesktopGui UiUtils Qt6::Test KF6::I18n)
        add_executable(test_ReaderActionBar tests/Gui/test_ReaderActionBar.cpp)
        target_link_libraries(test_ReaderActionBar PRIVATE DesktopGui UiUtils Qt6::Test KF6::I18n)
        add_executable(test_Appearance tests/Gui/test_Appearance.cpp)
        target_link_libraries(test_Appearance PRIVATE DesktopGui UiUtils Qt6::Test KF6::ConfigCore KF6::I18n)
        add_executable(test_EmbeddedPanels tests/Gui/test_EmbeddedPanels.cpp)
        target_link_libraries(test_EmbeddedPanels PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_AgendaPanel tests/Gui/test_AgendaPanel.cpp)
        target_link_libraries(test_AgendaPanel PRIVATE DesktopGui UiUtils Qt6::Test KF6::I18n)
        if(WITH_ABOOKADDRESSBOOK_PLUGIN STREQUAL "STATIC")
            add_executable(test_ContactPanelAddressBook tests/Gui/test_ContactPanelAddressBook.cpp)
            target_link_libraries(test_ContactPanelAddressBook PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        endif()
        add_executable(test_MsgListViewTeardown tests/Gui/test_MsgListViewTeardown.cpp)
        target_link_libraries(test_MsgListViewTeardown PRIVATE DesktopGui Qt6::Test KF6::I18n)
        add_executable(test_MainWindowDensity tests/Gui/test_MainWindowDensity.cpp)
        target_link_libraries(test_MainWindowDensity PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LayoutReadability tests/Gui/test_LayoutReadability.cpp)
        target_link_libraries(test_LayoutReadability PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_MainWindowTabs tests/Gui/test_MainWindowTabs.cpp)
        target_link_libraries(test_MainWindowTabs PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_VimMode tests/Gui/test_VimMode.cpp)
        target_link_libraries(test_VimMode PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_MailBoxTreeView tests/Gui/test_MailBoxTreeView.cpp)
        target_link_libraries(test_MailBoxTreeView PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_MailboxNavigation tests/Gui/test_MailboxNavigation.cpp)
        target_link_libraries(test_MailboxNavigation PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_MainWindowAccounts tests/Gui/test_MainWindowAccounts.cpp)
        target_link_libraries(test_MainWindowAccounts PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_MainWindowShell tests/Gui/test_MainWindowShell.cpp)
        target_link_libraries(test_MainWindowShell PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_Accessibility tests/Gui/test_Accessibility.cpp)
        target_link_libraries(test_Accessibility PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ColorScheme KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_EnvelopeView tests/Gui/test_EnvelopeView.cpp)
        target_link_libraries(test_EnvelopeView PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LinkTarget tests/Gui/test_LinkTarget.cpp)
        target_link_libraries(test_LinkTarget PRIVATE DesktopGui UiUtils Qt6::Test KF6::I18n)
        add_executable(test_MailSafetyPrompt tests/Gui/test_MailSafetyPrompt.cpp)
        target_link_libraries(test_MailSafetyPrompt PRIVATE DesktopGui UiUtils Qt6::Network Qt6::Test KF6::ConfigCore KF6::I18n)
        add_executable(test_SavedSearch tests/Gui/test_SavedSearch.cpp)
        target_link_libraries(test_SavedSearch PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_ClearTextPassword tests/Gui/test_ClearTextPassword.cpp)
        target_link_libraries(test_ClearTextPassword PRIVATE Plugins Qt6::Test ${STATIC_PLUGINS})
        add_executable(test_PasswordBackend tests/Gui/test_PasswordBackend.cpp)
        target_link_libraries(test_PasswordBackend PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImap tests/Gui/test_LiveImap.cpp)
        target_link_libraries(test_LiveImap PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImapAccounts tests/Gui/test_LiveImapAccounts.cpp)
        target_link_libraries(test_LiveImapAccounts PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImapPanels tests/Gui/test_LiveImapPanels.cpp)
        target_link_libraries(test_LiveImapPanels PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImapCompose tests/Gui/test_LiveImapCompose.cpp)
        target_link_libraries(test_LiveImapCompose PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Network Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImapOperations tests/Gui/test_LiveImapOperations.cpp)
        target_link_libraries(test_LiveImapOperations PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_LiveImapSearch tests/Gui/test_LiveImapSearch.cpp)
        target_link_libraries(test_LiveImapSearch PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Network Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_InlineNotificationBar tests/Gui/test_InlineNotificationBar.cpp)
        target_link_libraries(test_InlineNotificationBar PRIVATE DesktopGui Qt6::Test KF6::I18n)
        add_executable(test_AddAccountDialog tests/Gui/test_AddAccountDialog.cpp)
        target_link_libraries(test_AddAccountDialog PRIVATE DesktopGui OAuth UiUtils Qt6::Test KF6::I18n)
        set_property(TARGET test_AddAccountDialog APPEND PROPERTY INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_executable(test_MessageSelectionRestorer tests/Gui/test_MessageSelectionRestorer.cpp)
        target_link_libraries(test_MessageSelectionRestorer PRIVATE DesktopGui Qt6::Test)
        add_executable(test_MessageRows tests/Gui/test_MessageRows.cpp)
        target_link_libraries(test_MessageRows PRIVATE DesktopGui Qt6::Test KF6::ConfigCore KF6::I18n)
        add_executable(test_MsgListViewSelection tests/Gui/test_MsgListViewSelection.cpp)
        target_link_libraries(test_MsgListViewSelection PRIVATE DesktopGui Qt6::Test KF6::I18n)
        add_executable(test_SessionRestart tests/Gui/test_SessionRestart.cpp)
        target_link_libraries(test_SessionRestart PRIVATE AppVersion DesktopGui IPC UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_AccountSettingsDialog tests/Gui/test_AccountSettingsDialog.cpp)
        target_link_libraries(test_AccountSettingsDialog PRIVATE AppVersion DesktopGui IPC Pop3 Jmap UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        add_executable(test_SettingsDialog tests/Gui/test_SettingsDialog.cpp)
        target_link_libraries(test_SettingsDialog PRIVATE AppVersion DesktopGui IPC Pop3 Jmap UiUtils Qt6::Test KF6::ConfigCore KF6::I18n ${STATIC_PLUGINS})
        # The settings groups derive from the pages of the settings dialog, whose forms uic generates
        add_dependencies(test_SettingsDialog DesktopGui)
        target_include_directories(test_SettingsDialog PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/DesktopGui_autogen/include ${path_DesktopGui})
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(NAME test_DensityMenu COMMAND test_DensityMenu)
            set_tests_properties(test_DensityMenu PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_AttachmentPanel COMMAND test_AttachmentPanel)
            set_tests_properties(test_AttachmentPanel PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_FileChooser COMMAND test_FileChooser)
            set_tests_properties(test_FileChooser PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_SavedFilesSearchDialog COMMAND test_SavedFilesSearchDialog)
            set_tests_properties(test_SavedFilesSearchDialog PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_SearchIndexDialog COMMAND test_SearchIndexDialog)
            set_tests_properties(test_SearchIndexDialog PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_TagListWidget COMMAND test_TagListWidget)
            set_tests_properties(test_TagListWidget PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_ReaderActionBar COMMAND test_ReaderActionBar)
            set_tests_properties(test_ReaderActionBar PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_Appearance COMMAND test_Appearance)
            set_tests_properties(test_Appearance PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_EmbeddedPanels COMMAND test_EmbeddedPanels)
            add_test(NAME test_AgendaPanel COMMAND test_AgendaPanel)
            set_tests_properties(test_AgendaPanel PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            set_tests_properties(test_EmbeddedPanels PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_SettingsDialog COMMAND test_SettingsDialog)
            set_tests_properties(test_SettingsDialog PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            if(TARGET test_ContactPanelAddressBook)
                add_test(NAME test_ContactPanelAddressBook COMMAND test_ContactPanelAddressBook)
                set_tests_properties(test_ContactPanelAddressBook PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            endif()
            add_test(NAME test_MsgListViewTeardown COMMAND test_MsgListViewTeardown)
            set_tests_properties(test_MsgListViewTeardown PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_VimMode COMMAND test_VimMode)
            set_tests_properties(test_VimMode PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MailBoxTreeView COMMAND test_MailBoxTreeView)
            set_tests_properties(test_MailBoxTreeView PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MailboxNavigation COMMAND test_MailboxNavigation)
            set_tests_properties(test_MailboxNavigation PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_Accessibility COMMAND test_Accessibility)
            set_tests_properties(test_Accessibility PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_EnvelopeView COMMAND test_EnvelopeView)
            set_tests_properties(test_EnvelopeView PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_LinkTarget COMMAND test_LinkTarget)
            set_tests_properties(test_LinkTarget PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MailSafetyPrompt COMMAND test_MailSafetyPrompt)
            set_tests_properties(test_MailSafetyPrompt PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_SavedSearch COMMAND test_SavedSearch)
            set_tests_properties(test_SavedSearch PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_ClearTextPassword COMMAND test_ClearTextPassword)
            add_test(NAME test_PasswordBackend COMMAND test_PasswordBackend)
            set_tests_properties(test_PasswordBackend PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_InlineNotificationBar COMMAND test_InlineNotificationBar)
            set_tests_properties(test_InlineNotificationBar PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_AddAccountDialog COMMAND test_AddAccountDialog)
            set_tests_properties(test_AddAccountDialog PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MessageSelectionRestorer COMMAND test_MessageSelectionRestorer)
            set_tests_properties(test_MessageSelectionRestorer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MessageRows COMMAND test_MessageRows)
            set_tests_properties(test_MessageRows PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MsgListViewSelection COMMAND test_MsgListViewSelection)
            set_tests_properties(test_MsgListViewSelection PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            add_test(NAME test_MainWindowDensity COMMAND test_MainWindowDensity)
            set(reader_account_cases)
            if(WITH_WEBENGINE)
                set(reader_account_cases focusFollowsScreenOrder escapeReturnsToMessageList findMovesToSearchOrFindsInMessage)
            endif()
            foreach(account_case ${reader_account_cases} migratesSingleProfileIntoFirstAccount switchingKeepsAccountsIndependent backgroundAccountStatusIsShown
                    systemSleepReachesEveryAccount
                    removingAccountDeletesItsSettingsAndPasswords accountsSwitchFromTheKeyboard
                    savedSearchesAndFolderExpansionFollowTheAccount savedSearchesAreVirtualFoldersOfEveryAccount
                    savedSearchesCanBeManaged
                    focusCyclesThroughShellRegions
                    findSavedFilesOpensTheDialog goToFolderActionsWithoutFolders checkMailAndNextUnreadFolder destructiveActionsOffline quickActionsOffline
                    messageListFiltersOffline shortcutOverviewAndSelectionOffline powerUserProcessingOffline
                    groupingAndRelevanceOffline folderViewStateOffline unifiedInboxOffline unifiedInboxIsRestored
                    identitiesFollowTheAccount composerSendsWithItsOwnAccount composerChoosesItsAccount
                    newAccountsStartWithoutIdentities
                    oauthAccountsOfferSignIn)
                add_test(NAME test_MainWindowAccounts_${account_case} COMMAND test_MainWindowAccounts ${account_case})
                set_tests_properties(test_MainWindowAccounts_${account_case} PROPERTIES
                    TIMEOUT 60
                    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            endforeach()
            foreach(shell_case capabilityGateHidesOnlyUnsupportedActions unsupportedActionsFollowTheActiveAccount
                    sidebarShowsHeadersOnlyForSeveralAccounts sidebarGroupsAccountsInTheWindow sidebarHeaderMenuActsOnItsAccount
                    headerRegionIsOptional headerShowsFolderAndStatus legacyCompactPaneSizesCarryOver
                    legacyWidePaneSizesCarryOver closingQuitsByDefault closingHidesWhenRunningInBackground
                    requestsBringTheirWindowForward sessionEndSavesUnsentMessages)
                add_test(NAME test_MainWindowShell_${shell_case} COMMAND test_MainWindowShell ${shell_case})
                set_tests_properties(test_MainWindowShell_${shell_case} PROPERTIES
                    TIMEOUT 60
                    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            endforeach()
            add_test(NAME test_SessionRestart COMMAND test_SessionRestart)
            set_tests_properties(test_SessionRestart PROPERTIES TIMEOUT 120 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            # One main window per process: the shortcut handler is a singleton.
            foreach(settings_case accountsKeepTheirOwnCaches applicationSettingsLeaveTheAccountAlone
                    accountSettingsEditOnlyTheActiveAccount cancellingAccountSettingsKeepsTheAccount
                    addingAnAccountOpensItsAccountSettings clientSecretStaysOutOfTheSettings
                    searchIndexIsChosenPerAccount searchIndexAttachmentsAreChosenPerAccount
                    searchIndexPrivacyIsKeptPerAccount memoryOnlyCacheLeavesNoIndex startAfterLoginNeedsBackgroundRunning
                    plasmaSearchIsOptIn pop3AccountsHaveTheirOwnSettings jmapAccountsHaveTheirOwnSettings
                    jmapAccountsSynchronizeWithTheServer gmailKeepsItsOwnSentCopiesAndArchiveIsAutomatic)
                add_test(NAME test_AccountSettingsDialog_${settings_case} COMMAND test_AccountSettingsDialog ${settings_case})
                set_tests_properties(test_AccountSettingsDialog_${settings_case} PROPERTIES
                    TIMEOUT 60
                    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            endforeach()
            set_tests_properties(test_MainWindowDensity PROPERTIES
                TIMEOUT 30
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            add_test(NAME test_MainWindowTabs COMMAND test_MainWindowTabs)
            set_tests_properties(test_MainWindowTabs PROPERTIES
                TIMEOUT 60
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            add_test(NAME test_LayoutReadability COMMAND test_LayoutReadability)
            set_tests_properties(test_LayoutReadability PROPERTIES
                TIMEOUT 120
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            add_test(NAME test_LayoutReadabilityScaled COMMAND test_LayoutReadability)
            set_tests_properties(test_LayoutReadabilityScaled PROPERTIES
                TIMEOUT 120
                ENVIRONMENT "IMPRESARIO_TEST_DPR=2;QT_QPA_PLATFORM=offscreen:configfile=${CMAKE_CURRENT_SOURCE_DIR}/tests/Gui/offscreen-hidpi.json;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
            add_test(NAME test_MainWindowDensityNamedProfile COMMAND test_MainWindowDensity)
            set_tests_properties(test_MainWindowDensityNamedProfile PROPERTIES
                TIMEOUT 30
                ENVIRONMENT "PLASMA_TEST_PROFILE=named;QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
        endif()
    endif()
    if(WITH_DESKTOP AND UNIX AND NOT APPLE)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        find_program(GTK_UPDATE_ICON_CACHE_TEST_EXECUTABLE NAMES gtk-update-icon-cache)
        if(Python3_Interpreter_FOUND AND GTK_UPDATE_ICON_CACHE_TEST_EXECUTABLE AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME test_ApplicationIconInstall COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_icon_install.py ${CMAKE_COMMAND}
                ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_INSTALL_DATAROOTDIR}
                ${GTK_UPDATE_ICON_CACHE_TEST_EXECUTABLE})
        endif()
    endif()
    if(WITH_WEBENGINE)
        add_executable(test_WebEngineMessageProfile tests/Gui/test_WebEngineMessageProfile.cpp)
        target_link_libraries(test_WebEngineMessageProfile PRIVATE WebEngineMessageRenderer UiUtils Qt6::Test)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(NAME test_WebEngineMessageProfile COMMAND test_WebEngineMessageProfile)
            set_tests_properties(test_WebEngineMessageProfile PROPERTIES ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
        endif()
    endif()
    impresario_test(Composer Composer_Submission)
    impresario_test(Composer Composer_Existing)
    impresario_test(Composer Composer_AttachmentRequests)
    impresario_test(Composer Composer_HeaderSafety)
    impresario_test(Composer Composer_responses)
    target_link_libraries(test_Composer_responses)
    impresario_test(Composer Html_formatting)
    target_link_libraries(test_Html_formatting
        PRIVATE
            Qt::Widgets
    )
    if(WITH_WEBENGINE)
        target_link_libraries(test_Html_formatting
            PRIVATE
                Qt6::WebEngineWidgets
        )
        set_tests_properties(test_Html_formatting PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;QTWEBENGINE_DISABLE_SANDBOX=1")
    endif()
    impresario_test(Imap Imap_DisappearingMailboxes)
    impresario_test(Imap Imap_Idle)
    impresario_test(Imap Imap_LowLevelParser)
    impresario_test(Imap Imap_Message)
    impresario_test(Imap Imap_Model)
    impresario_test(Imap Imap_MsgPartNetAccessManager)
    set_property(TEST test_Imap_MsgPartNetAccessManager PROPERTY ENVIRONMENT "${UBSAN_ENV_SUPPRESSIONS}")
    impresario_test(Imap Imap_Parser_parse)
    impresario_test(Imap Imap_Parser_write)
    impresario_test(Imap Imap_Responses)
    impresario_test(Imap Imap_SelectedMailboxUpdates)
    impresario_test(Imap Imap_Tasks_CreateMailbox)
    impresario_test(Imap Imap_Tasks_DeleteMailbox)
    impresario_test(Imap Imap_Tasks_RenameMailbox)
    impresario_test(Imap Imap_Tasks_ListChildMailboxes)
    impresario_test(Imap Imap_Tasks_ObtainSynchronizedMailbox)
    impresario_test(Imap Imap_Tasks_OpenConnection)
    impresario_test(Imap Imap_Threading)
    impresario_test(Imap Imap_ViewStatePreservation)
    impresario_test(Imap Imap_BodyParts)
    impresario_test(Imap Imap_Offline)
    impresario_test(Imap Imap_CopyAndFlagOperations)
    impresario_test(Cryptography Cryptography_MessageModel)

    if(WITH_CRYPTO_MESSAGES)
      find_program(GPGCONF_BINARY NAMES gpgconf)
      if(GPGCONF_BINARY_NOTFOUND)
        message(SEND_ERROR "The `gpgconf` binary from GnuPG not found, this is needed for crypto tests.")
      endif()

      if(NOT UNIX)
        message(SEND_ERROR "The Cryptography unit tests really need Unix. Patches welcome.")
      endif()

      add_library(fake-dev-random SHARED ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utils/fake-dev-random.c)
      set_target_properties(fake-dev-random PROPERTIES AUTOMOC off)
      target_link_libraries(fake-dev-random ${CMAKE_DL_LIBS})

      # FIXME: it would be nice to depend on the contents of keys/, but in my testing it produces Makefiles
      # which suffer from races (the keygen.sh is run multiple times in parallel within the CI environment).
      # I wasn't able to track down the root cause behind this; it affected all cmake versions within the CI
      # as of Feb 2016, which is 3.1.something up to 3.3.something.
      add_custom_command(OUTPUT crypto_test_data.h
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/Cryptography/keygen.sh ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS fake-dev-random tests/Cryptography/keygen.sh tests/Cryptography/batch-keygen)
      add_custom_target(crypto_test_data DEPENDS crypto_test_data.h)

      impresario_test(Cryptography Cryptography_PGP)
      target_link_libraries(test_Cryptography_PGP PRIVATE UiUtils)
      set_property(TEST test_Cryptography_PGP PROPERTY ENVIRONMENT "${UBSAN_ENV_SUPPRESSIONS}")
      add_dependencies(test_Cryptography_PGP crypto_test_data)
    endif()

    impresario_test(Misc PrivateFiles)
    impresario_test(LocalMail LocalMailMime)
    impresario_test(LocalMail LocalMailStore)
    impresario_test(LocalMail LocalImapServer)
    impresario_test(Pop3 Pop3Fetch)
    target_link_libraries(test_Pop3Fetch PRIVATE Pop3)
    # Opt-in, outside ctest: tools/dev-container/live-pop3.sh starts the server it needs
    add_executable(test_LivePop3 tests/Pop3/test_LivePop3.cpp)
    target_link_libraries(test_LivePop3 PRIVATE LocalMail Pop3 Qt6::Network Qt6::Test)
    impresario_test(Jmap JmapSync)
    target_link_libraries(test_JmapSync PRIVATE Jmap)
    impresario_test(Jmap JmapSubmission)
    target_link_libraries(test_JmapSubmission PRIVATE Jmap)
    # Opt-in, outside ctest: tools/dev-container/live-jmap.sh starts the server it needs
    add_executable(test_LiveJmap tests/Jmap/test_LiveJmap.cpp)
    target_link_libraries(test_LiveJmap PRIVATE Jmap LocalMail Qt6::Network Qt6::Test)
    impresario_test(Misc Rfc5322)
    impresario_test(Misc ApplicationProfile)
    impresario_test(Misc RingBuffer)
    impresario_test(Misc SenderIdentitiesModel)
    impresario_test(Misc SqlCache)
    impresario_test(Misc algorithms)
    impresario_test(Misc rfccodecs)
    impresario_test(Misc prettySize)
    impresario_test(Misc Formatting)
    impresario_test(Misc QaimDfsIterator)
    impresario_test(Misc FavoriteTagsModel)
    impresario_test(Misc AccountCache)
    impresario_test(Misc TrustedCertificates)
    impresario_test(Misc AccountCapabilities)
    impresario_test(Misc AccountRegistry)
    impresario_test(Misc AccountIdentities)
    impresario_test(Misc AccountStatus)
    impresario_test(Misc OAuth)
    impresario_test(Misc SmtpClient)
    impresario_test(Misc AttachmentCollector)
    target_link_libraries(test_AttachmentCollector PRIVATE UiUtils)
    impresario_test(Misc RelatedMessages)
    target_link_libraries(test_RelatedMessages PRIVATE UiUtils)
    impresario_test(Misc Agenda)
    target_link_libraries(test_Agenda PRIVATE UiUtils)
    impresario_test(Misc RelatedSearch)
    target_link_libraries(test_RelatedSearch PRIVATE UiUtils)
    impresario_test(Misc MessageId)
    target_link_libraries(test_MessageId PRIVATE UiUtils)
    impresario_test(Misc MessageSecurity)
    target_link_libraries(test_MessageSecurity PRIVATE UiUtils)
    impresario_test(Misc QuickFilters)
    target_link_libraries(test_QuickFilters PRIVATE UiUtils)
    impresario_test(Misc MailingLists)
    target_link_libraries(test_MailingLists PRIVATE UiUtils)
    impresario_test(Misc RecentFolders)
    target_link_libraries(test_RecentFolders PRIVATE UiUtils)
    impresario_test(Misc SelectionAfterRemoval)
    target_link_libraries(test_SelectionAfterRemoval PRIVATE UiUtils)
    impresario_test(Misc UnreadFolders)
    target_link_libraries(test_UnreadFolders PRIVATE UiUtils)
    impresario_test(Misc ShortcutOverview)
    target_link_libraries(test_ShortcutOverview PRIVATE UiUtils)
    impresario_test(Misc ViewStateStore)
    target_link_libraries(test_ViewStateStore PRIVATE UiUtils)
    impresario_test(Misc SpecialFolder)
    impresario_test(Misc PrettyMsgListFilters)
    impresario_test(Misc GroupingMsgListModel)
    target_link_libraries(test_GroupingMsgListModel PRIVATE UiUtils)
    impresario_test(Misc UnifiedMsgListModel)
    target_link_libraries(test_UnifiedMsgListModel PRIVATE UiUtils)
    impresario_test(Misc Relevance)
    target_link_libraries(test_Relevance PRIVATE UiUtils)
    impresario_test(Misc SearchBuilder)
    target_link_libraries(test_SearchBuilder PRIVATE UiUtils)
    impresario_test(Misc SearchQuery)
    target_link_libraries(test_SearchQuery PRIVATE UiUtils)
    impresario_test(Misc SearchScope)
    target_link_libraries(test_SearchScope PRIVATE UiUtils)
    impresario_test(Misc SearchSuggestions)
    target_link_libraries(test_SearchSuggestions PRIVATE UiUtils)
    impresario_test(Misc SearchSupport)
    impresario_test(Misc SearchResultsModel)
    impresario_test(Misc SearchIndex)
    impresario_test(Misc SearchSnippets)
    impresario_test(Misc SearchFacets)
    target_link_libraries(test_SearchFacets PRIVATE UiUtils)
    target_link_libraries(test_SearchResultsModel PRIVATE UiUtils)
    impresario_test(Misc LinkSafety)
    target_link_libraries(test_LinkSafety PRIVATE UiUtils)
    impresario_test(Misc MailSafety)
    target_link_libraries(test_MailSafety PRIVATE UiUtils)
    impresario_test(Misc ClamdScanner)
    target_link_libraries(test_ClamdScanner PRIVATE UiUtils Qt6::Network)
    impresario_test(Misc ServerDiscovery)
    target_link_libraries(test_ServerDiscovery PRIVATE UiUtils Qt6::Network)
    impresario_test(Misc ActivationToken)
    target_link_libraries(test_ActivationToken PRIVATE UiUtils)
    impresario_test(Misc Color)
    target_link_libraries(test_Color PRIVATE UiUtils)
    impresario_test(Misc AccountNotices)
    target_link_libraries(test_AccountNotices PRIVATE UiUtils)
    impresario_test(Misc MailRules)
    target_link_libraries(test_MailRules PRIVATE UiUtils)
    impresario_test(Misc NewMail)
    target_link_libraries(test_NewMail PRIVATE UiUtils)
    impresario_test(Misc NewMailWatcher)
    target_link_libraries(test_NewMailWatcher PRIVATE UiUtils)
    impresario_test(Misc ActivityLog)
    target_link_libraries(test_ActivityLog PRIVATE UiUtils)
    impresario_test(Misc OutputSurfaces)
    target_link_libraries(test_OutputSurfaces PRIVATE UiUtils)
    impresario_test(Misc SettingsSchema)
    target_link_libraries(test_SettingsSchema PRIVATE UiUtils)
    impresario_test(Misc Keymap)
    target_link_libraries(test_Keymap PRIVATE UiUtils)
    impresario_test(Misc MailToProductivity)
    target_link_libraries(test_MailToProductivity PRIVATE UiUtils)
    impresario_test(Misc MailTabs)
    target_link_libraries(test_MailTabs PRIVATE UiUtils)
    impresario_test(Misc Theme)
    target_link_libraries(test_Theme PRIVATE UiUtils)
    if(WITH_DBUS)
        add_executable(test_KRunnerBridge tests/Misc/test_KRunnerBridge.cpp src/IPC/KRunnerBridge.cpp src/IPC/KRunnerBridge.h)
        target_compile_definitions(test_KRunnerBridge PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
        target_include_directories(test_KRunnerBridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(test_KRunnerBridge PRIVATE Qt6::DBus Qt6::Test)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(test_KRunnerBridge test_KRunnerBridge)
        endif()
        add_executable(test_MailStatusBridge tests/Misc/test_MailStatusBridge.cpp src/IPC/MailStatusBridge.cpp src/IPC/MailStatusBridge.h)
        target_compile_definitions(test_MailStatusBridge PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
        target_include_directories(test_MailStatusBridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(test_MailStatusBridge PRIVATE Qt6::DBus Qt6::Test)
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(test_MailStatusBridge test_MailStatusBridge)
        endif()
    endif()
    if(WITH_PLASMA_WIDGETS)
        # The client against a fake application on a private session bus
        add_executable(test_MailStatusClient tests/PlasmaWidgets/test_MailStatusClient.cpp
            src/PlasmaWidgets/MailStatusClient.cpp src/PlasmaWidgets/MailStatusClient.h
            src/IPC/MailStatusBridge.cpp src/IPC/MailStatusBridge.h)
        target_compile_definitions(test_MailStatusClient PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
        target_include_directories(test_MailStatusClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(test_MailStatusClient PRIVATE Qt6::DBus Qt6::Qml Qt6::Gui KF6::WindowSystem KF6::KIOGui KF6::Service Qt6::Test)
        find_program(DBUS_RUN_SESSION dbus-run-session)
        if(NOT CMAKE_CROSSCOMPILING)
            if(DBUS_RUN_SESSION)
                add_test(NAME test_MailStatusClient COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_MailStatusClient>)
            else()
                add_test(NAME test_MailStatusClient COMMAND test_MailStatusClient)
            endif()
        endif()
        if(Qt6QuickTest_FOUND)
            # The shared views with a fake client
            add_executable(test_PlasmaWidgetViews tests/PlasmaWidgets/test_PlasmaWidgetViews.cpp)
            target_compile_definitions(test_PlasmaWidgetViews PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/PlasmaWidgets")
            target_link_libraries(test_PlasmaWidgetViews PRIVATE Qt6::QuickTest)
            add_dependencies(test_PlasmaWidgetViews PlasmaImpresarioWidgets)
            if(NOT CMAKE_CROSSCOMPILING)
                add_test(NAME test_PlasmaWidgetViews COMMAND test_PlasmaWidgetViews -import ${CMAKE_BINARY_DIR}/bin)
                set_tests_properties(test_PlasmaWidgetViews PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
            endif()
        endif()
    endif()
    if(WITH_DESKTOP)
        impresario_test(Misc SavedSearches)
        target_link_libraries(test_SavedSearches PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc DestructiveActions)
        target_link_libraries(test_DestructiveActions PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc FileSearch)
        target_link_libraries(test_FileSearch PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc BackgroundRunning)
        target_link_libraries(test_BackgroundRunning PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc PlasmaSearch)
        target_link_libraries(test_PlasmaSearch PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc DesktopFeedback)
        target_link_libraries(test_DesktopFeedback PRIVATE DesktopServices KF6::ConfigCore Qt6::DBus)
        impresario_test(Misc SleepMonitor)
        target_link_libraries(test_SleepMonitor PRIVATE DesktopServices Qt6::DBus)
        impresario_test(Misc MailStatus)
        target_link_libraries(test_MailStatus PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc AutomationSettings)
        target_link_libraries(test_AutomationSettings PRIVATE DesktopServices KF6::ConfigCore)
        impresario_test(Misc SettingsStore)
        target_link_libraries(test_SettingsStore PRIVATE DesktopServices UiUtils KF6::ConfigCore)
        impresario_test(Gui MailAutomation)
        target_link_libraries(test_MailAutomation PRIVATE DesktopGui UiUtils KF6::ConfigCore KF6::I18n)
        impresario_test(Gui RulesDialog)
        target_link_libraries(test_RulesDialog PRIVATE DesktopGui UiUtils KF6::I18n)
        impresario_test(Gui ActivityDialog)
        target_link_libraries(test_ActivityDialog PRIVATE DesktopGui UiUtils KF6::I18n)
        impresario_test(Gui NotificationsDialog)
        target_link_libraries(test_NotificationsDialog PRIVATE DesktopGui UiUtils KF6::I18n)
    endif()

    # The mail core neither includes nor links KDE desktop services; see docs/integrations/kde-plasma.md
    set(boundary_core_targets AppVersion Common Composer Cryptography Imap Jmap LocalMail MSA OAuth Plugins Pop3 Streams UiUtils
        qwwsmtpclient)
    set(boundary_links "")
    foreach(boundary_target ${boundary_core_targets})
        string(APPEND boundary_links "${boundary_target}=$<JOIN:$<TARGET_PROPERTY:${boundary_target},LINK_LIBRARIES>,|>\n")
    endforeach()
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ArchitectureBoundaries-links.txt CONTENT "${boundary_links}")
    set(boundary_desktop_sources "")
    foreach(boundary_source ${libDesktopServices_SOURCES} ${path_UiUtils}/AkonadiAgendaSource.cpp)
        file(RELATIVE_PATH boundary_relative ${CMAKE_CURRENT_SOURCE_DIR} ${boundary_source})
        list(APPEND boundary_desktop_sources ${boundary_relative})
    endforeach()
    list(REMOVE_DUPLICATES boundary_desktop_sources)
    string(REPLACE ";" "|" boundary_desktop_sources "${boundary_desktop_sources}")
    add_test(NAME test_ArchitectureBoundaries
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DLINKS_FILE=${CMAKE_CURRENT_BINARY_DIR}/ArchitectureBoundaries-links.txt
            "-DDESKTOP_SOURCES=${boundary_desktop_sources}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckArchitectureBoundaries.cmake)
    # The check itself, on fixtures: allowed libraries, exempt files and plugins pass; violations are named
    set(boundary_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/ArchitectureBoundaries)
    foreach(boundary_case "clean;clean-links.txt;Mail core is free of desktop services: 1 sources checked"
            "source-violation;clean-links.txt;src/Imap/Violation.cpp includes a desktop service: #  include <KConfigGroup>"
            "clean;link-violation.txt;Imap links the desktop service KF6::ConfigCore.*UiUtils links the integration target DesktopServices.*Composer links the desktop service Qt6::DBus"
            "automation-violation;clean-links.txt;src/Gui/MessageView.cpp reaches into mail automation: #include \"MailAutomation.h\".*src/Gui/ReaderPanel.h reaches into mail automation: #include \"UiUtils/MailRules.h\""
            "automation-allowed;clean-links.txt;Mail automation is reached only through the window: 2 sources checked")
        list(GET boundary_case 0 boundary_tree)
        list(GET boundary_case 1 boundary_links_fixture)
        list(GET boundary_case 2 boundary_expected)
        string(REGEX REPLACE "\\.txt$" "" boundary_name "${boundary_tree}_${boundary_links_fixture}")
        add_test(NAME test_ArchitectureBoundaries_${boundary_name}
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR=${boundary_fixtures}/${boundary_tree}
                -DLINKS_FILE=${boundary_fixtures}/${boundary_links_fixture}
                -DDESKTOP_SOURCES=src/UiUtils/Exempt.cpp
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckArchitectureBoundaries.cmake)
        set_tests_properties(test_ArchitectureBoundaries_${boundary_name} PROPERTIES PASS_REGULAR_EXPRESSION "${boundary_expected}")
    endforeach()

    # Every source file which sends something out of the application is in the reviewed register; see
    # docs/integrations/external-services.md
    add_test(NAME test_ExternalExposure
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DREGISTER=${CMAKE_CURRENT_SOURCE_DIR}/docs/integrations/external-services.md
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExternalExposure.cmake)
    # The check itself, on fixtures: a matching register passes, comments and bundled code are ignored, and
    # missing and stale entries are named
    set(exposure_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/tests/ExternalExposure)
    foreach(exposure_case "clean;External exposure register matches the sources: 1 of 2 sources send something out"
            "missing;src/Net/Fetch.cpp sends something out of the application [(]line 6: QNetworkRequest request,[)] but is not in the register"
            "stale;src/Net/Quiet.cpp is in the register but sends nothing out of the application")
        list(GET exposure_case 0 exposure_name)
        list(GET exposure_case 1 exposure_expected)
        add_test(NAME test_ExternalExposure_${exposure_name}
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR=${exposure_fixtures}/tree
                -DREGISTER=${exposure_fixtures}/register-${exposure_name}.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExternalExposure.cmake)
        set_tests_properties(test_ExternalExposure_${exposure_name} PROPERTIES PASS_REGULAR_EXPRESSION "${exposure_expected}")
    endforeach()

endif()

if(WIN32) # Check if we are on Windows
    if(MSVC10) # Check if we are using the Visual Studio compiler 2010
        # Because of linker errors (see http://stackoverflow.com/questions/5625884/conversion-of-stdwstring-to-qstring-throws-linker-error)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t-")
    elseif(MINGW)
    else()
        message(WARNING "You are using a compiler which we have not tested yet (not MSVC10 or MINGW).")
        message(WARNING "Please let us know how well it works.")
    endif()
endif()
