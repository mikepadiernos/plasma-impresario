/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/NewMailWatcher.h"

#include <QTimer>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"

namespace UiUtils {

namespace {

/** "Name <mailbox@host>", or the address alone, for each address in a RoleMessageFrom-style list */
QStringList addresses(const QVariant &list)
{
    QStringList result;
    const QVariantList items = list.toList();
    for (const QVariant &item : items) {
        const QStringList fields = item.toStringList();
        if (fields.size() < 4)
            continue;
        const QString address = fields.at(3).isEmpty() ? fields.at(2) : fields.at(2) + QLatin1Char('@') + fields.at(3);
        result << (fields.at(0).isEmpty() ? address : QStringLiteral("%1 <%2>").arg(fields.at(0), address));
    }
    return result;
}

}

NewMailWatcher::NewMailWatcher(Imap::Mailbox::Model *model, const QString &accountId, const QString &mailbox,
                               QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_accountId(accountId)
    , m_mailbox(mailbox)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(0);
    connect(m_timer, &QTimer::timeout, this, &NewMailWatcher::check);
    if (!model)
        return;
    connect(model, &QAbstractItemModel::rowsInserted, this, &NewMailWatcher::schedule);
    connect(model, &QAbstractItemModel::dataChanged, this, &NewMailWatcher::schedule);
    connect(model, &QAbstractItemModel::layoutChanged, this, &NewMailWatcher::schedule);
    connect(model, &QAbstractItemModel::modelReset, this, &NewMailWatcher::schedule);
    schedule();
}

NewMailWatcher::~NewMailWatcher() = default;

QString NewMailWatcher::accountId() const
{
    return m_accountId;
}

QString NewMailWatcher::mailbox() const
{
    return m_mailbox;
}

void NewMailWatcher::setOpensMailbox(bool opens)
{
    if (m_opensMailbox == opens)
        return;
    m_opensMailbox = opens;
    if (opens)
        schedule();
}

bool NewMailWatcher::opensMailbox() const
{
    return m_opensMailbox;
}

void NewMailWatcher::setBaseline(const Baseline &baseline)
{
    m_baseline = baseline;
    m_pending.clear();
    schedule();
}

NewMailWatcher::Baseline NewMailWatcher::baseline() const
{
    return m_baseline;
}

void NewMailWatcher::schedule()
{
    if (!m_timer->isActive())
        m_timer->start();
}

QModelIndex NewMailWatcher::messageList() const
{
    if (!m_model)
        return {};
    const QModelIndex mailbox = m_model->mailboxIndexByName(m_mailbox);
    if (!mailbox.isValid())
        return {};
    return m_model->index(0, 0, mailbox);
}

void NewMailWatcher::check()
{
    const QModelIndex list = messageList();
    if (!list.isValid())
        return;
    // Counting the messages selects the folder on the account's connection
    if (!m_opensMailbox && !list.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;
    const int rows = m_model->rowCount(list);
    if (!list.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;
    const uint uidValidity = list.parent().data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    if (!uidValidity)
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    uint highest = 0;
    QList<QModelIndex> fresh;
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_model->index(row, 0, list);
        const uint uid = index.data(Imap::Mailbox::RoleMessageUid).toUInt();
        if (!uid)
            continue;
        highest = std::max(highest, uid);
        if (m_baseline.uidValidity == uidValidity && (uid > m_baseline.highestUid || m_pending.contains(uid)))
            fresh << index;
    }

    if (m_baseline.uidValidity != uidValidity) {
        // A folder seen for the first time, or renumbered by the server: what is there is not news
        m_baseline = {uidValidity, highest, now};
        m_pending.clear();
        emit baselineChanged();
        return;
    }

    if (!fresh.isEmpty() && m_pending.isEmpty())
        m_pendingSince = m_baseline.lastCheck;
    const QDateTime since = m_pending.isEmpty() ? m_baseline.lastCheck : m_pendingSince;

    QList<MailRules::Message> ready;
    QSet<uint> waiting;
    for (const QModelIndex &index : std::as_const(fresh)) {
        const uint uid = index.data(Imap::Mailbox::RoleMessageUid).toUInt();
        // Asking for the subject loads the envelope, the flags and the structure
        index.data(Imap::Mailbox::RoleMessageSubject);
        if (index.data(Imap::Mailbox::RoleIsFetched).toBool())
            ready << message(index, m_accountId, m_mailbox);
        else
            waiting.insert(uid);
    }
    m_pending = waiting;

    const bool changed = highest > m_baseline.highestUid;
    m_baseline.highestUid = std::max(m_baseline.highestUid, highest);
    // A list loaded before is not updated while the connection shows another folder, so it was not checked then
    if (m_model->isMailboxOpen(list.parent()))
        m_baseline.lastCheck = now;
    if (!ready.isEmpty())
        emit arrived(ready, since);
    if (changed)
        emit baselineChanged();
}

MailRules::Message NewMailWatcher::message(const QModelIndex &index, const QString &accountId, const QString &mailbox)
{
    using namespace Imap::Mailbox;
    MailRules::Message result;
    result.accountId = accountId;
    result.folder = mailbox;
    result.uid = index.data(RoleMessageUid).toUInt();
    result.senders = addresses(index.data(RoleMessageFrom));
    result.recipients = addresses(index.data(RoleMessageTo)) + addresses(index.data(RoleMessageCc));
    result.subject = index.data(RoleMessageSubject).toString();
    result.hasAttachment = index.data(RoleMessageHasAttachments).toBool();
    result.flags = index.data(RoleMessageFlags).toStringList();
    result.received = index.data(RoleMessageInternalDate).toDateTime();
    result.messageId = QString::fromUtf8(index.data(RoleMessageMessageId).toByteArray());
    const QList<QByteArray> inReplyTo = index.data(RoleMessageInReplyTo).value<QList<QByteArray>>();
    for (const QByteArray &id : inReplyTo)
        result.inReplyTo << QString::fromUtf8(id);
    result.unread = !index.data(RoleMessageIsMarkedRead).toBool();
    return result;
}

}
