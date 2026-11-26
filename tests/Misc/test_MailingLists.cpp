/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_MailingLists.h"

#include <QStandardItemModel>
#include <QtTest>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/ListHeaders.h"
#include "UiUtils/MailingLists.h"

using UiUtils::MailingList;
using namespace UiUtils::MailingLists;

namespace {

QStandardItem *message(uint uid, const QVariantMap &listHeaders, const QString &listPost = QString(),
                       bool fetched = true)
{
    auto *item = new QStandardItem(QString::number(uid));
    item->setData(uid, Imap::Mailbox::RoleMessageUid);
    item->setData(fetched, Imap::Mailbox::RoleIsFetched);
    item->setData(listHeaders, Imap::Mailbox::RoleMessageHeaderList);
    QVariantList post;
    if (!listPost.isEmpty())
        post << QUrl(listPost);
    item->setData(post, Imap::Mailbox::RoleMessageHeaderListPost);
    item->setData(false, Imap::Mailbox::RoleMessageHeaderListPostNo);
    return item;
}

QVariantMap listId(const QString &value)
{
    return {{QStringLiteral("list-id"), value}};
}

}

void MailingListsTest::readsListHeaders()
{
    const QByteArray raw = "list-id: Developers\r\n"
                           "\t<dev.lists.example.org>\r\n"
                           "References: <a@example.org>\r\n"
                           "LIST-UNSUBSCRIBE: <mailto:dev-leave@lists.example.org>,\r\n"
                           "  <https://lists.example.org/leave>\r\n"
                           "List-Id: <second.example.org>\r\n"
                           "List-Archive:<https://lists.example.org/archive>\r\n"
                           "\r\n";
    const auto headers = Imap::LowLevelParser::listHeaders(raw);
    QCOMPARE(headers.value("list-id"), QByteArray("Developers <dev.lists.example.org>"));
    QCOMPARE(headers.value("list-unsubscribe"),
             QByteArray("<mailto:dev-leave@lists.example.org>, <https://lists.example.org/leave>"));
    QCOMPARE(headers.value("list-archive"), QByteArray("<https://lists.example.org/archive>"));
    // References has its own parser, and the second List-Id is ignored
    QCOMPARE(headers.keys(), (QList<QByteArray>{"list-archive", "list-id", "list-unsubscribe"}));
    QVERIFY(Imap::LowLevelParser::listHeaders(QByteArray()).isEmpty());
    QVERIFY(Imap::LowLevelParser::listHeaders(" folded without a header\r\n: no name\r\n").isEmpty());
}

void MailingListsTest::parsesListId_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("name");
    QTest::newRow("name and ID") << QStringLiteral("Developers <Dev.Lists.Example.ORG>")
                                 << QStringLiteral("dev.lists.example.org") << QStringLiteral("Developers");
    QTest::newRow("ID only") << QStringLiteral("<dev.lists.example.org>") << QStringLiteral("dev.lists.example.org")
                             << QString();
    QTest::newRow("quoted name") << QStringLiteral("\"Dev, \\\"core\\\" team\" <dev.example.org>")
                                 << QStringLiteral("dev.example.org") << QStringLiteral("Dev, \"core\" team");
    QTest::newRow("encoded name") << QStringLiteral("=?UTF-8?Q?Entwickler=C3=BCbersicht?= <dev.example.org>")
                                  << QStringLiteral("dev.example.org") << QStringLiteral("Entwicklerübersicht");
    QTest::newRow("spaces inside brackets") << QStringLiteral("Dev < dev.example.org >")
                                            << QStringLiteral("dev.example.org") << QStringLiteral("Dev");
    QTest::newRow("without brackets") << QStringLiteral("dev.example.org") << QStringLiteral("dev.example.org")
                                      << QString();
    QTest::newRow("words without brackets") << QStringLiteral("not an id") << QString() << QString();
    QTest::newRow("empty") << QString() << QString() << QString();
}

void MailingListsTest::parsesListId()
{
    QFETCH(QString, value);
    QFETCH(QString, id);
    QFETCH(QString, name);
    const MailingList list = parseListId(value);
    QCOMPARE(list.id, id);
    QCOMPARE(list.name, name);
    QVERIFY(list.address.isEmpty());
}

void MailingListsTest::readsHeaderUrls()
{
    QCOMPARE(headerUrls(QStringLiteral("<mailto:leave@example.org?subject=unsubscribe> (Use this), "
                                       "<https://example.org/ leave>")),
             (QList<QUrl>{QUrl(QStringLiteral("mailto:leave@example.org?subject=unsubscribe")),
                          QUrl(QStringLiteral("https://example.org/leave"))}));
    // Angle brackets in comments are no URL, and neither is text without a scheme
    QCOMPARE(headerUrls(QStringLiteral("(see <https://comment.example.org>) <ftp://example.org/x>, <nothing>")),
             (QList<QUrl>{QUrl(QStringLiteral("ftp://example.org/x"))}));
    QVERIFY(headerUrls(QStringLiteral("NO")).isEmpty());
    QVERIFY(headerUrls(QString()).isEmpty());
}

void MailingListsTest::picksUsableUrls()
{
    // The list's order of preference is kept, skipping what cannot be followed
    QCOMPARE(usableUrl({QUrl(QStringLiteral("ftp://example.org/x")), QUrl(QStringLiteral("mailto:")),
                        QUrl(QStringLiteral("https://example.org/help")), QUrl(QStringLiteral("mailto:a@example.org"))}),
             QUrl(QStringLiteral("https://example.org/help")));
    QCOMPARE(usableUrl({QUrl(QStringLiteral("mailto:a@example.org")), QUrl(QStringLiteral("https://example.org"))}),
             QUrl(QStringLiteral("mailto:a@example.org")));
    QCOMPARE(usableUrl({QUrl(QStringLiteral("http://example.org/a"))}), QUrl(QStringLiteral("http://example.org/a")));
    QVERIFY(!usableUrl({QUrl(QStringLiteral("javascript:alert(1)")), QUrl(QStringLiteral("https:///path"))}).isValid());
    QVERIFY(!usableUrl({}).isValid());
}

void MailingListsTest::recognizesListsOfMessages()
{
    QStandardItemModel model;
    model.appendRow(message(1, listId(QStringLiteral("Developers <dev.lists.example.org>")),
                            QStringLiteral("mailto:Dev@Lists.Example.org")));
    model.appendRow(message(2, {}, QStringLiteral("mailto:announce@example.org")));
    model.appendRow(message(3, {}));
    model.appendRow(message(4, listId(QStringLiteral("<dev.lists.example.org>")), QString(), false));

    const MailingList dev = fromMessage(model.index(0, 0));
    QCOMPARE(dev, (MailingList{QStringLiteral("dev.lists.example.org"), QStringLiteral("Developers"),
                               QStringLiteral("dev@lists.example.org")}));
    QCOMPARE(dev.searchValue(), QStringLiteral("dev.lists.example.org"));
    QCOMPARE(dev.displayName(), QStringLiteral("Developers"));

    // Lists without a List-Id are known by their posting address
    const MailingList announce = fromMessage(model.index(1, 0));
    QVERIFY(announce.isValid());
    QCOMPARE(announce.searchValue(), QStringLiteral("announce@example.org"));
    QCOMPARE(announce.displayName(), QStringLiteral("announce@example.org"));

    QVERIFY(!fromMessage(model.index(2, 0)).isValid());
    // Headers of messages which are not loaded are not asked for
    QVERIFY(!fromMessage(model.index(3, 0)).isValid());
    QVERIFY(!fromMessage(QModelIndex()).isValid());
}

void MailingListsTest::offersListActions()
{
    QStandardItemModel model;
    QVariantMap headers = listId(QStringLiteral("<dev.lists.example.org>"));
    headers.insert(QStringLiteral("list-unsubscribe"),
                   QStringLiteral("<https://lists.example.org/leave>, <mailto:dev-leave@lists.example.org>"));
    headers.insert(QStringLiteral("list-subscribe"), QStringLiteral("<mailto:dev-join@lists.example.org>"));
    headers.insert(QStringLiteral("list-help"), QStringLiteral("<ftp://lists.example.org/help>"));
    headers.insert(QStringLiteral("list-archive"), QStringLiteral("<https://lists.example.org/archive/dev>"));
    headers.insert(QStringLiteral("list-owner"), QStringLiteral("<mailto:dev-owner@lists.example.org> (Ann)"));
    model.appendRow(message(1, headers, QStringLiteral("mailto:dev@lists.example.org")));

    const QList<ActionTarget> expected{
        {Action::Post, QUrl(QStringLiteral("mailto:dev@lists.example.org"))},
        {Action::Subscribe, QUrl(QStringLiteral("mailto:dev-join@lists.example.org"))},
        {Action::Unsubscribe, QUrl(QStringLiteral("https://lists.example.org/leave"))},
        // Help is left out: FTP cannot be followed
        {Action::Archive, QUrl(QStringLiteral("https://lists.example.org/archive/dev"))},
        {Action::Owner, QUrl(QStringLiteral("mailto:dev-owner@lists.example.org"))},
    };
    QCOMPARE(actions(model.index(0, 0)), expected);

    // A list which takes no posts offers no Post action
    model.item(0)->setData(true, Imap::Mailbox::RoleMessageHeaderListPostNo);
    QCOMPARE(actions(model.index(0, 0)), expected.mid(1));

    model.item(0)->setData(false, Imap::Mailbox::RoleIsFetched);
    QVERIFY(actions(model.index(0, 0)).isEmpty());
}

void MailingListsTest::findsListsInModel()
{
    QStandardItemModel model;
    QStandardItem *thread = message(1, listId(QStringLiteral("Zeta Users <zeta.example.org>")));
    thread->appendRow(message(2, listId(QStringLiteral("<zeta.example.org>"))));
    thread->appendRow(message(3, listId(QStringLiteral("Alpha <alpha.example.org>"))));
    model.appendRow(thread);
    model.appendRow(message(4, {}, QStringLiteral("mailto:beta@example.org")));
    model.appendRow(message(5, {}));
    // Not loaded, so not counted
    model.appendRow(message(6, listId(QStringLiteral("Alpha <alpha.example.org>")), QString(), false));
    // A row without a UID, such as a group header, is not a message
    QStandardItem *header = message(0, listId(QStringLiteral("Gamma <gamma.example.org>")));
    model.appendRow(header);
    // A later message adds the description which an earlier one lacked
    model.appendRow(message(7, listId(QStringLiteral("<delta.example.org>"))));
    model.appendRow(message(8, listId(QStringLiteral("Delta <delta.example.org>"))));

    const QList<Found> lists = listsIn(&model);
    QStringList names;
    QList<int> counts;
    for (const Found &found : lists) {
        names << found.list.displayName();
        counts << found.messages;
    }
    QCOMPARE(names, (QStringList{QStringLiteral("Alpha"), QStringLiteral("beta@example.org"), QStringLiteral("Delta"),
                                 QStringLiteral("Zeta Users")}));
    QCOMPARE(counts, (QList<int>{1, 1, 2, 2}));
    QVERIFY(listsIn(nullptr).isEmpty());
}

void MailingListsTest::searchesListIdAndListPost()
{
    QCOMPARE(searchKeys(QStringLiteral("dev.example.org")),
             (QStringList{QStringLiteral("OR"), QStringLiteral("HEADER"), QStringLiteral("List-Id"),
                          QStringLiteral("dev.example.org"), QStringLiteral("HEADER"), QStringLiteral("List-Post"),
                          QStringLiteral("dev.example.org")}));
}

QTEST_GUILESS_MAIN(MailingListsTest)
