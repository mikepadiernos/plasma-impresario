/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_MessageSecurity.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/MessageSecurity.h"
#include <QStandardItemModel>
#include <QtTest>

using namespace Imap::Mailbox;
using Security = UiUtils::MessageSecurity;

namespace {

/** Remembers parts whose content was requested, which would start a download in
 * the IMAP model */
class RecordingModel : public QStandardItemModel
{
public:
    mutable QList<QPersistentModelIndex> contentRequests;

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (role == RolePartData || role == RolePartUnicodeText)
            contentRequests << QPersistentModelIndex(index);
        return QStandardItemModel::data(index, role);
    }
};

QStandardItem *part(const char *mimeType, const QList<QStandardItem *> &children = {})
{
    auto *item = new QStandardItem(QString::fromLatin1(mimeType));
    item->setData(QByteArray(mimeType), RolePartMimeType);
    for (auto *child : children)
        item->appendRow(child);
    return item;
}

QStandardItem *text(const QString &body, bool fetched = true)
{
    auto *item = part("text/plain");
    item->setData(fetched, RoleIsFetched);
    if (fetched)
        item->setData(body, RolePartUnicodeText);
    return item;
}

/** index(0, 0) is the message, like Cryptography::MessageModel and a message of
 * the IMAP model */
QModelIndex message(QStandardItemModel &model, QStandardItem *body)
{
    auto *root = new QStandardItem(QStringLiteral("message"));
    root->appendRow(body);
    model.appendRow(root);
    return model.index(model.rowCount() - 1, 0);
}

/** A multipart/signed as the reader's GpgMe part reports it */
QStandardItem *readerSigned(bool pending, bool checked)
{
    auto *item = part("multipart/signed", {text(QStringLiteral("hello"))});
    item->setData(checked, RolePartSignatureVerifySupported);
    item->setData(false, RolePartDecryptionSupported);
    item->setData(pending, RolePartCryptoNotFinishedYet);
    return item;
}

QStandardItem *readerEncrypted(bool pending)
{
    auto *item = part("multipart/encrypted");
    item->setData(false, RolePartSignatureVerifySupported);
    item->setData(true, RolePartDecryptionSupported);
    item->setData(pending, RolePartCryptoNotFinishedYet);
    return item;
}

} // namespace

void MessageSecurityTest::unprotectedMessage()
{
    QStandardItemModel model;
    const QModelIndex msg = message(model, part("multipart/alternative", {text(QStringLiteral("hi")), part("text/html")}));
    const Security security = Security::evaluate(msg);
    QVERIFY(security.isEmpty());
    QVERIFY(!security.isPending());
    QVERIFY(Security::evaluate(QModelIndex()).isEmpty());
}

void MessageSecurityTest::mimeStructureIsNeverVerified_data()
{
    QTest::addColumn<QByteArray>("mimeType");
    QTest::addColumn<QByteArray>("smimeType");
    QTest::addColumn<int>("signature");
    QTest::addColumn<int>("encryption");

    QTest::newRow("pgp-signed") << QByteArrayLiteral("multipart/signed") << QByteArray() << int(Security::Signature::NotVerified)
                                << int(Security::Encryption::None);
    QTest::newRow("pgp-encrypted") << QByteArrayLiteral("multipart/encrypted") << QByteArray() << int(Security::Signature::None)
                                   << int(Security::Encryption::NotDecrypted);
    QTest::newRow("smime-enveloped") << QByteArrayLiteral("application/pkcs7-mime") << QByteArrayLiteral("enveloped-data") << int(Security::Signature::None)
                                     << int(Security::Encryption::NotDecrypted);
    QTest::newRow("smime-signed-data") << QByteArrayLiteral("application/x-pkcs7-mime") << QByteArrayLiteral("Signed-Data")
                                       << int(Security::Signature::NotVerified) << int(Security::Encryption::None);
}

void MessageSecurityTest::mimeStructureIsNeverVerified()
{
    QFETCH(QByteArray, mimeType);
    QFETCH(QByteArray, smimeType);
    QFETCH(int, signature);
    QFETCH(int, encryption);

    RecordingModel model;
    auto *body = part(mimeType.constData(), {part("application/pgp-encrypted"), part("application/octet-stream")});
    if (!smimeType.isEmpty())
        body->setData(QVariant::fromValue(QMap<QByteArray, QByteArray>{{"SMIME-TYPE", smimeType}}), RolePartBodyFldParam);
    const Security security = Security::evaluate(message(model, body));
    QCOMPARE(int(security.signature), signature);
    QCOMPARE(int(security.encryption), encryption);
    QVERIFY(!security.inlinePgp);
    QVERIFY(security.signer.isEmpty());
    QVERIFY(model.contentRequests.isEmpty());
}

void MessageSecurityTest::readerSignatureResults_data()
{
    QTest::addColumn<bool>("pending");
    QTest::addColumn<bool>("checked");
    QTest::addColumn<bool>("keyMissing");
    QTest::addColumn<bool>("bad");
    QTest::addColumn<bool>("trusted");
    QTest::addColumn<bool>("validIgnoringTrust");
    QTest::addColumn<int>("expected");

    QTest::newRow("verifying") << true << true << false << false << false << false << int(Security::Signature::Pending);
    QTest::newRow("trusted") << false << true << false << false << true << true << int(Security::Signature::Trusted);
    QTest::newRow("untrusted-or-stranger") << false << true << false << false << false << true << int(Security::Signature::Untrusted);
    QTest::newRow("bad") << false << true << false << true << false << false << int(Security::Signature::Bad);
    QTest::newRow("unknown-key") << false << true << true << false << false << false << int(Security::Signature::UnknownKey);
    QTest::newRow("key-lookup-error") << false << true << false << false << false << false << int(Security::Signature::Unverifiable);
    // Malformed, data unavailable, no signatures, or no GpgME: the reader gives
    // up before verifying.
    QTest::newRow("not-attempted") << false << false << false << false << false << false << int(Security::Signature::Unverifiable);
}

void MessageSecurityTest::readerSignatureResults()
{
    QFETCH(bool, pending);
    QFETCH(bool, checked);
    QFETCH(bool, keyMissing);
    QFETCH(bool, bad);
    QFETCH(bool, trusted);
    QFETCH(bool, validIgnoringTrust);
    QFETCH(int, expected);

    QStandardItemModel model;
    auto *body = readerSigned(pending, checked);
    body->setData(keyMissing, RolePartSignatureKeyMissing);
    body->setData(bad, RolePartSignatureBad);
    body->setData(trusted, RolePartSignatureValidTrusted);
    body->setData(validIgnoringTrust, RolePartSignatureValidDisregardingTrust);
    body->setData(QStringLiteral("Ann <ann@example.org>"), RolePartSignatureSignerName);
    body->setData(QStringLiteral("Short status"), RolePartCryptoTLDR);
    body->setData(QStringLiteral("Long status"), RolePartCryptoDetailedMessage);
    const Security security = Security::evaluate(message(model, body));
    QCOMPARE(int(security.signature), expected);
    QCOMPARE(security.encryption, Security::Encryption::None);
    QCOMPARE(security.isPending(), pending);
    QCOMPARE(security.signer, QStringLiteral("Ann <ann@example.org>"));
    QCOMPARE(security.summary, QStringLiteral("Short status"));
    QCOMPARE(security.details, QStringLiteral("Long status"));
}

void MessageSecurityTest::readerDecryptionResults_data()
{
    QTest::addColumn<bool>("pending");
    QTest::addColumn<bool>("failed");
    QTest::addColumn<bool>("isSigned");
    QTest::addColumn<int>("encryption");
    QTest::addColumn<int>("signature");

    QTest::newRow("decrypting") << true << false << false << int(Security::Encryption::Pending) << int(Security::Signature::None);
    QTest::newRow("decrypted") << false << false << false << int(Security::Encryption::Decrypted) << int(Security::Signature::None);
    QTest::newRow("decrypted-and-signed") << false << false << true << int(Security::Encryption::Decrypted) << int(Security::Signature::Trusted);
    QTest::newRow("failed") << false << true << false << int(Security::Encryption::Failed) << int(Security::Signature::None);
}

void MessageSecurityTest::readerDecryptionResults()
{
    QFETCH(bool, pending);
    QFETCH(bool, failed);
    QFETCH(bool, isSigned);
    QFETCH(int, encryption);
    QFETCH(int, signature);

    RecordingModel model;
    auto *body = readerEncrypted(pending);
    body->setData(failed, RolePartCryptoDecryptionFailed);
    body->setData(isSigned, RolePartSignatureVerifySupported);
    body->setData(isSigned, RolePartSignatureValidTrusted);
    body->setData(isSigned, RolePartSignatureValidDisregardingTrust);
    if (!pending && !failed)
        body->appendRow(text(QStringLiteral("decrypted text")));
    const Security security = Security::evaluate(message(model, body));
    QCOMPARE(int(security.encryption), encryption);
    QCOMPARE(int(security.signature), signature);
}

void MessageSecurityTest::outermostResultWins()
{
    // An encrypted message whose decrypted content is itself a MIME-level signed
    // part, which the reader shows in place: the outer decryption result is
    // reported, and the inner signature as not verified.
    QStandardItemModel model;
    auto *body = readerEncrypted(false);
    body->setData(false, RolePartCryptoDecryptionFailed);
    body->setData(QStringLiteral("Encrypted message"), RolePartCryptoTLDR);
    body->appendRow(part("multipart/signed", {text(QStringLiteral("inner"))}));
    const Security security = Security::evaluate(message(model, body));
    QCOMPARE(security.encryption, Security::Encryption::Decrypted);
    QCOMPARE(security.signature, Security::Signature::NotVerified);
    QCOMPARE(security.summary, QStringLiteral("Encrypted message"));
}

void MessageSecurityTest::inlinePgpInLoadedText()
{
    QStandardItemModel model;
    const QModelIndex signedMsg = message(model, text(QStringLiteral("-----BEGIN PGP SIGNED MESSAGE-----\r\nHash: SHA256\r\n\r\nhi\r\n")));
    Security security = Security::evaluate(signedMsg);
    QCOMPARE(security.signature, Security::Signature::NotVerified);
    QCOMPARE(security.encryption, Security::Encryption::None);
    QVERIFY(security.inlinePgp);

    const QModelIndex encryptedMsg = message(model,
                                             part("multipart/alternative",
                                                  {text(QStringLiteral("Hi,\n-----BEGIN PGP MESSAGE-----\nhQEMA\n-----END PGP "
                                                                       "MESSAGE-----\n"))}));
    security = Security::evaluate(encryptedMsg);
    QCOMPARE(security.encryption, Security::Encryption::NotDecrypted);
    QCOMPARE(security.signature, Security::Signature::None);
    QVERIFY(security.inlinePgp);

    // Quoted armor in a reply and armor in an attached text file are not the
    // message's own protection.
    const QModelIndex quoted = message(model, text(QStringLiteral("> -----BEGIN PGP SIGNED MESSAGE-----\n> hi\n")));
    QVERIFY(Security::evaluate(quoted).isEmpty());
    auto *attachment = text(QStringLiteral("-----BEGIN PGP MESSAGE-----\n"));
    attachment->setData(QByteArrayLiteral("attachment"), RolePartBodyDisposition);
    QVERIFY(Security::evaluate(message(model, part("multipart/mixed", {text(QStringLiteral("see attached")), attachment}))).isEmpty());
}

void MessageSecurityTest::inlinePgpNeverDownloads()
{
    RecordingModel model;
    const QModelIndex msg = message(model, part("multipart/mixed", {text(QString(), false), part("image/png")}));
    QVERIFY(Security::evaluate(msg).isEmpty());
    QVERIFY(model.contentRequests.isEmpty());
}

void MessageSecurityTest::attachedMessagesAreIgnored()
{
    QStandardItemModel model;
    auto *forwarded = part("message/rfc822", {part("multipart/signed", {text(QStringLiteral("inner"))})});
    const QModelIndex msg = message(model, part("multipart/mixed", {text(QStringLiteral("fwd")), forwarded}));
    QVERIFY(Security::evaluate(msg).isEmpty());
}

QTEST_GUILESS_MAIN(MessageSecurityTest)
