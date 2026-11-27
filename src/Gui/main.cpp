/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2013 - 2014 Pali Rohár <pali.rohar@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QApplication>
#include <QSessionManager>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QObject>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTimer>
#include <QTextStream>
#include <QUrl>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KSignalHandler>

#include <csignal>

#include "AppVersion/SetCoreApplication.h"
#include "Common/Application.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "configure.cmake.h"
#include "Gui/AppearanceManager.h"
#include "Gui/AppearanceStyle.h"
#include "Gui/Util.h"
#include "Gui/Window.h"
#ifdef IMPRESARIO_HAVE_WEBENGINE
#include "Gui/WebEngineMessageProfile.h"
#endif
#include "Imap/Model/SystemNetworkWatcher.h"
#include "IPC/IPC.h"
#include "UiUtils/ActivationToken.h"
#include "UiUtils/BackgroundRunning.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/SettingsStore.h"
#include "UiUtils/Theme.h"

#include "static_plugins.h"

int main(int argc, char **argv)
{
#ifdef IMPRESARIO_HAVE_WEBENGINE
    Gui::WebEngineMessageProfile::registerScheme();
#endif
    Common::registerMetaTypes();

    // Read before Qt clears the startup ID; a running instance needs it to come to the front for this launch
    const QString activationToken = UiUtils::ActivationToken::fromEnvironment(QProcessEnvironment::systemEnvironment());

    QApplication app(argc, argv);

    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);

    AppVersion::setGitVersion();
    AppVersion::setCoreApplicationData();
    KLocalizedString::setApplicationDomain("plasma-impresario");
    app.setApplicationDisplayName(QStringLiteral("Plasma Impresario"));

    app.setDesktopFileName(QStringLiteral("io.github.mikepadiernos.plasma_impresario"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/plasma-impresario.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Plasma Impresario %1 - fast Qt IMAP e-mail client").arg(Common::Application::version));
    parser.addHelpOption();
    QCommandLineOption versionOption(QStringList() << QLatin1String("v") << QLatin1String("version"),
                                     QObject::tr("Displays version information."));
    parser.addOption(versionOption);

    QCommandLineOption addressbookOption(QStringList() << QLatin1String("a") << QLatin1String("addressbook"),
                                         QObject::tr("Show addressbook window."));
    parser.addOption(addressbookOption);

    QCommandLineOption composeOption(QStringList() << QLatin1String("c") << QLatin1String("compose"),
                                     QObject::tr("Compose new e-mail (default when url is provided)."));
    parser.addOption(composeOption);

    QCommandLineOption logtodiskOption(QStringList() << QLatin1String("l") << QLatin1String("log-to-disk"),
                                       QObject::tr("Activate debug traffic logging to disk by default."));
    parser.addOption(logtodiskOption);

    QCommandLineOption mainwindowOption(QStringList() << QLatin1String("m") << QLatin1String("mainwindow"),
                                        QObject::tr("Show main window (default when no option is provided)."));
    parser.addOption(mainwindowOption);

    QCommandLineOption profileOption(QStringList() << QLatin1String("p") << QLatin1String("profile"),
                                     QObject::tr("Set profile."), QObject::tr("profile"));
    parser.addOption(profileOption);

    QCommandLineOption messageIdOption(QStringList() << QLatin1String("message-id"),
                                       QObject::tr("Open the message with this Message-ID."), QObject::tr("Message-ID"));
    parser.addOption(messageIdOption);

    QCommandLineOption backgroundOption(UiUtils::BackgroundRunning::backgroundOption(),
                                        QObject::tr("Start without showing the window, when running in the background "
                                                    "is turned on in the settings."));
    parser.addOption(backgroundOption);

    QCommandLineOption attachOption(QStringList() << QLatin1String("attach"),
                                    QObject::tr("Compose a new e-mail with this file attached; may be given more than once."),
                                    QObject::tr("file"));
    parser.addOption(attachOption);

    QCommandLineOption resetAppearanceOption(QStringList() << QLatin1String("reset-appearance"),
                                             QObject::tr("Return the appearance options to Plasma's defaults, keeping "
                                                         "theme profiles and account and folder colours."));
    parser.addOption(resetAppearanceOption);
    QCommandLineOption noWorkspaceAppearanceOption(QStringList() << QLatin1String("no-workspace-appearance"),
                                                   QObject::tr("Start without the appearance options of workspaces."));
    parser.addOption(noWorkspaceAppearanceOption);

    parser.addPositionalArgument(QLatin1String("url"),
                                 QObject::tr("Mailto: URL address for composing new e-mail, or mid: URL of a message to open."),
                                 QObject::tr("[url]"));

    parser.process(app);

    QTextStream qOut(stdout, QIODevice::WriteOnly);
    QTextStream qErr(stderr, QIODevice::WriteOnly);

    if (parser.isSet(versionOption)) {
        qOut << app.applicationDisplayName() << QLatin1Char(' ') << app.applicationVersion() << Qt::endl;
        return 0;
    }

    bool error = false;
    bool showMainWindow = false;
    bool showComposeWindow = false;
    bool showAddressbookWindow = false;
    QString profileName;
    QString url;
    QString messageId;

    showAddressbookWindow = parser.isSet(addressbookOption);
    showComposeWindow = parser.isSet(composeOption);
    showMainWindow = parser.isSet(mainwindowOption);
    const bool startInBackground = parser.isSet(backgroundOption);
    if (parser.isSet(profileOption))
        profileName = parser.value(profileOption);
    if (parser.isSet(messageIdOption))
        messageId = parser.value(messageIdOption);
    QStringList attachments;
    const QStringList attachValues = parser.values(attachOption);
    for (const QString &file : attachValues) {
        // A running copy has another working folder, so relative paths are resolved here
        const bool isUrl = file.startsWith(QLatin1String("file:")) || file.contains(QLatin1String("://"));
        attachments << (isUrl ? file : QFileInfo(file).absoluteFilePath());
    }
    if (!attachments.isEmpty())
        showComposeWindow = true;
    if (parser.positionalArguments().size() > 0) {
        if (parser.positionalArguments().size() == 1) {
            url = parser.positionalArguments().at(0);
            if (url.startsWith(QLatin1String("mid:"), Qt::CaseInsensitive) && messageId.isEmpty()) {
                messageId = url;
                url.clear();
            } else if (!url.startsWith(QLatin1String("mailto:"))) {
                qErr << QObject::tr("Unexpected argument '%1'.").arg(url) << Qt::endl;
                error = true;
            } else
                showComposeWindow = true;
        } else {
            qErr << QObject::tr("Unexpected arguments.") << Qt::endl;
            error = true;
        }
    }

    if (!showMainWindow && !showComposeWindow && !showAddressbookWindow && messageId.isEmpty() && !startInBackground)
        showMainWindow = true;

    if (error) {
        qOut << Qt::endl << parser.helpText();
        return error ? 1 : 0;
    }

    if (parser.isSet(resetAppearanceOption)) {
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        UiUtils::SettingsStore::setTheme(*config, UiUtils::Theme::reset(UiUtils::SettingsStore::theme(*config)));
        config->sync();
        qOut << QObject::tr("The appearance options are back to Plasma's defaults.") << Qt::endl;
    }
    if (parser.isSet(noWorkspaceAppearanceOption))
        Gui::AppearanceManager::instance()->setWorkspaceAppearanceDisabled(true);
    // Toolbar menu markers, dividers and the other appearance options are drawn by this style
    Gui::AppearanceStyle::ensureInstalled();

    // Hack: support multiple "profiles"
    if (!profileName.isEmpty()) {
        // We are abusing the env vars here. Yes, it's a hidden global. Yes, it's ugly.
        // Take it or leave it, this is a time-limited hack.
        // The env var is also in UTF-8. I like UTF-8.
        qputenv("TROJITA_PROFILE", profileName.toUtf8());
    } else {
#ifndef Q_OS_WIN32
        unsetenv("TROJITA_PROFILE");
#else
        putenv("TROJITA_PROFILE=");
#endif
    }

    if (IPC::Instance::isRunning()) {
        IPC::Instance::handOverActivationToken(activationToken);
        if (showMainWindow)
            IPC::Instance::showMainWindow();
        if (showAddressbookWindow)
            IPC::Instance::showAddressbookWindow();
        if (showComposeWindow)
            IPC::Instance::composeMail(url, attachments);
        if (!messageId.isEmpty())
            IPC::Instance::openMessage(messageId);
        return 0;
    }


    QSettings settings(Common::Application::organization,
                       profileName.isEmpty() ? Common::Application::name : Common::Application::name + QLatin1Char('-') + profileName);

    if (!Imap::Mailbox::SystemNetworkWatcher::init()) {
        qErr << "Failed to load network watcher.";
        return 1;
    }

    Gui::MainWindow win(&settings);

    QString errmsg;
    if (!IPC::registerInstance(&win, errmsg))
        qErr << QObject::tr("Error: Registering IPC instance failed: %1").arg(errmsg) << Qt::endl;

    // An autostart entry left from before running in the background was turned off shows the window as usual
    const bool hideWindow = startInBackground && !showMainWindow
            && UiUtils::BackgroundRunning::runsInBackground(*KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc")));
    if (hideWindow) {
        // The tray icon and notifications work without the window; opening the application again shows it
    } else if ( settings.value(Common::SettingsNames::guiStartMinimized, QVariant(false)).toBool() ) {
        if ( !settings.value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool() ) {
            win.show();
            win.setWindowState(Qt::WindowMinimized);
        }
    } else {
        win.show();
    }

    if (showAddressbookWindow)
        win.invokeContactEditor();

    if (showComposeWindow) {
        // Launched by the user or another application, such as the e-mail portal, which may hand over files
        win.composeMailFromOtherApplication(url.isEmpty() ? QUrl() : QUrl::fromEncoded(url.toUtf8()), attachments);
    }

    if (!messageId.isEmpty()) {
        // After the accounts have been set up by the event loop
        QTimer::singleShot(0, &win, [&win, messageId]() { win.openMessageById(messageId); });
    }

    if (parser.isSet(logtodiskOption)) {
        win.enableLoggingToDisk();
    }

    // When the Plasma session ends, unsent messages are kept as drafts and reopen at the next start. Plasma asks X11
    // clients to save their data through the session manager; on Wayland, the session's processes get SIGTERM.
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &win, [&win](QSessionManager &) {
        win.saveUnsentMessages();
    });
    KSignalHandler::self()->watchSignal(SIGTERM);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, &win, [&win](int signal) {
        if (signal != SIGTERM)
            return;
        win.saveUnsentMessages();
        QCoreApplication::quit();
    });

    return app.exec();
}
