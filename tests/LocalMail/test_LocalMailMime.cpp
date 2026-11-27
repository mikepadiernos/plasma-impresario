/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "Imap/Parser/LowLevelParser.h"
#include "Imap/Parser/Message.h"
#include "LocalMail/Mime.h"

using namespace LocalMail;

namespace {

QByteArray sampleMessage()
{
    return Mime::canonicalLineEnds(
        "From: =?UTF-8?Q?J=C3=BCrgen?= <jurgen@example.org>\n"
        "To: Alice <alice@example.org>, bob@example.org\n"
        "Cc: \"Team, Core\" <team@example.org>\n"
        "Subject: Report \"Q3\"\n"
        "Date: Tue, 24 Nov 2026 10:00:00 +0100\n"
        "Message-ID: <m1@example.org>\n"
        "In-Reply-To: <m0@example.org>\n"
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/mixed;\n"
        " boundary=\"xyz\"\n"
        "\n"
        "--xyz\n"
        "Content-Type: text/plain; charset=utf-8\n"
        "Content-Transfer-Encoding: 8bit\n"
        "\n"
        "H\xc3\xa4llo\n"
        "--xyz\n"
        "Content-Type: application/pdf; name=\"r.pdf\"\n"
        "Content-Disposition: attachment; filename=\"r.pdf\"\n"
        "Content-Transfer-Encoding: base64\n"
        "\n"
        "JVBERi0=\n"
        "--xyz\n"
        "Content-Type: message/rfc822\n"
        "\n"
        "From: c@example.org\n"
        "Subject: inner\n"
        "\n"
        "body\n"
        "--xyz--\n");
}

QVariantList parsedList(const QByteArray &text)
{
    const QByteArray line = text + "\r\n";
    int pos = 0;
    return Imap::LowLevelParser::parseList('(', ')', line, pos);
}

}

class LocalMailMimeTest : public QObject
{
    Q_OBJECT
private slots:
    void canonicalizesLineEnds()
    {
        QCOMPARE(Mime::canonicalLineEnds("a\nb\r\nc\rd"), QByteArray("a\r\nb\r\nc\r\nd"));
    }

    void writesNstrings()
    {
        QCOMPARE(Mime::nstring(""), QByteArray("NIL"));
        QCOMPARE(Mime::nstring("", false), QByteArray("\"\""));
        QCOMPARE(Mime::nstring("a\"b\\c"), QByteArray("\"a\\\"b\\\\c\""));
        QCOMPARE(Mime::nstring("line\r\nbreak"), QByteArray("{11}\r\nline\r\nbreak"));
        QCOMPARE(Mime::nstring("H\xc3\xa4"), QByteArray("{3}\r\nH\xc3\xa4"));
    }

    void envelopeReadsBackThroughTheImapParser()
    {
        const Mime::Part message = Mime::parse(sampleMessage());
        const QByteArray text = Mime::envelope(message.header);
        const QByteArray line = text + "\r\n";
        const auto envelope = Imap::Message::Envelope::fromList(parsedList(text), line, 0);
        QCOMPARE(envelope.subject, QStringLiteral("Report \"Q3\""));
        QCOMPARE(envelope.from.size(), 1);
        QCOMPARE(envelope.from.first().name, QStringLiteral("J\u00fcrgen"));
        QCOMPARE(envelope.from.first().mailbox, QStringLiteral("jurgen"));
        QCOMPARE(envelope.from.first().host, QStringLiteral("example.org"));
        QCOMPARE(envelope.sender.size(), 1);
        QCOMPARE(envelope.replyTo.size(), 1);
        QCOMPARE(envelope.to.size(), 2);
        QCOMPARE(envelope.to.at(1).mailbox, QStringLiteral("bob"));
        QCOMPARE(envelope.cc.size(), 1);
        QCOMPARE(envelope.cc.first().name, QStringLiteral("Team, Core"));
        QVERIFY(envelope.bcc.isEmpty());
        QVERIFY(envelope.messageId.contains("m1@example.org"));
        QCOMPARE(envelope.date, QDateTime(QDate(2026, 11, 24), QTime(9, 0), QTimeZone::UTC));
    }

    void bodyStructureReadsBackThroughTheImapParser()
    {
        const Mime::Part message = Mime::parse(sampleMessage());
        for (bool extended : {true, false}) {
            const QByteArray text = Mime::bodyStructure(message, extended);
            const QByteArray line = text + "\r\n";
            const auto parsed = Imap::Message::AbstractMessage::fromList(parsedList(text), line, 0);
            const auto multi = parsed.dynamicCast<Imap::Message::MultiMessage>();
            QVERIFY(multi);
            QCOMPARE(multi->mediaSubType.toLower(), QByteArray("mixed"));
            QCOMPARE(multi->bodies.size(), 3);
            const auto text1 = multi->bodies[0].dynamicCast<Imap::Message::TextMessage>();
            QVERIFY(text1);
            QCOMPARE(text1->bodyFldOctets, quint64(6));
            QCOMPARE(text1->bodyFldParam.value("CHARSET").toLower(), QByteArray("utf-8"));
            const auto pdf = multi->bodies[1].dynamicCast<Imap::Message::BasicMessage>();
            QVERIFY(pdf);
            QCOMPARE(pdf->mediaType.toLower(), QByteArray("application"));
            QCOMPARE(pdf->bodyFldEnc.toLower(), QByteArray("base64"));
            if (extended)
                QCOMPARE(pdf->bodyFldDsp.first.toLower(), QByteArray("attachment"));
            const auto inner = multi->bodies[2].dynamicCast<Imap::Message::MsgMessage>();
            QVERIFY(inner);
            QCOMPARE(inner->envelope.subject, QStringLiteral("inner"));
            QVERIFY(inner->body);
        }
    }

    void readsSections()
    {
        const QByteArray raw = sampleMessage();
        const Mime::Part message = Mime::parse(raw);
        QByteArray out;
        QVERIFY(Mime::section(message, raw, "", &out));
        QCOMPARE(out, raw);
        QVERIFY(Mime::section(message, raw, "1", &out));
        QCOMPARE(out, QByteArray("H\xc3\xa4llo"));
        QVERIFY(Mime::section(message, raw, "2", &out));
        QCOMPARE(out, QByteArray("JVBERi0="));
        QVERIFY(Mime::section(message, raw, "2.MIME", &out));
        QVERIFY(out.startsWith("Content-Type: application/pdf"));
        QVERIFY(out.endsWith("\r\n\r\n"));
        QVERIFY(Mime::section(message, raw, "3.HEADER", &out));
        QVERIFY(out.contains("Subject: inner\r\n"));
        QVERIFY(Mime::section(message, raw, "3.TEXT", &out));
        QCOMPARE(out, QByteArray("body"));
        QVERIFY(Mime::section(message, raw, "HEADER.FIELDS (SUBJECT message-id)", &out));
        QCOMPARE(out, QByteArray("Subject: Report \"Q3\"\r\nMessage-ID: <m1@example.org>\r\n\r\n"));
        QVERIFY(Mime::section(message, raw, "HEADER.FIELDS.NOT (FROM TO CC SUBJECT DATE MESSAGE-ID IN-REPLY-TO MIME-VERSION)", &out));
        QVERIFY(out.startsWith("Content-Type: multipart/mixed;\r\n boundary=\"xyz\"\r\n"));
        QVERIFY(Mime::section(message, raw, "TEXT", &out));
        QVERIFY(out.startsWith("--xyz\r\n"));
        QVERIFY(!Mime::section(message, raw, "9", &out));
    }

    void readsSentDate()
    {
        const Mime::Part message = Mime::parse(sampleMessage());
        QCOMPARE(Mime::sentDate(message.header).toUTC(), QDateTime(QDate(2026, 11, 24), QTime(9, 0), QTimeZone::UTC));
        QVERIFY(!Mime::sentDate(Mime::Header("Subject: x\r\n\r\n")).isValid());
    }

    void treatsAMessageWithoutHeaderEndAsHeaderOnly()
    {
        const Mime::Part message = Mime::parse("Subject: only\r\n");
        QCOMPARE(message.header.value("subject"), QByteArray("only"));
        QVERIFY(message.body.isEmpty());
        QCOMPARE(message.type, QByteArray("text"));
    }
};

QTEST_GUILESS_MAIN(LocalMailMimeTest)
#include "test_LocalMailMime.moc"
