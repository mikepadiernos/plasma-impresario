/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "DecryptedTextIndexer.h"

#include <QAbstractItemModel>

#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/SearchIndex.h"
#include "Imap/Model/SearchIndexer.h"

namespace Imap
{
namespace Mailbox
{

DecryptedTextIndexer::DecryptedTextIndexer(QAbstractItemModel *partModel, SearchIndexer *indexer, const QString &mailbox,
                                           uint uid)
    : QObject(partModel)
    , m_indexer(indexer)
    , m_mailbox(mailbox)
    , m_uid(uid)
{
    Q_ASSERT(partModel);
    connect(partModel, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &parent) { partsInserted(parent); });
}

bool DecryptedTextIndexer::isBelowEncryptedPart(const QModelIndex &index)
{
    for (QModelIndex part = index; part.isValid(); part = part.parent()) {
        if (part.data(RolePartDecryptionSupported).toBool())
            return true;
    }
    return false;
}

QStringList DecryptedTextIndexer::texts(const QModelIndex &index)
{
    QStringList result;
    if (!index.isValid())
        return result;
    const QByteArray mimeType = index.data(RolePartMimeType).toByteArray().toLower();
    if (mimeType.startsWith("multipart/")) {
        const QAbstractItemModel *model = index.model();
        for (int row = 0; row < model->rowCount(index); ++row)
            result << texts(model->index(row, 0, index));
        return result;
    }
    // An attached message stays out, as for the messages which are not encrypted
    if (mimeType.startsWith("message/"))
        return result;
    if (mimeType != "text/plain" && mimeType != "text/html")
        return result;
    if (index.data(RolePartBodyDisposition).toByteArray().toLower() == "attachment")
        return result;
    const QString text = decodeByteArray(index.data(RolePartData).toByteArray(), index.data(RolePartCharset).toByteArray());
    const QString readable = mimeType == "text/html" ? SearchIndex::plainTextOfHtml(text) : text;
    if (!readable.trimmed().isEmpty())
        result << readable;
    return result;
}

void DecryptedTextIndexer::partsInserted(const QModelIndex &parent)
{
    if (!m_indexer || !m_indexer->indexesDecrypted() || !isBelowEncryptedPart(parent))
        return;
    const QAbstractItemModel *model = parent.model();
    QStringList found;
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QStringList partTexts = texts(model->index(row, 0, parent));
        for (const QString &text : partTexts) {
            if (!m_sent.contains(text) && !found.contains(text))
                found << text;
        }
    }
    if (found.isEmpty())
        return;
    m_sent << found;
    m_indexer->addDecryptedText(m_mailbox, m_uid, found.join(QLatin1Char('\n')));
}

}
}
