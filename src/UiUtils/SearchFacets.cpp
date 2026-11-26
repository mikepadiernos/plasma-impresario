/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchFacets.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <algorithm>

#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"
#include "UiUtils/RelatedMessages.h"

namespace UiUtils {

namespace {

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::SearchFacets", text, comment);
}

/** @short Flags which the user does not see as tags, as GroupingMsgListModel leaves them out */
bool isSystemFlag(const QString &flag)
{
    if (flag.isEmpty() || flag.startsWith(QLatin1Char('\\')))
        return true;
    static const QStringList special{
        QStringLiteral("$forwarded"), QStringLiteral("$junk"), QStringLiteral("$notjunk"), QStringLiteral("$mdnsent"),
        QStringLiteral("$submitted"), QStringLiteral("$submitpending"), QStringLiteral("junk"), QStringLiteral("nonjunk"),
    };
    return special.contains(flag.toLower());
}

/** @short "Name <address>" of the first address, or just the address */
QString senderLabel(const QVariant &addresses, const QString &address)
{
    const QVariantList list = addresses.toList();
    const QStringList parts = list.isEmpty() ? QStringList() : list.constFirst().toStringList();
    const QString name = parts.isEmpty() ? QString() : parts.constFirst().simplified();
    return name.isEmpty() ? address : QStringLiteral("%1 <%2>").arg(name, address);
}

/** @short Order of the date values, newest first */
int dateOrder(const QString &key)
{
    static const QStringList order{
        QStringLiteral("future"), QStringLiteral("today"), QStringLiteral("yesterday"), QStringLiteral("thisWeek"),
        QStringLiteral("lastWeek"), QStringLiteral("thisMonth"), QStringLiteral("thisYear"), QStringLiteral("older"),
    };
    const int position = order.indexOf(key);
    return position < 0 ? order.size() : position;
}

SearchFacets::Value dateValue(const QDate &date, const QDate &today)
{
    using Value = SearchFacets::Value;
    const auto value = [](const QString &key, const QString &label) {
        return Value{SearchFacets::Kind::Date, key, label};
    };
    const QDate weekStart = today.addDays(-((today.dayOfWeek() - QLocale().firstDayOfWeek() + 7) % 7));
    if (date > today)
        return value(QStringLiteral("future"), tr("Future"));
    if (date == today)
        return value(QStringLiteral("today"), tr("Today"));
    if (date == today.addDays(-1))
        return value(QStringLiteral("yesterday"), tr("Yesterday"));
    if (date >= weekStart)
        return value(QStringLiteral("thisWeek"), tr("Earlier This Week"));
    if (date >= weekStart.addDays(-7))
        return value(QStringLiteral("lastWeek"), tr("Last Week"));
    if (date >= QDate(today.year(), today.month(), 1))
        return value(QStringLiteral("thisMonth"), tr("Earlier This Month"));
    if (date >= QDate(today.year(), 1, 1))
        return value(QStringLiteral("thisYear"), tr("Earlier This Year"));
    return value(QStringLiteral("older"), tr("Older"));
}

}

QList<SearchFacets::Kind> SearchFacets::kinds()
{
    return {Kind::Account, Kind::Folder, Kind::Sender, Kind::Date, Kind::Attachments, Kind::Tag};
}

QString SearchFacets::kindName(const Kind kind)
{
    switch (kind) {
    case Kind::Account:
        return tr("Account", "search facet");
    case Kind::Folder:
        return tr("Folder", "search facet");
    case Kind::Sender:
        return tr("Sender", "search facet");
    case Kind::Date:
        return tr("Date", "search facet");
    case Kind::Attachments:
        return tr("Attachments", "search facet");
    case Kind::Tag:
        return tr("Tag", "search facet");
    }
    return QString();
}

QList<SearchFacets::Value> SearchFacets::valuesOf(const QModelIndex &message, const Kind kind, const Context &context)
{
    using namespace Imap::Mailbox;
    const QVariant fetched = message.data(RoleIsFetched);
    // Placeholders of messages missing from a thread, and group headers
    if (!fetched.isValid() || !message.data(RoleMessageUid).toUInt())
        return {};
    switch (kind) {
    case Kind::Account: {
        const QVariant id = message.data(RoleMessageAccountId);
        if (id.isValid())
            return {Value{kind, id.toString(), message.data(RoleMessageAccountName).toString()}};
        if (context.accountId.isEmpty() && context.accountName.isEmpty())
            return {};
        return {Value{kind, context.accountId, context.accountName}};
    }
    case Kind::Folder: {
        QString mailbox = message.data(RoleMailboxName).toString();
        // Models which do not forward the role have the message below its list, below its mailbox
        if (mailbox.isEmpty())
            mailbox = Imap::deproxifiedIndex(message).parent().parent().data(RoleMailboxName).toString();
        if (mailbox.isEmpty())
            return {};
        const QVariant account = message.data(RoleMessageAccountId);
        const QString accountId = account.isValid() ? account.toString() : context.accountId;
        QString label = message.data(RoleMessageSourceLabel).toString();
        if (label.isEmpty())
            label = mailbox.compare(QLatin1String("INBOX"), Qt::CaseInsensitive) == 0 ? tr("Inbox") : mailbox;
        return {Value{kind, accountId + QLatin1Char('\n') + mailbox, label}};
    }
    case Kind::Tag: {
        // Flags are known without downloading the message
        QList<Value> values;
        const QStringList flags = message.data(RoleMessageFlags).toStringList();
        for (const QString &flag : flags) {
            if (isSystemFlag(flag))
                continue;
            const Value value{kind, flag.toCaseFolded(), flag};
            if (!values.contains(value))
                values << value;
        }
        return values;
    }
    default:
        break;
    }
    // Asking for the envelope or the structure of a message which is not loaded would download it
    if (!fetched.toBool())
        return {};
    switch (kind) {
    case Kind::Sender: {
        const QVariant from = message.data(RoleMessageFrom);
        const QString address = firstAddress(from);
        if (address.isEmpty())
            return {Value{kind, QString(), tr("Unknown Sender")}};
        return {Value{kind, address, senderLabel(from, address)}};
    }
    case Kind::Date: {
        const QDateTime dateTime = message.data(RoleMessageDate).toDateTime();
        if (!dateTime.isValid())
            return {Value{kind, QStringLiteral("none"), tr("No Date")}};
        const QDate today = context.today.isValid() ? context.today : QDate::currentDate();
        return {dateValue(dateTime.toLocalTime().date(), today)};
    }
    case Kind::Attachments:
        if (message.data(RoleMessageHasAttachments).toBool())
            return {Value{kind, QStringLiteral("with"), tr("With Attachments")}};
        return {Value{kind, QStringLiteral("without"), tr("Without Attachments")}};
    default:
        return {};
    }
}

void SearchFacets::setContext(const Context &context)
{
    m_context = context;
}

SearchFacets::Context SearchFacets::context() const
{
    return m_context;
}

QList<SearchFacets::Value> SearchFacets::chosen() const
{
    return m_chosen;
}

bool SearchFacets::isEmpty() const
{
    return m_chosen.isEmpty();
}

bool SearchFacets::contains(const Value &value) const
{
    return m_chosen.contains(value);
}

void SearchFacets::toggle(const Value &value)
{
    if (!m_chosen.removeOne(value))
        m_chosen << value;
}

void SearchFacets::remove(const Value &value)
{
    m_chosen.removeOne(value);
}

void SearchFacets::clear()
{
    m_chosen.clear();
}

bool SearchFacets::matches(const QModelIndex &message) const
{
    return matchesExcept(message, Kind::Account, false);
}

bool SearchFacets::matchesExcept(const QModelIndex &message, const Kind ignored, const bool ignoring) const
{
    for (const Kind kind : kinds()) {
        if (ignoring && kind == ignored)
            continue;
        bool chosenKind = false;
        bool found = false;
        QList<Value> values;
        bool valuesRead = false;
        for (const Value &chosen : m_chosen) {
            if (chosen.kind != kind)
                continue;
            chosenKind = true;
            if (!valuesRead) {
                values = valuesOf(message, kind, m_context);
                valuesRead = true;
            }
            if (values.contains(chosen)) {
                found = true;
                break;
            }
        }
        if (chosenKind && !found)
            return false;
    }
    return true;
}

QList<SearchFacets::Count> SearchFacets::counts(const QModelIndexList &messages, const Kind kind) const
{
    QList<Count> result;
    QHash<QString, int> positions;
    const auto add = [&result, &positions](const Value &value, const int count) {
        auto it = positions.constFind(value.key);
        if (it == positions.cend()) {
            it = positions.insert(value.key, result.size());
            result << Count{value, 0};
        }
        result[*it].count += count;
    };
    for (const Value &chosen : m_chosen) {
        if (chosen.kind == kind)
            add(chosen, 0);
    }
    for (const QModelIndex &message : messages) {
        if (!matchesExcept(message, kind, true))
            continue;
        const QList<Value> values = valuesOf(message, kind, m_context);
        for (const Value &value : values)
            add(value, 1);
    }
    const auto byLabel = [](const Count &left, const Count &right) {
        return QString::localeAwareCompare(left.value.label, right.value.label) < 0;
    };
    switch (kind) {
    case Kind::Date:
        std::stable_sort(result.begin(), result.end(), [](const Count &left, const Count &right) {
            return dateOrder(left.value.key) < dateOrder(right.value.key);
        });
        break;
    case Kind::Attachments:
        std::stable_sort(result.begin(), result.end(), [](const Count &left, const Count &right) {
            return left.value.key == QLatin1String("with") && right.value.key != QLatin1String("with");
        });
        break;
    default:
        std::stable_sort(result.begin(), result.end(), [&byLabel](const Count &left, const Count &right) {
            if (left.count != right.count)
                return left.count > right.count;
            return byLabel(left, right);
        });
        break;
    }
    return result;
}

}
