/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QModelIndex>
#include <QString>

namespace UiUtils {

/** @short Signature and encryption state of a message, as far as it is known
without doing any work

The result is read from a tree of MIME parts. When the tree comes from the
reader's part model (Cryptography::MessageModel), the cryptography layer's
verification and decryption results are used. Otherwise, and for parts the
cryptography layer did not handle, only the MIME structure is known, which is
reported as "not verified" or "not decrypted" and never as a verified result.

Evaluating a tree never starts a download, a decryption or a key lookup: inline
PGP markers are only searched for in text parts which are already loaded.
*/
struct MessageSecurity {
    enum class Signature {
        None,
        /** The reader is still waiting for data or verifying */
        Pending,
        /** Signed according to the MIME structure or an inline PGP marker, but no
           verification result is available */
        NotVerified,
        /** Valid signature by a trusted key which belongs to the sender */
        Trusted,
        /** Technically valid signature, but the key is not fully trusted or does
           not belong to the sender */
        Untrusted,
        /** The signature does not match the data, or the key was revoked */
        Bad,
        /** The signer's key is not available */
        UnknownKey,
        /** Verification failed or could not be attempted, e.g. data unavailable,
           malformed or no backend */
        Unverifiable,
    };

    enum class Encryption {
        None,
        /** The reader is still waiting for data or decrypting */
        Pending,
        /** Encrypted according to the MIME structure or an inline PGP marker, but
           not decrypted */
        NotDecrypted,
        Decrypted,
        /** Decryption failed or could not be attempted */
        Failed,
    };

    Signature signature = Signature::None;
    Encryption encryption = Encryption::None;
    /** The signature or encryption was found as inline PGP armor in a text part
     */
    bool inlinePgp = false;
    /** The signer as reported by the verification, if any */
    QString signer;
    QDateTime signDate;
    /** Short and detailed status from the cryptography layer, if any */
    QString summary;
    QString details;

    bool isEmpty() const
    {
        return signature == Signature::None && encryption == Encryption::None;
    }
    bool isPending() const
    {
        return signature == Signature::Pending || encryption == Encryption::Pending;
    }

    /** @short Evaluate the MIME parts below @arg message

    The message is either the root item of a per-message part model such as
    Cryptography::MessageModel::index(0, 0), or a message of the IMAP model.
    Attached messages (message/rfc822) are not evaluated, as their security is not
    the message's.
    */
    static MessageSecurity evaluate(const QModelIndex &message);

    bool operator==(const MessageSecurity &other) const = default;
};

} // namespace UiUtils
