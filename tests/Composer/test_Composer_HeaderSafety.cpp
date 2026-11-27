/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QBuffer>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QTest>
#include <QTimeZone>
#include <QUrl>

#include "test_Composer_HeaderSafety.h"
#include "Composer/HeaderSafety.h"
#include "Composer/Mailto.h"
#include "Composer/MessageComposer.h"

using Imap::Message::MailAddress;
namespace Safety = Composer::HeaderSafety;

namespace {

MailAddress senderAddress()
{
    return MailAddress(QStringLiteral("Foo Bar"), QString(), QStringLiteral("foo.bar"), QStringLiteral("example.org"));
}

std::unique_ptr<Composer::MessageComposer> makeComposer()
{
    auto composer = std::make_unique<Composer::MessageComposer>(nullptr);
    composer->setFrom(senderAddress());
    composer->setTimestamp(QDateTime(QDate(2026, 11, 26), QTime(10, 0), QTimeZone::UTC));
    composer->setSubject(QStringLiteral("testing"));
    composer->setText(QStringLiteral("Sample message"));
    composer->setReportTrojitaVersions(false);
    return composer;
}

QByteArray serialize(const Composer::AbstractComposer &composer, Composer::BccMode bcc = Composer::BccMode::Omit)
{
    QByteArray message;
    QBuffer buffer(&message);
    buffer.open(QIODevice::WriteOnly);
    QString error;
    if (!composer.asRawMessage(&buffer, &error, bcc))
        qWarning() << "Serialization failed:" << error;
    return message;
}

/** The header block, with folded continuation lines joined to the line they continue */
QList<QByteArray> headerLines(const QByteArray &message)
{
    QList<QByteArray> lines;
    const QByteArray head = message.left(message.indexOf("\r\n\r\n"));
    for (const QByteArray &line : head.split('\n')) {
        QByteArray clean = line;
        if (clean.endsWith('\r'))
            clean.chop(1);
        if (!lines.isEmpty() && (clean.startsWith(' ') || clean.startsWith('\t')))
            lines.last().append(clean);
        else
            lines << clean;
    }
    return lines;
}

QList<QByteArray> headerNames(const QByteArray &message)
{
    QList<QByteArray> names;
    for (const QByteArray &line : headerLines(message))
        names << line.left(line.indexOf(':'));
    return names;
}

QByteArray headerValue(const QByteArray &message, const QByteArray &name)
{
    for (const QByteArray &line : headerLines(message)) {
        if (line.startsWith(QByteArray(name + ':')))
            return line.mid(name.size() + 1).trimmed();
    }
    return {};
}

bool hasBareLineBreaks(const QByteArray &message)
{
    for (int i = 0; i < message.size(); ++i) {
        if (message[i] == '\n' && (i == 0 || message[i - 1] != '\r'))
            return true;
        if (message[i] == '\r' && (i + 1 >= message.size() || message[i + 1] != '\n'))
            return true;
    }
    return false;
}

const QList<QByteArray> expectedHeaders{"From", "To", "Subject", "Date", "MIME-Version", "Message-ID", "User-Agent",
                                        "Content-Type", "Content-Transfer-Encoding"};

}

void ComposerHeaderSafetyTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ComposerHeaderSafetyTest::text_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << QStringLiteral("Hello world") << QStringLiteral("Hello world");
    QTest::newRow("crlf") << QStringLiteral("Hi\r\nBcc: x@y") << QStringLiteral("Hi Bcc: x@y");
    QTest::newRow("lone cr") << QStringLiteral("Hi\rthere") << QStringLiteral("Hi there");
    QTest::newRow("lone lf") << QStringLiteral("Hi\nthere") << QStringLiteral("Hi there");
    QTest::newRow("tab") << QStringLiteral("a\tb") << QStringLiteral("a b");
    QTest::newRow("nul") << QString(QStringLiteral("a") + QChar(0) + QStringLiteral("b")) << QStringLiteral("a b");
    QTest::newRow("del") << QString(QStringLiteral("a") + QChar(0x7f) + QStringLiteral("b")) << QStringLiteral("a b");
    QTest::newRow("c1") << QString(QStringLiteral("a") + QChar(0x9b) + QStringLiteral("b")) << QStringLiteral("a b");
    QTest::newRow("nel") << QString(QStringLiteral("a") + QChar(0x85) + QStringLiteral("b")) << QStringLiteral("a b");
    QTest::newRow("line separator") << QString(QStringLiteral("a") + QChar(0x2028) + QStringLiteral("b"))
                                    << QStringLiteral("a b");
    QTest::newRow("paragraph separator") << QString(QStringLiteral("a") + QChar(0x2029) + QStringLiteral("b"))
                                         << QStringLiteral("a b");
    QTest::newRow("unicode kept") << QStringLiteral("Přílišné žluťoučký") << QStringLiteral("Přílišné žluťoučký");
    QTest::newRow("nbsp kept") << QString(QStringLiteral("a") + QChar(0xa0) + QStringLiteral("b"))
                               << QString(QStringLiteral("a") + QChar(0xa0) + QStringLiteral("b"));
}

void ComposerHeaderSafetyTest::text()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(Safety::text(input), expected);
}

void ComposerHeaderSafetyTest::strippedAddressParts()
{
    QCOMPARE(Safety::stripped(QStringLiteral("foo\r\n.bar")), QStringLiteral("foo.bar"));
    const MailAddress clean = Safety::address(MailAddress(QStringLiteral("Eve\r\nBcc: x@y"), QString(),
                                                          QStringLiteral("eve\n"), QStringLiteral("exa\rmple.org")));
    QCOMPARE(clean.name, QStringLiteral("Eve Bcc: x@y"));
    QCOMPARE(clean.mailbox, QStringLiteral("eve"));
    QCOMPARE(clean.host, QStringLiteral("example.org"));
}

void ComposerHeaderSafetyTest::messageIdKeepsVisibleAscii()
{
    QCOMPARE(Safety::messageId("abc@example.org"), QByteArray("abc@example.org"));
    QCOMPARE(Safety::messageId("<abc@exa\r\nmple.org>\r\nBcc: x"), QByteArray("abc@example.orgBcc:x"));
    QCOMPARE(Safety::messageId("\r\n \t"), QByteArray());
}

void ComposerHeaderSafetyTest::subjectCannotAddHeaders()
{
    auto composer = makeComposer();
    composer->setRecipients({qMakePair(Composer::ADDRESS_TO, MailAddress(QString(), QString(), QStringLiteral("to"),
                                                                         QStringLiteral("example.net")))});
    for (const QString &subject : {QStringLiteral("Hi\r\nBcc: eve@example.com"), QStringLiteral("Hi\nBcc: eve@example.com"),
                                   QStringLiteral("Hi\rBcc: eve@example.com"),
                                   QString(QStringLiteral("Hi") + QChar(0x2028) + QStringLiteral("Bcc: eve@example.com"))}) {
        composer->setSubject(subject);
        const QByteArray message = serialize(*composer);
        QCOMPARE(headerNames(message), expectedHeaders);
        QVERIFY(!hasBareLineBreaks(message));
        QCOMPARE(headerValue(message, "Subject"), QByteArray("Hi Bcc: eve@example.com"));
    }
}

void ComposerHeaderSafetyTest::organizationCannotAddHeaders()
{
    auto composer = makeComposer();
    composer->setOrganization(QStringLiteral("ACME\r\nX-Injected: yes"));
    const QByteArray message = serialize(*composer);
    QVERIFY(!headerNames(message).contains("X-Injected"));
    QCOMPARE(headerValue(message, "Organization"), QByteArray("ACME X-Injected: yes"));
}

void ComposerHeaderSafetyTest::addressesCannotAddHeaders()
{
    auto composer = makeComposer();
    composer->setFrom(MailAddress(QStringLiteral("Foo\r\nX-Injected: from"), QString(), QStringLiteral("foo"),
                                  QStringLiteral("example.org")));
    composer->setRecipients({
        qMakePair(Composer::ADDRESS_TO, MailAddress(QStringLiteral("To\r\nX-Injected: to"), QString(),
                                                    QStringLiteral("to\r\n"), QStringLiteral("example.net"))),
        qMakePair(Composer::ADDRESS_CC, MailAddress(QString(), QString(), QStringLiteral("cc"),
                                                    QStringLiteral("example.net\r\nX-Injected: cc"))),
    });
    const QByteArray message = serialize(*composer);
    QVERIFY(!hasBareLineBreaks(message));
    QVERIFY(!headerNames(message).contains("X-Injected"));
    QVERIFY(headerValue(message, "To").endsWith("<to@example.net>"));
}

void ComposerHeaderSafetyTest::referencesCannotAddHeaders()
{
    auto composer = makeComposer();
    composer->setInReplyTo({QByteArray("parent@example.org>\r\nX-Injected: <x")});
    composer->setReferences({QByteArray("root@example.org"), QByteArray("\r\n"), QByteArray("parent@example.org\r\nX-Injected: y")});
    const QByteArray message = serialize(*composer);
    QVERIFY(!hasBareLineBreaks(message));
    QVERIFY(!headerNames(message).contains("X-Injected"));
    QCOMPARE(headerValue(message, "In-Reply-To"), QByteArray("<parent@example.orgX-Injected:x>"));
    QCOMPARE(headerValue(message, "References"), QByteArray("<root@example.org> <parent@example.orgX-Injected:y>"));
}

void ComposerHeaderSafetyTest::attachmentNameCannotAddHeaders()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("attachment data");
    file.flush();

    auto composer = makeComposer();
    QVERIFY(composer->addFileAttachment(file.fileName()));
    composer->setAttachmentName(composer->index(0, 0), QStringLiteral("report\r\nX-Injected: yes.txt"));
    const QByteArray message = serialize(*composer);
    QVERIFY(!hasBareLineBreaks(message));
    QVERIFY(!message.contains("\r\nX-Injected"));
    QVERIFY(message.contains("filename"));
    QVERIFY(message.contains("report"));
}

void ComposerHeaderSafetyTest::smtpEnvelopeHasNoControls()
{
    auto composer = makeComposer();
    composer->setFrom(MailAddress(QString(), QString(), QStringLiteral("foo\r\n"), QStringLiteral("example.org\r\nRSET")));
    composer->setRecipients({qMakePair(Composer::ADDRESS_BCC, MailAddress(QString(), QString(), QStringLiteral("hid\rden"),
                                                                          QStringLiteral("exa\nmple.net")))});
    QCOMPARE(composer->rawFromAddress(), QByteArray("foo@example.orgrset"));
    QCOMPARE(composer->rawRecipientAddresses(), QList<QByteArray>{QByteArray("hidden@example.net")});
}

void ComposerHeaderSafetyTest::mailtoSubjectIsOneLine()
{
    QString subject, body;
    QList<QPair<Composer::RecipientKind, QString>> recipients;
    QList<QByteArray> inReplyTo, references;
    Composer::parseRFC6068Mailto(QUrl(QStringLiteral("mailto:to@example.net?subject=Hi%0D%0ABcc:%20eve@example.com&body=a%0D%0Ab")),
                                 subject, body, recipients, inReplyTo, references);
    QCOMPARE(subject, QStringLiteral("Hi Bcc: eve@example.com"));
    QVERIFY(body.contains(QLatin1Char('\n')));
}

void ComposerHeaderSafetyTest::bccOmittedByDefault()
{
    auto composer = makeComposer();
    composer->setRecipients({
        qMakePair(Composer::ADDRESS_TO, MailAddress(QString(), QString(), QStringLiteral("to"), QStringLiteral("example.net"))),
        qMakePair(Composer::ADDRESS_BCC, MailAddress(QString(), QString(), QStringLiteral("hidden"), QStringLiteral("example.net"))),
    });
    const QByteArray message = serialize(*composer);
    QVERIFY(!headerNames(message).contains("Bcc"));
    QVERIFY(!message.contains("hidden@example.net"));
    QVERIFY(composer->rawRecipientAddresses().contains("hidden@example.net"));
}

void ComposerHeaderSafetyTest::bccKeptOnRequest()
{
    auto composer = makeComposer();
    composer->setRecipients({
        qMakePair(Composer::ADDRESS_TO, MailAddress(QString(), QString(), QStringLiteral("to"), QStringLiteral("example.net"))),
        qMakePair(Composer::ADDRESS_BCC, MailAddress(QStringLiteral("Hidden"), QString(), QStringLiteral("hidden"),
                                                     QStringLiteral("example.net"))),
        qMakePair(Composer::ADDRESS_BCC, MailAddress(QString(), QString(), QStringLiteral("other"), QStringLiteral("example.net"))),
    });
    const QByteArray sent = serialize(*composer, Composer::BccMode::Keep);
    QCOMPARE(headerValue(sent, "Bcc"), QByteArray("Hidden <hidden@example.net>, <other@example.net>"));

    // Both copies describe the same message
    const QByteArray delivered = serialize(*composer, Composer::BccMode::Omit);
    QCOMPARE(headerValue(delivered, "Message-ID"), headerValue(sent, "Message-ID"));
    QCOMPARE(delivered.mid(delivered.indexOf("\r\n\r\n")), sent.mid(sent.indexOf("\r\n\r\n")));
}

void ComposerHeaderSafetyTest::bccInCatenateData()
{
    auto composer = makeComposer();
    composer->setRecipients({qMakePair(Composer::ADDRESS_BCC, MailAddress(QString(), QString(), QStringLiteral("hidden"),
                                                                          QStringLiteral("example.net")))});
    QList<Imap::Mailbox::CatenatePair> parts;
    QString error;
    QVERIFY(composer->asCatenateData(parts, &error));
    QVERIFY(!parts.isEmpty());
    QVERIFY(!parts.first().second.contains("hidden@example.net"));
    QVERIFY(composer->asCatenateData(parts, &error, Composer::BccMode::Keep));
    QVERIFY(parts.first().second.contains("Bcc: <hidden@example.net>\r\n"));
}

void ComposerHeaderSafetyTest::messageIdUsesSenderDomain()
{
    auto composer = makeComposer();
    const QByteArray id = headerValue(serialize(*composer), "Message-ID");
    static const QRegularExpression format(QStringLiteral(
            "^<[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}@example\\.org>$"));
    QVERIFY2(format.match(QString::fromLatin1(id)).hasMatch(), id.constData());

    const QByteArray idn = Composer::AbstractComposer::generateMessageId(
            MailAddress(QString(), QString(), QStringLiteral("a"), QStringLiteral("příklad.cz")));
    QVERIFY2(idn.contains("@xn--") && idn.endsWith(".cz"), idn.constData());
}

void ComposerHeaderSafetyTest::messageIdWithoutDomain()
{
    for (const QString &host : {QString(), QStringLiteral("\r\n"), QStringLiteral("[192.0.2.1]")}) {
        const QByteArray id = Composer::AbstractComposer::generateMessageId(
                MailAddress(QString(), QString(), QStringLiteral("a"), host));
        QVERIFY2(id.endsWith("@localhost"), id.constData());
    }
}

void ComposerHeaderSafetyTest::messageIdIsRandom()
{
    QSet<QByteArray> ids;
    for (int i = 0; i < 100; ++i)
        ids.insert(Composer::AbstractComposer::generateMessageId(senderAddress()));
    QCOMPARE(ids.size(), 100);

    // Two composed messages never share an identifier
    QCOMPARE(headerValue(serialize(*makeComposer()), "Message-ID") == headerValue(serialize(*makeComposer()), "Message-ID"),
             false);
}

void ComposerHeaderSafetyTest::messageIdOmitsHostName()
{
    const QString hostName = QSysInfo::machineHostName();
    auto composer = makeComposer();
    const QByteArray message = serialize(*composer);
    if (!hostName.isEmpty() && hostName != QLatin1String("localhost"))
        QVERIFY2(!message.contains(hostName.toUtf8()), qPrintable(hostName));
    const QByteArray id = headerValue(message, "Message-ID");
    QCOMPARE(id.mid(id.indexOf('@')), QByteArray("@example.org>"));
}

QTEST_GUILESS_MAIN(ComposerHeaderSafetyTest)
