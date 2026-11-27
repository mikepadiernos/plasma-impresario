/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailStatus.h"

#include <KConfigGroup>

#include <algorithm>

#include "Imap/Model/SearchIndex.h"
#include "UiUtils/PlasmaSearch.h"

namespace UiUtils {

namespace MailStatus {

namespace {

const char groupName[] = "PlasmaWidgets";
const char sharingKey[] = "Share";

bool isInbox(const QString &folder)
{
    return folder.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0;
}

} // namespace

Sharing sharing(const KConfigBase &config)
{
    const QString stored = config.group(QLatin1String(groupName)).readEntry(sharingKey, QString());
    return stored.isEmpty() ? Sharing::Counts : sharingFromName(stored);
}

void setSharing(KConfigBase &config, Sharing sharing)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(sharingKey, sharingName(sharing));
    group.sync();
}

QString sharingName(Sharing sharing)
{
    switch (sharing) {
    case Sharing::Nothing:
        return QStringLiteral("nothing");
    case Sharing::Counts:
        return QStringLiteral("counts");
    case Sharing::Senders:
        return QStringLiteral("senders");
    case Sharing::Subjects:
        return QStringLiteral("subjects");
    }
    return QStringLiteral("nothing");
}

Sharing sharingFromName(const QString &name)
{
    for (Sharing candidate : {Sharing::Counts, Sharing::Senders, Sharing::Subjects}) {
        if (name == sharingName(candidate))
            return candidate;
    }
    // An unknown value shares nothing rather than guessing more
    return Sharing::Nothing;
}

QString stateName(State state)
{
    switch (state) {
    case State::Idle:
        return QStringLiteral("idle");
    case State::Connecting:
        return QStringLiteral("connecting");
    case State::Online:
        return QStringLiteral("online");
    case State::Offline:
        return QStringLiteral("offline");
    case State::SignInNeeded:
        return QStringLiteral("signInNeeded");
    case State::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

QVariantMap toMap(const Account &account, Sharing sharing)
{
    if (sharing == Sharing::Nothing)
        return {};
    return {
        {QStringLiteral("id"), account.id},
        {QStringLiteral("name"), account.name},
        {QStringLiteral("unread"), account.unread < 0 ? -1 : account.unread},
        {QStringLiteral("state"), stateName(account.state)},
    };
}

QVariantMap toMap(const Message &message, Sharing sharing)
{
    if (sharing != Sharing::Senders && sharing != Sharing::Subjects)
        return {};
    QVariantMap map{
        {QStringLiteral("id"), message.id},
        {QStringLiteral("account"), message.accountId},
        {QStringLiteral("accountName"), message.accountName},
        {QStringLiteral("folder"), message.folder},
        {QStringLiteral("sender"), message.sender},
        // ISO 8601, or empty without a date, so that the widgets read it without
        // knowing D-Bus date types
        {QStringLiteral("sent"), message.sent.isValid() ? message.sent.toString(Qt::ISODate) : QString()},
    };
    if (sharing == Sharing::Subjects)
        map.insert(QStringLiteral("subject"), message.subject);
    return map;
}

bool isChosen(const QStringList &filter, const QString &value, bool isFolder)
{
    if (filter.isEmpty())
        return true;
    return std::any_of(filter.cbegin(), filter.cend(), [&value, isFolder](const QString &chosen) {
        return chosen == value || (isFolder && isInbox(chosen) && isInbox(value));
    });
}

QList<Message> unreadMessages(
    const QList<Source> &sources, const QStringList &accounts, const QStringList &folders, int limit)
{
    QList<Message> result;
    limit = std::min(limit, maximumMessages);
    if (limit <= 0)
        return result;
    // The index stores the INBOX under its canonical name
    QStringList mailboxes;
    for (const QString &folder : folders) {
        const QString name = isInbox(folder) ? QStringLiteral("INBOX") : folder;
        if (!name.isEmpty() && !mailboxes.contains(name))
            mailboxes << name;
    }
    if (!folders.isEmpty() && mailboxes.isEmpty())
        return result;
    for (const Source &source : sources) {
        if (!source.index || !source.index->isOpen() || !isChosen(accounts, source.accountId))
            continue;
        const auto found = source.index->unreadMessages(mailboxes, limit);
        for (const auto &summary : found) {
            if (!summary.location.uidValidity)
                continue;
            Message message;
            message.id = PlasmaSearch::matchId(
                {source.accountId, summary.location.mailbox, summary.location.uidValidity, summary.location.uid});
            message.accountId = source.accountId;
            message.accountName = source.accountName;
            message.folder = summary.location.mailbox;
            message.sender = summary.sender;
            message.subject = summary.subject;
            message.sent = summary.sent;
            result << message;
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const Message &a, const Message &b) {
        if (a.sent.isValid() != b.sent.isValid())
            return a.sent.isValid();
        return a.sent > b.sent;
    });
    if (result.size() > limit)
        result.resize(limit);
    return result;
}

} // namespace MailStatus

} // namespace UiUtils
