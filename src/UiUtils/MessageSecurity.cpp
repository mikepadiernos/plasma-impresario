/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageSecurity.h"

#include <QAbstractItemModel>
#include <QMap>
#include <QRegularExpression>

#include "Imap/Model/ItemRoles.h"

namespace UiUtils {

namespace {

using namespace Imap::Mailbox;

constexpr int MaximumDepth = 100;

QByteArray mimeTypeOf(const QModelIndex &part)
{
    return part.data(RolePartMimeType).toByteArray().toLower();
}

/** Parts replaced by the cryptography layer answer these roles; the IMAP model
 * and plain parts never do. */
bool isReaderCryptoPart(const QModelIndex &part)
{
    return part.data(RolePartSignatureVerifySupported).isValid() || part.data(RolePartDecryptionSupported).isValid();
}

MessageSecurity::Signature signatureResult(const QModelIndex &part)
{
    using Signature = MessageSecurity::Signature;
    if (part.data(RolePartSignatureKeyMissing).toBool())
        return Signature::UnknownKey;
    if (part.data(RolePartSignatureBad).toBool())
        return Signature::Bad;
    if (part.data(RolePartSignatureValidTrusted).toBool())
        return Signature::Trusted;
    if (part.data(RolePartSignatureValidDisregardingTrust).toBool())
        return Signature::Untrusted;
    return Signature::Unverifiable;
}

class Walker
{
public:
    MessageSecurity result;

    void walk(const QModelIndex &part, int depth)
    {
        if (!part.isValid() || depth > MaximumDepth)
            return;
        const QAbstractItemModel *model = part.model();
        const QByteArray mimeType = mimeTypeOf(part);

        if (mimeType == "message/rfc822")
            return;

        if (isReaderCryptoPart(part)) {
            readCryptoPart(part, mimeType);
            // The decrypted or verified content is inserted below; it may carry its
            // own MIME-level protection.
            walkChildren(part, depth);
            return;
        }

        if (mimeType == "multipart/signed") {
            setSignature(MessageSecurity::Signature::NotVerified);
            // The second part is the detached signature.
            walk(model->index(0, 0, part), depth + 1);
            return;
        }
        if (mimeType == "multipart/encrypted") {
            // Without the cryptography layer, the children are the protocol version
            // and the ciphertext.
            setEncryption(MessageSecurity::Encryption::NotDecrypted);
            return;
        }
        if (mimeType == "application/pkcs7-mime" || mimeType == "application/x-pkcs7-mime") {
            const auto params = part.data(RolePartBodyFldParam).value<QMap<QByteArray, QByteArray>>();
            if (params.value(QByteArrayLiteral("SMIME-TYPE")).toLower() == "signed-data")
                setSignature(MessageSecurity::Signature::NotVerified);
            else
                setEncryption(MessageSecurity::Encryption::NotDecrypted);
            return;
        }
        if (mimeType == "text/plain") {
            readInlinePgp(part);
            return;
        }
        if (mimeType.startsWith("multipart/"))
            walkChildren(part, depth);
    }

private:
    void walkChildren(const QModelIndex &part, int depth)
    {
        const QAbstractItemModel *model = part.model();
        const int rows = model->rowCount(part);
        for (int row = 0; row < rows; ++row)
            walk(model->index(row, 0, part), depth + 1);
    }

    void setSignature(MessageSecurity::Signature signature)
    {
        if (result.signature == MessageSecurity::Signature::None)
            result.signature = signature;
    }

    void setEncryption(MessageSecurity::Encryption encryption)
    {
        if (result.encryption == MessageSecurity::Encryption::None)
            result.encryption = encryption;
    }

    void readCryptoPart(const QModelIndex &part, const QByteArray &mimeType)
    {
        // Report the outermost result; nested protection is rare and the reader
        // shows it in place.
        const bool hadResult = !result.isEmpty();
        const bool pending = part.data(RolePartCryptoNotFinishedYet).toBool();
        const bool encrypted = part.data(RolePartDecryptionSupported).toBool();
        const bool signatureChecked = part.data(RolePartSignatureVerifySupported).toBool();

        if (encrypted) {
            if (pending)
                setEncryption(MessageSecurity::Encryption::Pending);
            else if (part.data(RolePartCryptoDecryptionFailed).toBool())
                setEncryption(MessageSecurity::Encryption::Failed);
            else
                setEncryption(MessageSecurity::Encryption::Decrypted);
            // A signature inside the encrypted data is only known once decryption
            // finished.
            if (!pending && signatureChecked)
                setSignature(signatureResult(part));
        } else if (pending) {
            setSignature(MessageSecurity::Signature::Pending);
        } else if (signatureChecked) {
            setSignature(signatureResult(part));
        } else if (mimeType == "multipart/signed") {
            // The verification failed before any signature was looked at.
            setSignature(MessageSecurity::Signature::Unverifiable);
        }

        if (!hadResult) {
            result.signer = part.data(RolePartSignatureSignerName).toString();
            result.signDate = part.data(RolePartSignatureSignDate).toDateTime();
            result.summary = part.data(RolePartCryptoTLDR).toString();
            result.details = part.data(RolePartCryptoDetailedMessage).toString();
        }
    }

    void readInlinePgp(const QModelIndex &part)
    {
        if (part.data(RolePartBodyDisposition).toByteArray().toLower() == "attachment")
            return;
        // Never download a part just to look for armor.
        if (!part.data(RoleIsFetched).toBool())
            return;
        const QString text = part.data(RolePartUnicodeText).toString();
        static const QRegularExpression signedArmor(QStringLiteral("(^|\\n)-----BEGIN PGP SIGNED MESSAGE-----\\r?\\n"));
        static const QRegularExpression encryptedArmor(QStringLiteral("(^|\\n)-----BEGIN PGP MESSAGE-----\\r?\\n"));
        bool found = false;
        if (text.contains(signedArmor) && result.signature == MessageSecurity::Signature::None) {
            result.signature = MessageSecurity::Signature::NotVerified;
            found = true;
        }
        if (text.contains(encryptedArmor) && result.encryption == MessageSecurity::Encryption::None) {
            result.encryption = MessageSecurity::Encryption::NotDecrypted;
            found = true;
        }
        if (found)
            result.inlinePgp = true;
    }
};

} // namespace

MessageSecurity MessageSecurity::evaluate(const QModelIndex &message)
{
    if (!message.isValid())
        return {};
    Walker walker;
    const QAbstractItemModel *model = message.model();
    const int rows = model->rowCount(message);
    for (int row = 0; row < rows; ++row)
        walker.walk(model->index(row, 0, message), 0);
    return walker.result;
}

} // namespace UiUtils
