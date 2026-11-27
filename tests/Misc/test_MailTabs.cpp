/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QTest>

#include "UiUtils/MailTabs.h"
#include "UiUtils/Workspaces.h"

using namespace UiUtils;
using MailTabs::Session;
using MailTabs::Tab;

class MailTabsTest : public QObject
{
    Q_OBJECT

private:
    static Tab folder(const QString &account, const QString &mailbox)
    {
        return Tab{account, mailbox, 0, 0, QString()};
    }

private slots:
    void labels()
    {
        QCOMPARE(MailTabs::label(folder(QStringLiteral("a"), QStringLiteral("Lists/KDE"))), QStringLiteral("KDE"));
        QCOMPARE(MailTabs::label(folder(QStringLiteral("a"), QStringLiteral("Lists.KDE")), QLatin1Char('.')),
                 QStringLiteral("KDE"));
        const Tab message{QStringLiteral("a"), QStringLiteral("INBOX"), 7, 42, QStringLiteral("  Hello\n there ")};
        QCOMPARE(MailTabs::label(message), QStringLiteral("Hello there"));
        QVERIFY(MailTabs::toolTip(message, QStringLiteral("Work")).contains(QLatin1String("Work, INBOX")));
    }

    void regions()
    {
        QCOMPARE(MailTabs::regions().size(), 5);
        for (MailTabs::Region region : MailTabs::regions())
            QCOMPARE(MailTabs::regionFromConfigName(MailTabs::configName(region)), region);
        QCOMPARE(MailTabs::regionFromConfigName(QStringLiteral("ceiling")), MailTabs::Region::Header);
    }

    void serialization()
    {
        Session session;
        session.tabs = {folder(QStringLiteral("a"), QStringLiteral("INBOX")),
                        Tab{QStringLiteral("b"), QStringLiteral("Lists/KDE"), 3, 9, QStringLiteral("Re: patch")}};
        session.current = 1;
        QCOMPARE(MailTabs::deserialize(MailTabs::serialize(session)), std::optional<Session>(session));
        QCOMPARE(MailTabs::deserialize("garbage"), std::nullopt);
        QCOMPARE(MailTabs::deserialize(R"({"version":2,"tabs":[]})"), std::nullopt);
        const auto clamped = MailTabs::deserialize(R"({"version":1,"current":9,"tabs":[{"account":"a","mailbox":"X"},
                                                       {"account":"","mailbox":"Y"}]})");
        QVERIFY(clamped);
        QCOMPARE(clamped->tabs.size(), 1);
        QCOMPARE(clamped->current, 0);
    }

    void unavailableAccountsAreDropped()
    {
        Session session;
        session.tabs = {folder(QStringLiteral("a"), QStringLiteral("1")), folder(QStringLiteral("gone"), QStringLiteral("2")),
                        folder(QStringLiteral("b"), QStringLiteral("3"))};
        session.current = 2;
        const Session restored = MailTabs::restorable(session, {QStringLiteral("a"), QStringLiteral("b")});
        QCOMPARE(restored.tabs.size(), 2);
        QCOMPARE(restored.current, 1);
        QCOMPARE(restored.tabs.at(restored.current).mailbox, QStringLiteral("3"));
        // The shown tab was of the missing account: its left neighbour is shown
        session.current = 1;
        QCOMPARE(MailTabs::restorable(session, {QStringLiteral("a"), QStringLiteral("b")}).current, 0);
        QCOMPARE(MailTabs::restorable(session, {}), Session());
        QCOMPARE(MailTabs::withoutAccount(session, QStringLiteral("a")).tabs.size(), 2);
    }

    void renamedFolders()
    {
        Session session;
        session.tabs = {folder(QStringLiteral("a"), QStringLiteral("Lists")), folder(QStringLiteral("a"), QStringLiteral("Lists/KDE")),
                        folder(QStringLiteral("a"), QStringLiteral("Listsx")), folder(QStringLiteral("b"), QStringLiteral("Lists"))};
        const Session renamed = MailTabs::withRenamedFolder(session, QStringLiteral("a"), QStringLiteral("Lists"),
                                                            QStringLiteral("Mail"), QLatin1Char('/'));
        QCOMPARE(renamed.tabs.at(0).mailbox, QStringLiteral("Mail"));
        QCOMPARE(renamed.tabs.at(1).mailbox, QStringLiteral("Mail/KDE"));
        QCOMPARE(renamed.tabs.at(2).mailbox, QStringLiteral("Listsx"));
        QCOMPARE(renamed.tabs.at(3).mailbox, QStringLiteral("Lists"));
    }

    void closing()
    {
        QCOMPARE(MailTabs::currentAfterClose(1, 0, 0), -1);
        QCOMPARE(MailTabs::currentAfterClose(3, 0, 2), 1);
        QCOMPARE(MailTabs::currentAfterClose(3, 2, 0), 0);
        QCOMPARE(MailTabs::currentAfterClose(3, 1, 1), 1);
        QCOMPARE(MailTabs::currentAfterClose(3, 2, 2), 1);
    }

    void workspaces()
    {
        using Workspaces::Workspace;
        const QList<Workspace> list = {
            Workspace{QStringLiteral("w1"), QStringLiteral("Work"), {QStringLiteral("gone"), QStringLiteral("work")},
                      QStringLiteral("act-work"), QStringLiteral("/notes/work")},
            Workspace{QStringLiteral("w2"), QStringLiteral("Home"), {QStringLiteral("home")}, QString(), QString()},
        };
        const QStringList accounts = {QStringLiteral("work"), QStringLiteral("home")};
        QCOMPARE(Workspaces::forActivity(list, QStringLiteral("act-work")), &list.at(0));
        QCOMPARE(Workspaces::forActivity(list, QString()), nullptr);
        QCOMPARE(Workspaces::accountForActivity(list, QStringLiteral("act-work"), QStringLiteral("home"), accounts),
                 QStringLiteral("work"));
        QCOMPARE(Workspaces::accountForActivity(list, QStringLiteral("act-work"), QStringLiteral("work"), accounts), QString());
        QCOMPARE(Workspaces::accountForActivity(list, QStringLiteral("other"), QStringLiteral("home"), accounts), QString());
        QCOMPARE(Workspaces::notesFolder(list, QStringLiteral("act-work"), QStringLiteral("/n")), QStringLiteral("/notes/work"));
        QCOMPARE(Workspaces::notesFolder(list, QStringLiteral("other"), QStringLiteral("/n")), QStringLiteral("/n"));
        QCOMPARE(Workspaces::withoutAccount(list, QStringLiteral("work")).at(0).accounts, QStringList{QStringLiteral("gone")});
        QCOMPARE(Workspaces::newId(list), QStringLiteral("workspace-1"));
        QCOMPARE(Workspaces::problems(list.at(0), accounts, {QStringLiteral("act-work")}, true).size(), 1);
        QCOMPARE(Workspaces::problems(list.at(0), accounts, {}, false).size(), 2);
        QCOMPARE(Workspaces::problems(list.at(1), accounts, {}, false), QStringList());
    }
};

QTEST_GUILESS_MAIN(MailTabsTest)
#include "test_MailTabs.moc"
