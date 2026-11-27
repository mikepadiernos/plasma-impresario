/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QtTest>
#include "test_MessageId.h"
#include "UiUtils/MessageId.h"
#include "UiUtils/MessageIdLookup.h"

using UiUtils::MessageId::Problem;

Q_DECLARE_METATYPE(Problem)

void MessageIdTest::parses_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QByteArray>("messageId");
    QTest::addColumn<QByteArray>("contentId");
    QTest::addColumn<Problem>("problem");

    QTest::newRow("bare") << QStringLiteral("abc@example.org") << QByteArray("abc@example.org") << QByteArray() << Problem::None;
    QTest::newRow("brackets") << QStringLiteral("  <abc@example.org>\n") << QByteArray("abc@example.org") << QByteArray()
                              << Problem::None;
    QTest::newRow("bare keeps percent signs") << QStringLiteral("a%40b@example.org") << QByteArray("a%40b@example.org")
                                              << QByteArray() << Problem::None;
    QTest::newRow("mid") << QStringLiteral("mid:abc@example.org") << QByteArray("abc@example.org") << QByteArray()
                         << Problem::None;
    QTest::newRow("mid upper case") << QStringLiteral("MID:abc@example.org") << QByteArray("abc@example.org") << QByteArray()
                                    << Problem::None;
    QTest::newRow("mid encoded") << QStringLiteral("mid:%3Cfoo4%25foo1@bar.net%3E") << QByteArray("foo4%foo1@bar.net")
                                 << QByteArray() << Problem::None;
    QTest::newRow("mid with part") << QStringLiteral("mid:960830.1639@XIson.com/partA.960830.1639@XIson.com")
                                   << QByteArray("960830.1639@XIson.com") << QByteArray("partA.960830.1639@XIson.com")
                                   << Problem::None;
    QTest::newRow("several @") << QStringLiteral("\"a@b\"@example.org") << QByteArray("\"a@b\"@example.org") << QByteArray()
                               << Problem::None;
    QTest::newRow("utf-8") << QStringLiteral("zprávа@příklad.cz") << QStringLiteral("zprávа@příklad.cz").toUtf8() << QByteArray()
                           << Problem::None;

    QTest::newRow("empty") << QString() << QByteArray() << QByteArray() << Problem::Empty;
    QTest::newRow("blank") << QStringLiteral("   ") << QByteArray() << QByteArray() << Problem::Empty;
    QTest::newRow("empty brackets") << QStringLiteral("<>") << QByteArray() << QByteArray() << Problem::Empty;
    QTest::newRow("empty mid") << QStringLiteral("mid:") << QByteArray() << QByteArray() << Problem::Empty;
    QTest::newRow("space") << QStringLiteral("a b@example.org") << QByteArray() << QByteArray() << Problem::InvalidCharacter;
    QTest::newRow("encoded space") << QStringLiteral("mid:a%20b@example.org") << QByteArray() << QByteArray()
                                   << Problem::InvalidCharacter;
    QTest::newRow("control") << QStringLiteral("a\x01@example.org") << QByteArray() << QByteArray() << Problem::InvalidCharacter;
    QTest::newRow("delete") << QStringLiteral("a\x7f@example.org") << QByteArray() << QByteArray() << Problem::InvalidCharacter;
    QTest::newRow("inner bracket") << QStringLiteral("<a<b@example.org>") << QByteArray() << QByteArray()
                                   << Problem::InvalidCharacter;
    QTest::newRow("two ids") << QStringLiteral("<a@x> <b@y>") << QByteArray() << QByteArray() << Problem::InvalidCharacter;
    QTest::newRow("no at") << QStringLiteral("abc.example.org") << QByteArray() << QByteArray() << Problem::MissingAt;
    QTest::newRow("nothing before at") << QStringLiteral("@example.org") << QByteArray() << QByteArray() << Problem::MissingAt;
    QTest::newRow("nothing after at") << QStringLiteral("mid:abc@") << QByteArray() << QByteArray() << Problem::MissingAt;
    QTest::newRow("too long") << (QString(995, QLatin1Char('a')) + QStringLiteral("@b.c")) << QByteArray() << QByteArray()
                              << Problem::TooLong;
    QTest::newRow("longest") << (QString(994, QLatin1Char('a')) + QStringLiteral("@b.c"))
                             << (QByteArray(994, 'a') + "@b.c") << QByteArray() << Problem::None;
}

void MessageIdTest::parses()
{
    QFETCH(QString, input);
    QFETCH(QByteArray, messageId);
    QFETCH(QByteArray, contentId);
    QFETCH(Problem, problem);

    const UiUtils::MessageId::Parsed parsed = UiUtils::MessageId::parse(input);
    QCOMPARE(parsed.problem, problem);
    QCOMPARE(parsed.isValid(), problem == Problem::None);
    QCOMPARE(parsed.messageId, messageId);
    QCOMPARE(parsed.contentId, contentId);
}

void MessageIdTest::buildsUrls()
{
    using namespace UiUtils::MessageId;
    QCOMPARE(url("<abc@example.org>").toString(), QStringLiteral("mid:abc@example.org"));
    const QUrl encoded = url("foo4%foo1 x@bar.net");
    QCOMPARE(encoded.scheme(), QStringLiteral("mid"));
    QCOMPARE(encoded.toString(QUrl::FullyEncoded), QStringLiteral("mid:foo4%25foo1%20x@bar.net"));
    // A URL which the handler receives reads back as the same ID
    QCOMPARE(parse(url("a/b%c@example.org").toString(QUrl::FullyEncoded)).messageId, QByteArray("a/b%c@example.org"));
    QCOMPARE(parse(url("abc@example.org").toString(QUrl::FullyEncoded)).messageId, QByteArray("abc@example.org"));
}

void MessageIdTest::searchesTheHeader()
{
    QCOMPARE(UiUtils::MessageId::searchConditions("<abc@example.org>"),
             (QStringList{QStringLiteral("HEADER"), QStringLiteral("Message-ID"), QStringLiteral("abc@example.org")}));
}

void MessageIdTest::comparesIds()
{
    using UiUtils::MessageId::sameId;
    QVERIFY(sameId("abc@example.org", "<abc@example.org>"));
    QVERIFY(sameId(" <abc@example.org> ", "abc@example.org"));
    QVERIFY(!sameId("abc@example.org", "xabc@example.org"));
    QVERIFY(!sameId("abc@example.org", "ABC@example.org"));
    QVERIFY(!sameId("", ""));
}

void MessageIdTest::lookupWithoutAccounts()
{
    using Lookup = UiUtils::MessageIdLookup;
    Lookup lookup;
    QSignalSpy finished(&lookup, &Lookup::finished);
    QCOMPARE(lookup.state(), Lookup::State::Idle);
    QCOMPARE(lookup.start("abc@example.org", {}), Lookup::State::NotFound);
    QCOMPARE(finished.size(), 1);
    QVERIFY(lookup.found().isEmpty());
    QVERIFY(lookup.serverAccounts().isEmpty());
    QVERIFY(!lookup.searchServers());

    // Accounts whose model is gone are skipped
    QCOMPARE(lookup.start("abc@example.org", {Lookup::Account{QStringLiteral("gone"), nullptr, nullptr}}), Lookup::State::NotFound);
    QCOMPARE(finished.size(), 2);
    QCOMPARE(lookup.start("not an id", {}), Lookup::State::NotFound);
    QCOMPARE(lookup.messageId(), QByteArray("not an id"));
    lookup.cancel();
    QCOMPARE(lookup.state(), Lookup::State::NotFound);
}

QTEST_GUILESS_MAIN(MessageIdTest)
