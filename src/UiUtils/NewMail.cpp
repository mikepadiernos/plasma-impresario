/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/NewMail.h"

#include <QCoreApplication>
#include <algorithm>

namespace UiUtils {

namespace NewMail {

namespace {

QString tr(const char *text, const char *comment = nullptr, int n = -1)
{
    return QCoreApplication::translate("UiUtils::NewMail", text, comment, n);
}

bool isInbox(const QString &folder)
{
    return folder.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0;
}

QString folderKey(const QString &folder)
{
    return isInbox(folder) ? QStringLiteral("INBOX") : folder;
}

/** The last part of a folder's name, for a short line in a notification */
QString shortFolder(const QString &folder)
{
    if (isInbox(folder))
        return tr("Inbox");
    const qsizetype at = std::max(folder.lastIndexOf(QLatin1Char('/')), folder.lastIndexOf(QLatin1Char('.')));
    return at >= 0 && at + 1 < folder.size() ? folder.mid(at + 1) : folder;
}

}

Mode Settings::mode(const QString &accountId) const
{
    return accountModes.value(accountId, Mode::Each);
}

bool Settings::announces(const QString &accountId, const QString &folder) const
{
    const auto own = folders.value(accountId);
    const auto found = own.constFind(folderKey(folder));
    if (found != own.constEnd())
        return found.value();
    return isInbox(folder);
}

bool Settings::hasFolderSetting(const QString &accountId, const QString &folder) const
{
    return folders.value(accountId).contains(folderKey(folder));
}

void Settings::setAnnounces(const QString &accountId, const QString &folder, bool announce)
{
    QHash<QString, bool> &own = folders[accountId];
    if (announce == isInbox(folder))
        own.remove(folderKey(folder));
    else
        own.insert(folderKey(folder), announce);
    if (own.isEmpty())
        folders.remove(accountId);
}

QStringList Settings::extraFolders(const QString &accountId) const
{
    QStringList result;
    const auto own = folders.value(accountId);
    for (auto it = own.constBegin(); it != own.constEnd(); ++it) {
        if (it.value() && !isInbox(it.key()))
            result.append(it.key());
    }
    std::sort(result.begin(), result.end());
    return result;
}

Skip classify(const MailRules::Message &message, const Settings &settings, const QDateTime &lastCheck,
              const QStringList &readingIds, bool ruleNotify)
{
    if (settings.mode(message.accountId) == Mode::Off)
        return Skip::AccountOff;
    if (!ruleNotify && !settings.announces(message.accountId, message.folder))
        return Skip::FolderQuiet;
    if (!message.unread)
        return Skip::Read;
    // INTERNALDATE survives COPY and MOVE, so mail moved in from elsewhere looks as old as it is
    if (lastCheck.isValid() && message.received.isValid() && message.received < lastCheck.addSecs(-graceSeconds))
        return Skip::Old;
    if (!message.messageId.isEmpty() && readingIds.contains(message.messageId))
        return Skip::Reading;
    for (const QString &parent : message.inReplyTo) {
        if (readingIds.contains(parent))
            return Skip::Reading;
    }
    return Skip::None;
}

Delivery delivery(Mode mode, bool windowActive, bool quietWhileActive, const QDateTime &lastShown, const QDateTime &now,
                  int summaryMinutes)
{
    if (mode == Mode::Off)
        return Delivery::Never;
    if (windowActive && quietWhileActive)
        return Delivery::Hold;
    if (mode == Mode::Summary && nextSummary(lastShown, now, summaryMinutes).isValid())
        return Delivery::Hold;
    return Delivery::Now;
}

QDateTime nextSummary(const QDateTime &lastShown, const QDateTime &now, int summaryMinutes)
{
    if (!lastShown.isValid())
        return QDateTime();
    const QDateTime due = lastShown.addSecs(std::max(1, summaryMinutes) * 60);
    return due > now ? due : QDateTime();
}

bool Digest::add(const QString &accountId, const QList<Entry> &entries)
{
    bool added = false;
    Account &account = m_accounts[accountId];
    for (const Entry &entry : entries) {
        const bool known = std::any_of(account.entries.cbegin(), account.entries.cend(), [&entry](const Entry &other) {
            return other.uid == entry.uid && folderKey(other.folder) == folderKey(entry.folder);
        });
        if (known)
            continue;
        account.entries.append(entry);
        added = true;
    }
    if (account.entries.isEmpty() && account.counts.isEmpty())
        m_accounts.remove(accountId);
    return added;
}

bool Digest::addCount(const QString &accountId, const QString &folder, int count)
{
    if (count <= 0)
        return false;
    m_accounts[accountId].counts[folderKey(folder)] += count;
    return true;
}

bool Digest::remove(const QString &accountId, const QString &folder, uint uid)
{
    auto found = m_accounts.find(accountId);
    if (found == m_accounts.end())
        return false;
    const qsizetype removed = found->entries.removeIf([&](const Entry &entry) {
        return entry.uid == uid && folderKey(entry.folder) == folderKey(folder);
    });
    if (found->entries.isEmpty() && found->counts.isEmpty())
        m_accounts.erase(found);
    return removed > 0;
}

void Digest::clear(const QString &accountId)
{
    m_accounts.remove(accountId);
}

bool Digest::clearCount(const QString &accountId, const QString &folder)
{
    auto found = m_accounts.find(accountId);
    if (found == m_accounts.end() || !found->counts.remove(folderKey(folder)))
        return false;
    if (found->entries.isEmpty() && found->counts.isEmpty())
        m_accounts.erase(found);
    return true;
}

QList<Entry> Digest::entries(const QString &accountId) const
{
    return m_accounts.value(accountId).entries;
}

int Digest::count(const QString &accountId) const
{
    const Account account = m_accounts.value(accountId);
    int total = static_cast<int>(account.entries.size());
    for (int counted : account.counts)
        total += counted;
    return total;
}

bool Digest::isEmpty(const QString &accountId) const
{
    return count(accountId) == 0;
}

QStringList Digest::accounts() const
{
    QStringList result = m_accounts.keys();
    std::sort(result.begin(), result.end());
    return result;
}

Text Digest::text(const QString &accountId, const QString &accountName, bool showContent) const
{
    Text text;
    const Account account = m_accounts.value(accountId);
    text.count = count(accountId);
    if (text.count == 0)
        return text;
    text.single = account.entries.size() == 1 && account.counts.isEmpty();
    text.title = text.count == 1 ? tr("New message in %1", "%1 account name").arg(accountName)
                                 : tr("%n new messages in %1", "%1 account name", text.count).arg(accountName);

    QList<Entry> newest = account.entries;
    std::stable_sort(newest.begin(), newest.end(), [](const Entry &a, const Entry &b) { return a.received > b.received; });
    QStringList lines;
    if (showContent) {
        for (const Entry &entry : newest) {
            if (lines.size() == listedMessages)
                break;
            const QString sender = entry.sender.isEmpty() ? tr("Unknown sender") : entry.sender;
            const QString subject = entry.subject.isEmpty() ? tr("(no subject)") : entry.subject;
            lines.append(tr("%1: %2", "a new message: %1 sender, %2 subject").arg(sender, subject));
        }
        const int rest = static_cast<int>(newest.size()) - static_cast<int>(lines.size());
        if (rest > 0)
            lines.append(tr("and %n more", "further new messages not listed", rest));
    } else {
        QHash<QString, int> perFolder;
        for (const Entry &entry : newest)
            ++perFolder[folderKey(entry.folder)];
        QStringList names = perFolder.keys();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            return isInbox(a) != isInbox(b) ? isInbox(a) : a < b;
        });
        for (const QString &folder : names)
            lines.append(tr("%n in %1", "new messages in a folder", perFolder.value(folder)).arg(shortFolder(folder)));
    }
    QStringList counted = account.counts.keys();
    std::sort(counted.begin(), counted.end());
    for (const QString &folder : counted)
        lines.append(tr("%n new in %1", "new messages in a folder, known only by their number", account.counts.value(folder))
                         .arg(shortFolder(folder)));
    text.body = lines.join(QLatin1Char('\n'));
    return text;
}

}

}
