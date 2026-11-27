/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QProcessEnvironment>
#include <QTest>
#include <QTimeZone>

#include "UiUtils/ActivationToken.h"

using UiUtils::ActivationToken;

/** @short Tokens which let a window of the running application come to the front for a request */
class ActivationTokenTest : public QObject
{
    Q_OBJECT
private slots:
    void acceptsTokens();
    void readsTheEnvironment();
    void isTakenOnce();
    void expires();
    void laterTokensReplaceEarlierOnes();

private:
    static QDateTime at(qint64 ms) { return QDateTime::fromMSecsSinceEpoch(1'800'000'000'000 + ms, QTimeZone::UTC); }
};

void ActivationTokenTest::acceptsTokens()
{
    // As KWin and KStartupInfo create them
    QVERIFY(ActivationToken::isAcceptable(QStringLiteral("kwin-5-2c2b1d2e-6a1f-4f1d-9e3e-0d4b2a7c1f00")));
    QVERIFY(ActivationToken::isAcceptable(QStringLiteral("plasmashell-1234-host_TIME56789")));
    QVERIFY(ActivationToken::isAcceptable(QString(ActivationToken::maximumLength, QLatin1Char('a'))));

    QVERIFY(!ActivationToken::isAcceptable(QString()));
    QVERIFY(!ActivationToken::isAcceptable(QString(ActivationToken::maximumLength + 1, QLatin1Char('a'))));
    for (const QString &token : {QStringLiteral("two words"), QStringLiteral("line\nbreak"), QStringLiteral("tab\there"),
                                 QStringLiteral("t\u00f6ken"), QStringLiteral("nul\u0000"), QStringLiteral("del\u007f")})
        QVERIFY2(!ActivationToken::isAcceptable(token), qPrintable(token));
}

void ActivationTokenTest::readsTheEnvironment()
{
    QProcessEnvironment environment;
    QVERIFY(ActivationToken::fromEnvironment(environment).isEmpty());

    environment.insert(QStringLiteral("DESKTOP_STARTUP_ID"), QStringLiteral("x11-id_TIME1"));
    QCOMPARE(ActivationToken::fromEnvironment(environment), QStringLiteral("x11-id_TIME1"));

    // Wayland's token comes first; a session can set both
    environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), QStringLiteral("wayland-token"));
    QCOMPARE(ActivationToken::fromEnvironment(environment), QStringLiteral("wayland-token"));

    // An unusable token does not hide a usable one
    environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), QStringLiteral("not a token"));
    QCOMPARE(ActivationToken::fromEnvironment(environment), QStringLiteral("x11-id_TIME1"));
    environment.insert(QStringLiteral("DESKTOP_STARTUP_ID"), QString());
    QVERIFY(ActivationToken::fromEnvironment(environment).isEmpty());
}

void ActivationTokenTest::isTakenOnce()
{
    ActivationToken token;
    QVERIFY(!token.isPending());
    QVERIFY(token.take(at(0)).isEmpty());

    token.handOver(QStringLiteral("first"), at(0));
    QVERIFY(token.isPending());
    QCOMPARE(token.take(at(100)), QStringLiteral("first"));
    QVERIFY(!token.isPending());
    QVERIFY(token.take(at(200)).isEmpty());

    // Unusable tokens are not kept
    token.handOver(QStringLiteral("not a token"), at(300));
    QVERIFY(!token.isPending());
}

void ActivationTokenTest::expires()
{
    ActivationToken token;
    token.handOver(QStringLiteral("late"), at(0));
    QCOMPARE(token.take(at(ActivationToken::lifetimeMs)), QStringLiteral("late"));

    token.handOver(QStringLiteral("too-late"), at(0));
    QVERIFY(token.take(at(ActivationToken::lifetimeMs + 1)).isEmpty());
    // An expired token is dropped rather than kept for the next window
    QVERIFY(!token.isPending());

    // A clock set back does not make a token valid for longer
    token.handOver(QStringLiteral("clock"), at(1000));
    QVERIFY(token.take(at(0)).isEmpty());
}

void ActivationTokenTest::laterTokensReplaceEarlierOnes()
{
    ActivationToken token;
    token.handOver(QStringLiteral("first"), at(0));
    token.handOver(QStringLiteral("second"), at(10));
    QCOMPARE(token.take(at(20)), QStringLiteral("second"));

    token.handOver(QStringLiteral("kept"), at(0));
    token.handOver(QString(), at(10));
    QCOMPARE(token.take(at(20)), QStringLiteral("kept"));
}

QTEST_GUILESS_MAIN(ActivationTokenTest)
#include "test_ActivationToken.moc"
