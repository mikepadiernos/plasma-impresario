/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_DECRYPTEDTEXTINDEXER_H
#define IMAP_MODEL_DECRYPTEDTEXTINDEXER_H

#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QAbstractItemModel;

namespace Imap
{
namespace Mailbox
{

class SearchIndexer;

/** @short Hands the text of an opened message, once it is decrypted, to the account's search index

It watches the part tree of one message as shown, where decrypting an encrypted part inserts the decrypted parts below
it. The text and HTML parts which arrive below an encrypted part are passed to SearchIndexer::addDecryptedText(),
which ignores them unless the user chose to index decrypted text. Nothing arrives when decryption fails, so nothing is
indexed then.

The object belongs to the part tree, and goes away with it.
*/
class DecryptedTextIndexer : public QObject
{
    Q_OBJECT
public:
    DecryptedTextIndexer(QAbstractItemModel *partModel, SearchIndexer *indexer, const QString &mailbox, uint uid);

    /** @short The readable text of the part @arg index and of the parts below it; attachments are left out */
    static QStringList texts(const QModelIndex &index);

private:
    void partsInserted(const QModelIndex &parent);
    static bool isBelowEncryptedPart(const QModelIndex &index);

    QPointer<SearchIndexer> m_indexer;
    QString m_mailbox;
    uint m_uid;
    QStringList m_sent;
};

}
}

#endif
