/* Copyright (C) 2006 - 2017 Jan Kundrát <jkt@kde.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IMAP_MESSAGECOMPOSER_H
#define IMAP_MESSAGECOMPOSER_H

#include <QAbstractListModel>
#include <QPointer>

#include "Composer/AbstractComposer.h"
#include "Composer/ContentDisposition.h"
#include "Imap/Model/CatenateData.h"
#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {
class Model;
}
}

namespace Composer {

class AttachmentItem;

/** @short Model storing individual parts of a composed message */
class MessageComposer : public QAbstractListModel, public AbstractComposer
{
    Q_OBJECT
public:

    explicit MessageComposer(Imap::Mailbox::Model *model);
    ~MessageComposer() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

    void setInReplyTo(const QList<QByteArray> &inReplyTo);
    void setReferences(const QList<QByteArray> &references);
    void setTimestamp(const QDateTime &timestamp);
    void setSubject(const QString &subject);
    void setOrganization(const QString &organization);
    void setText(const QString &text);
    void setReplyingToMessage(const QModelIndex &index);
    void prepareForwarding(const QModelIndex &index, const ForwardMode mode);

    bool isReadyForSerialization() const override;
    bool asRawMessage(QIODevice *target, QString *errorMessage, BccMode bcc = BccMode::Omit) const override;
    bool asCatenateData(QList<Imap::Mailbox::CatenatePair> &target, QString *errorMessage,
                        BccMode bcc = BccMode::Omit) const override;
    void setPreloadEnabled(const bool preload) override;
    void setRecipients(const QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > &recipients) override;
    void setFrom(const Imap::Message::MailAddress &from) override;

    QDateTime timestamp() const override;
    QList<QByteArray> inReplyTo() const;
    QList<QByteArray> references() const;
    QByteArray rawFromAddress() const override;
    QList<QByteArray> rawRecipientAddresses() const override;
    bool hasBlindCopyRecipients() const override;
    QModelIndex replyingToMessage() const override;
    QModelIndex forwardingMessage() const override;

    bool addFileAttachment(const QString &path);
    /** @short Attach the file at @arg source on another computer, once it has been fetched

    A private, empty local copy is created at once, and the attachment shows in the list, unavailable until
    setRemoteFileFetched() reports that the copy is complete. remoteFileAttachmentAdded() asks for it to be fetched,
    unless @arg announce is false because the caller fetches it, as from Nextcloud.
    Returns the copy's path, or an empty string after setting @arg error.
    */
    QString addRemoteFileAttachment(const QUrl &source, QString *error = nullptr, bool announce = true);
    /** @short The local copy at @arg localCopy is complete; returns false when its attachment was removed meanwhile */
    bool setRemoteFileFetched(const QString &localCopy);
    /** @short Remove the attachment whose local copy is @arg localCopy, as when fetching it failed or was cancelled */
    void removeRemoteFileAttachment(const QString &localCopy);
    void removeAttachment(const QModelIndex &index);
    void setAttachmentContentDisposition(const QModelIndex &index, const ContentDisposition disposition);
    void setAttachmentName(const QModelIndex &index, const QString &newName);

    void setReportImpresarioVersions(const bool reportVersion);

signals:
    /** A file on another computer was attached and should be fetched from @arg source into @arg localCopy */
    void remoteFileAttachmentAdded(const QUrl &source, const QString &localCopy);
    /** The attachment fetched into @arg localCopy was removed, so fetching it is no longer needed */
    void remoteFileAttachmentRemoved(const QString &localCopy);

private:
    int remoteFileRow(const QString &localCopy) const;

    static QByteArray encodeHeaderField(const QString &text);

    void ensureRandomStrings() const;
    void writeCommonMessageBeginning(QIODevice *target, BccMode bcc) const;
    bool writeAttachmentHeader(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment) const;
    bool writeAttachmentBody(QIODevice *target, QString *errorMessage, const AttachmentItem *attachment) const;

    void writeHeaderWithMsgIds(QIODevice *target, const QByteArray &headerName, const QList<QByteArray> &messageIds) const;

    bool validateDropImapMessage(QDataStream &stream, QString &mailbox, uint &uidValidity, QList<uint> &uids) const;
    bool validateDropImapPart(QDataStream &stream, QString &mailbox, uint &uidValidity, uint &uid, QByteArray &impresarioPath) const;
    bool dropImapMessage(QDataStream &stream);
    bool dropImapPart(QDataStream &stream);
    bool dropAttachmentList(QDataStream &stream);

    Imap::Message::MailAddress m_from;
    QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > m_recipients;
    QList<QByteArray> m_inReplyTo;
    QList<QByteArray> m_references;
    QDateTime m_timestamp;
    QString m_subject;
    QString m_organization;
    QString m_text;
    QPersistentModelIndex m_replyingTo;
    QPersistentModelIndex m_forwarding;
    mutable QByteArray m_messageId;
    mutable QByteArray m_mimeBoundary;

    QList<AttachmentItem *> m_attachments;
    QPointer<Imap::Mailbox::Model> m_model;
    bool m_shouldPreload;
    bool m_reportImpresarioVersions;
};

}

#endif // IMAP_MESSAGECOMPOSER_H
