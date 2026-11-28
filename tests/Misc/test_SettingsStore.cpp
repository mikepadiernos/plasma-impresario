/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <KConfigGroup>
#include <KSharedConfig>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "Common/SettingsNames.h"
#include "UiUtils/SettingsStore.h"

using namespace UiUtils;
using SettingsSchema::Layer;

class SettingsStoreTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_files = 0;

    QString freshPath()
    {
        return m_dir.filePath(QStringLiteral("config-%1").arg(++m_files));
    }

    static KSharedConfigPtr open(const QString &path)
    {
        return KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void applicationValuesPersist()
    {
        const QString path = freshPath();
        {
            auto config = open(path);
            QCOMPARE(SettingsStore::application(*config, nullptr, QStringLiteral("Tabs/Enabled")), QVariant());
            SettingsStore::setApplication(*config, nullptr, QStringLiteral("Tabs/Enabled"), true);
            SettingsStore::setApplication(*config, nullptr, QStringLiteral("Tabs/Region"), QStringLiteral("left"));
            SettingsStore::setApplication(*config, nullptr, QStringLiteral("Search/HistoryLimit"), QStringLiteral("35"));
            SettingsStore::setApplication(*config, nullptr, QStringLiteral("Unknown/Key"), 1);
        }
        KConfig reread(path, KConfig::SimpleConfig);
        QCOMPARE(KConfigGroup(&reread, QStringLiteral("Tabs")).readEntry("Enabled", false), true);
        QCOMPARE(SettingsStore::value(reread, nullptr, QStringLiteral("Tabs/Region")), QVariant(QStringLiteral("left")));
        QCOMPARE(SettingsStore::value(reread, nullptr, QStringLiteral("Search/HistoryLimit")), QVariant(35));
        QVERIFY(!reread.hasGroup(QStringLiteral("Unknown")));
        // Unset values fall back to the defaults
        QCOMPARE(SettingsStore::value(reread, nullptr, QStringLiteral("Keyboard/VimMode")), QVariant(false));
        auto config = open(path);
        SettingsStore::setApplication(*config, nullptr, QStringLiteral("Tabs/Enabled"), QVariant());
        QCOMPARE(SettingsStore::application(*config, nullptr, QStringLiteral("Tabs/Enabled")), QVariant());
    }

    void legacyValuesStayInTheProfileSettings()
    {
        auto config = open(freshPath());
        QSettings legacy(m_dir.filePath(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        legacy.setValue(Common::SettingsNames::autoMarkReadEnabled, false);
        QCOMPARE(SettingsStore::value(*config, &legacy, QStringLiteral("Reading/AutoMarkRead")), QVariant(false));
        SettingsStore::setApplication(*config, &legacy, QStringLiteral("Reading/MarkReadDelay"), 4);
        QCOMPARE(legacy.value(Common::SettingsNames::autoMarkReadSeconds).toInt(), 4);
        QVERIFY(!config->hasGroup(QStringLiteral("Reading")));
        SettingsStore::setApplication(*config, &legacy, QStringLiteral("Reading/MarkReadDelay"), QVariant());
        QVERIFY(!legacy.contains(Common::SettingsNames::autoMarkReadSeconds));
    }

    void accountsAreIsolated()
    {
        const QString path = freshPath();
        const QString key = QStringLiteral("Reading/PreferPlainText");
        {
            auto config = open(path);
            SettingsStore::setAccountValue(*config, QStringLiteral("work"), key, false);
            SettingsStore::setAccountValue(*config, QStringLiteral("work"), QStringLiteral("Shell/StartFolder"),
                                           QStringLiteral("Projects"));
            SettingsStore::setFolderValue(*config, QStringLiteral("work"), QStringLiteral("Lists/KDE"), key, true);
            SettingsStore::setFolderValue(*config, QStringLiteral("home"), QStringLiteral("INBOX"),
                                          QStringLiteral("Reading/MarkReadDelay"), 9);
            // Settings which cannot be set for an account or a folder are not stored there
            SettingsStore::setAccountValue(*config, QStringLiteral("work"), QStringLiteral("Keyboard/VimMode"), true);
            SettingsStore::setFolderValue(*config, QStringLiteral("work"), QStringLiteral("INBOX"),
                                          QStringLiteral("Shell/StartFolder"), QStringLiteral("x"));
        }
        auto config = open(path);
        QCOMPARE(SettingsStore::value(*config, nullptr, key, QStringLiteral("work"), QStringLiteral("INBOX")), QVariant(false));
        QCOMPARE(SettingsStore::value(*config, nullptr, key, QStringLiteral("work"), QStringLiteral("Lists/KDE")),
                 QVariant(true));
        QCOMPARE(SettingsStore::value(*config, nullptr, key, QStringLiteral("home"), QStringLiteral("Lists/KDE")),
                 QVariant(true));
        QCOMPARE(SettingsStore::value(*config, nullptr, QStringLiteral("Reading/MarkReadDelay"), QStringLiteral("work"),
                                      QStringLiteral("INBOX")),
                 QVariant(0));
        QCOMPARE(SettingsStore::value(*config, nullptr, QStringLiteral("Reading/MarkReadDelay"), QStringLiteral("home"),
                                      QStringLiteral("INBOX")),
                 QVariant(9));
        QCOMPARE(SettingsStore::accountValues(*config, QStringLiteral("work")).size(), 2);
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("work")), QStringList{QStringLiteral("Lists/KDE")});
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("home")), QStringList{QStringLiteral("INBOX")});
        const auto layers = SettingsStore::layers(*config, nullptr, QStringLiteral("work"), QStringLiteral("Lists/KDE"));
        QCOMPARE(SettingsSchema::resolve(key, layers).layer, Layer::Folder);
        QCOMPARE(SettingsSchema::inherited(key, layers, Layer::Folder), (SettingsSchema::Resolved{false, Layer::Account}));

        SettingsStore::removeAccount(*config, QStringLiteral("work"));
        QCOMPARE(SettingsStore::accountValues(*config, QStringLiteral("work")), SettingsSchema::Values());
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("work")), QStringList());
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("home")), QStringList{QStringLiteral("INBOX")});
    }

    void clearingTheLastFolderValueForgetsTheFolder()
    {
        auto config = open(freshPath());
        const QString key = QStringLiteral("Reading/AutoMarkRead");
        SettingsStore::setFolderValue(*config, QStringLiteral("a"), QStringLiteral("F"), key, false);
        SettingsStore::setFolderValue(*config, QStringLiteral("a"), QStringLiteral("F"), key, QVariant());
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("a")), QStringList());
        // Removing what was never set creates nothing
        SettingsStore::setFolderValue(*config, QStringLiteral("a"), QStringLiteral("G"), key, QVariant());
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("a")), QStringList());
    }

    void foldersWithAnyName()
    {
        auto config = open(freshPath());
        const QString odd = QStringLiteral("Weird]/[name=x");
        SettingsStore::setFolderValue(*config, QStringLiteral("a"), odd, QStringLiteral("Reading/AutoMarkRead"), false);
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("a")), QStringList{odd});
        QCOMPARE(SettingsStore::value(*config, nullptr, QStringLiteral("Reading/AutoMarkRead"), QStringLiteral("a"), odd),
                 QVariant(false));
    }

    void renamingFolders()
    {
        auto config = open(freshPath());
        const QString key = QStringLiteral("Reading/AutoMarkRead");
        for (const QString &folder : {QStringLiteral("Lists"), QStringLiteral("Lists/KDE"), QStringLiteral("Listsx")})
            SettingsStore::setFolderValue(*config, QStringLiteral("a"), folder, key, false);
        SettingsStore::renameFolder(*config, QStringLiteral("a"), QStringLiteral("Lists"), QStringLiteral("Mail"),
                                    QLatin1Char('/'));
        QCOMPARE(SettingsStore::folders(*config, QStringLiteral("a")),
                 (QStringList{QStringLiteral("Listsx"), QStringLiteral("Mail"), QStringLiteral("Mail/KDE")}));
    }

    void workspacesPersist()
    {
        const QString path = freshPath();
        const QList<Workspaces::Workspace> list = {
            {QStringLiteral("w2"), QStringLiteral("Work"), {QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("act"),
             QStringLiteral("/notes")},
            {QStringLiteral("w1"), QStringLiteral("Home"), {}, QString(), QString()},
        };
        SettingsStore::setWorkspaces(*open(path), list);
        QCOMPARE(SettingsStore::workspaces(KConfig(path, KConfig::SimpleConfig)), list);
        SettingsStore::setWorkspaces(*open(path), {list.at(1)});
        QCOMPARE(SettingsStore::workspaces(*open(path)), QList<Workspaces::Workspace>{list.at(1)});
    }

    void nextcloudAccountsPersist()
    {
        const QString path = freshPath();
        Nextcloud::Account work;
        work.id = QStringLiteral("nc2");
        work.server = QUrl(QStringLiteral("https://cloud.example.com/nextcloud"));
        work.loginName = QStringLiteral("alice@example.com");
        work.userId = QStringLiteral("alice");
        work.displayName = QStringLiteral("Alice Example");
        Nextcloud::Account home = work;
        home.id = QStringLiteral("nc1");
        home.server = QUrl(QStringLiteral("https://home.example.net"));
        Nextcloud::Account online = work;
        online.id = QStringLiteral("kaccounts-7");
        online.source = Nextcloud::Account::Source::OnlineAccounts;
        online.onlineAccountId = 7;
        SettingsStore::setNextcloudAccounts(*open(path), {work, home, online});
        // Accounts from KDE Online Accounts are found again, not stored
        QCOMPARE(SettingsStore::nextcloudAccounts(KConfig(path, KConfig::SimpleConfig)),
                 (QList<Nextcloud::Account>{work, home}));
        KConfig raw(path, KConfig::SimpleConfig);
        const QStringList keys = KConfigGroup(&raw, QStringLiteral("Nextcloud")).group(QStringLiteral("nc2")).keyList();
        for (const QString &key : keys)
            QVERIFY2(!key.contains(QLatin1String("assword")), qPrintable(key));

        {
            KConfig edit(path, KConfig::SimpleConfig);
            KConfigGroup(&edit, QStringLiteral("Nextcloud"))
                .group(QStringLiteral("nc1"))
                .writeEntry("Server", QStringLiteral("http://home.example.net"));
        }
        QCOMPARE(SettingsStore::nextcloudAccounts(KConfig(path, KConfig::SimpleConfig)),
                 QList<Nextcloud::Account>{work});

        SettingsStore::setNextcloudAccounts(*open(path), {});
        QVERIFY(SettingsStore::nextcloudAccounts(*open(path)).isEmpty());
    }

    void themePersists()
    {
        const QString path = freshPath();
        {
            auto config = open(path);
            Theme::Values values;
            values.insert(QStringLiteral("WindowOpacity"), 80);
            values.insert(QStringLiteral("StripeRows"), false);
            values.insert(QStringLiteral("MessageListStyle"), QStringLiteral("Cards"));
            values.insert(QStringLiteral("PaneTintColor"), QColor(QStringLiteral("#123456")));
            values.insert(QStringLiteral("TreeLinePalette"), QStringList{QStringLiteral("#ff0000")});
            // Defaults and invalid values are not stored
            values.insert(QStringLiteral("PaneOpacity"), 100);
            values.insert(QStringLiteral("CornerRadius"), QStringLiteral("Huge"));
            values.insert(QStringLiteral("Unknown"), 1);
            SettingsStore::setTheme(*config, values);
        }
        KConfig reread(path, KConfig::SimpleConfig);
        const KConfigGroup group(&reread, QLatin1String(Theme::Group));
        QCOMPARE(group.readEntry("PaneTintColor", QString()), QStringLiteral("#123456"));
        QVERIFY(!group.hasKey("PaneOpacity"));
        QVERIFY(!group.hasKey("CornerRadius"));
        QVERIFY(!group.hasKey("Unknown"));
        const Theme::Values stored = SettingsStore::theme(reread);
        QCOMPARE(stored.size(), 5);
        QCOMPARE(stored.value(QStringLiteral("WindowOpacity")).toInt(), 80);
        QCOMPARE(stored.value(QStringLiteral("MessageListStyle")).toString(), QStringLiteral("Cards"));
        QCOMPARE(stored.value(QStringLiteral("PaneTintColor")).value<QColor>(), QColor(QStringLiteral("#123456")));

        // KConfig's own colour form is read too, and values edited by hand are checked
        auto config = open(path);
        KConfigGroup edited = config->group(QLatin1String(Theme::Group));
        edited.writeEntry("PaneTintColor", QStringLiteral("18,52,86"));
        edited.writeEntry("WindowOpacity", 5);
        const Theme::Values reloaded = SettingsStore::theme(*config);
        QCOMPARE(reloaded.value(QStringLiteral("PaneTintColor")).value<QColor>(), QColor(18, 52, 86));
        QVERIFY(!reloaded.contains(QStringLiteral("WindowOpacity")));
    }

    void themeProfilesPersist()
    {
        auto config = open(freshPath());
        QList<ThemeProfiles::Profile> profiles{
            {QStringLiteral("p1"), QStringLiteral("Night"), {{QStringLiteral("WindowOpacity"), 90}}},
            {QStringLiteral("p2"), QStringLiteral("Day"), {}},
        };
        SettingsStore::setThemeProfiles(*config, profiles);
        QStringList problems;
        const QList<ThemeProfiles::Profile> restored = SettingsStore::themeProfiles(*config, &problems);
        QCOMPARE(problems, QStringList());
        QCOMPARE(restored.size(), 2);
        QCOMPARE(restored.at(0).name, QStringLiteral("Night"));
        QCOMPARE(restored.at(0).values.value(QStringLiteral("WindowOpacity")).toInt(), 90);
        SettingsStore::setThemeProfiles(*config, {});
        QVERIFY(SettingsStore::themeProfiles(*config).isEmpty());

        QCOMPARE(SettingsStore::followedTheme(*config), QString());
        SettingsStore::setFollowedTheme(*config, QStringLiteral("/tmp/opus.json"));
        QCOMPARE(SettingsStore::followedTheme(*config), QStringLiteral("/tmp/opus.json"));
        SettingsStore::setFollowedTheme(*config, QString());
        QCOMPARE(SettingsStore::followedTheme(*config), QString());
    }

    void colorsFollowAccountsAndFolders()
    {
        auto config = open(freshPath());
        SettingsStore::setAccountColor(*config, QStringLiteral("work"), QColor(Qt::red));
        SettingsStore::setAccountColor(*config, QStringLiteral("home"), QColor(Qt::blue));
        const QString odd = QStringLiteral("Weird]/[name=x");
        for (const QString &folder : {QStringLiteral("Lists"), QStringLiteral("Lists/KDE"), odd})
            SettingsStore::setFolderColor(*config, QStringLiteral("work"), folder, QColor(Qt::green));
        SettingsStore::setFolderColor(*config, QStringLiteral("home"), QStringLiteral("INBOX"), QColor(Qt::yellow));

        FolderColors::Colors colors = SettingsStore::folderColors(*config);
        QCOMPARE(colors.accounts.value(QStringLiteral("work")), QColor(Qt::red));
        QCOMPARE(colors.folders.value(QStringLiteral("work")).size(), 3);
        QCOMPARE(colors.folders.value(QStringLiteral("work")).value(odd), QColor(Qt::green));

        SettingsStore::renameFolder(*config, QStringLiteral("work"), QStringLiteral("Lists"), QStringLiteral("Mail"),
                                    QLatin1Char('/'));
        colors = SettingsStore::folderColors(*config);
        QVERIFY(colors.folders.value(QStringLiteral("work")).contains(QStringLiteral("Mail/KDE")));
        QVERIFY(!colors.folders.value(QStringLiteral("work")).contains(QStringLiteral("Lists")));

        // An invalid colour removes the folder's colour
        SettingsStore::setFolderColor(*config, QStringLiteral("work"), odd, QColor());
        QCOMPARE(SettingsStore::folderColors(*config).folders.value(QStringLiteral("work")).size(), 2);

        SettingsStore::removeAccount(*config, QStringLiteral("work"));
        colors = SettingsStore::folderColors(*config);
        QVERIFY(!colors.accounts.contains(QStringLiteral("work")));
        QVERIFY(!colors.folders.contains(QStringLiteral("work")));
        QCOMPARE(colors.accounts.value(QStringLiteral("home")), QColor(Qt::blue));
        QCOMPARE(colors.folders.value(QStringLiteral("home")).value(QStringLiteral("INBOX")), QColor(Qt::yellow));
    }

    void workspaceAppearancePersists()
    {
        const QString path = freshPath();
        Workspaces::Workspace workspace{QStringLiteral("w1"), QStringLiteral("Work"), {}, QString(), QString()};
        workspace.appearance = Workspaces::Workspace::Appearance::Selected;
        workspace.themeProfile = QStringLiteral("p1");
        workspace.selectedAppearance = {QStringLiteral("MessageListStyle")};
        workspace.appearanceOverrides.insert(QStringLiteral("MessageListStyle"), QStringLiteral("Cards"));
        SettingsStore::setWorkspaces(*open(path), {workspace});
        const QList<Workspaces::Workspace> restored = SettingsStore::workspaces(KConfig(path, KConfig::SimpleConfig));
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.at(0).appearance, Workspaces::Workspace::Appearance::Selected);
        QCOMPARE(restored.at(0).themeProfile, QStringLiteral("p1"));
        QCOMPARE(restored.at(0).selectedAppearance, QStringList{QStringLiteral("MessageListStyle")});
        QCOMPARE(restored.at(0).appearanceOverrides.value(QStringLiteral("MessageListStyle")).toString(),
                 QStringLiteral("Cards"));
    }

    void keymapImportCanBeUndone()
    {
        auto config = open(freshPath());
        Keymap::Bindings custom = Keymap::defaultBindings();
        custom.insert(QStringLiteral("message.reply"), QStringLiteral("q"));
        SettingsStore::setBindings(*config, custom);
        SettingsStore::setFollowedProfile(*config, QStringLiteral("/tmp/opus.json"));
        QCOMPARE(SettingsStore::bindings(*config), custom);
        QVERIFY(!SettingsStore::canRestoreKeymap(*config));

        Keymap::Profile incoming;
        incoming.vimMode = true;
        incoming.timeout = 2000;
        incoming.bindings.insert(QStringLiteral("move.down"), QStringLiteral("J"));
        SettingsStore::importKeymap(*config, incoming);
        Keymap::Profile now = SettingsStore::keymapProfile(*config);
        QVERIFY(now.vimMode);
        QCOMPARE(now.timeout, 2000);
        QCOMPARE(now.bindings.value(QStringLiteral("move.down")), QStringLiteral("J"));
        QCOMPARE(now.bindings.value(QStringLiteral("message.reply")), QStringLiteral("q"));
        QCOMPARE(SettingsStore::followedProfile(*config), QStringLiteral("/tmp/opus.json"));
        QVERIFY(SettingsStore::canRestoreKeymap(*config));

        QVERIFY(SettingsStore::restoreKeymap(*config));
        now = SettingsStore::keymapProfile(*config);
        QVERIFY(!now.vimMode);
        QCOMPARE(now.timeout, 1000);
        QCOMPARE(now.bindings, custom);
        QVERIFY(!SettingsStore::canRestoreKeymap(*config));
        QVERIFY(!SettingsStore::restoreKeymap(*config));
        SettingsStore::setFollowedProfile(*config, QString());
        QCOMPARE(SettingsStore::followedProfile(*config), QString());
    }

    void tabsPersistInTheState()
    {
        const QString path = freshPath();
        MailTabs::Session session;
        session.tabs = {{QStringLiteral("a"), QStringLiteral("INBOX"), 0, 0, QString()},
                        {QStringLiteral("a"), QStringLiteral("INBOX"), 5, 6, QStringLiteral("Hi")}};
        session.current = 1;
        SettingsStore::setTabs(*open(path), session);
        QCOMPARE(SettingsStore::tabs(*open(path)), session);
        QCOMPARE(SettingsStore::tabs(*open(freshPath())), MailTabs::Session());
    }

    void searchReset()
    {
        auto config = open(freshPath());
        SettingsStore::setApplication(*config, nullptr, QStringLiteral("Search/UseIndex"), QStringLiteral("server"));
        KConfigGroup(config, QStringLiteral("QuickSearch")).writeEntry("RememberSearches", false);
        SettingsStore::resetSearch(*config);
        QCOMPARE(SettingsStore::value(*config, nullptr, QStringLiteral("Search/UseIndex")), QVariant(QStringLiteral("automatic")));
        QVERIFY(!KConfigGroup(config, QStringLiteral("QuickSearch")).hasKey("RememberSearches"));
    }

    void unavailableIntegrationsKeepTheirValues()
    {
        // A setting whose integration is missing keeps what the user chose, for when the integration returns
        auto config = open(freshPath());
        const QString key = QStringLiteral("Integrations/MailToCalendar");
        SettingsStore::setApplication(*config, nullptr, key, false);
        const SettingsSchema::Definition *def = SettingsSchema::definition(key);
        QVERIFY(!SettingsSchema::isAvailable(*def, SettingsSchema::Feature::Notes));
        QCOMPARE(SettingsStore::value(*config, nullptr, key), QVariant(false));
    }
};

QTEST_GUILESS_MAIN(SettingsStoreTest)
#include "test_SettingsStore.moc"
