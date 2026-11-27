/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/MailTabs.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace UiUtils {

namespace MailTabs {

namespace {

constexpr std::pair<Region, const char *> regionNames[] = {
    {Region::Header, "header"},
    {Region::Left, "left"},
    {Region::Main, "main"},
    {Region::Right, "right"},
    {Region::Footer, "footer"},
};

}

QString label(const Tab &tab, QChar separator)
{
    if (tab.uid && !tab.subject.trimmed().isEmpty())
        return tab.subject.simplified();
    const QString folder = tab.mailbox.section(separator, -1);
    return folder.isEmpty() ? tab.mailbox : folder;
}

QString toolTip(const Tab &tab, const QString &accountName)
{
    const QString place = QCoreApplication::translate("UiUtils::MailTabs", "%1, %2").arg(accountName, tab.mailbox);
    if (tab.uid && !tab.subject.trimmed().isEmpty())
        return tab.subject.simplified() + QLatin1Char('\n') + place;
    return place;
}

QString configName(Region region)
{
    for (const auto &[value, name] : regionNames) {
        if (value == region)
            return QLatin1String(name);
    }
    return QStringLiteral("header");
}

Region regionFromConfigName(const QString &name)
{
    for (const auto &[value, known] : regionNames) {
        if (name == QLatin1String(known))
            return value;
    }
    return Region::Header;
}

QList<Region> regions()
{
    QList<Region> result;
    for (const auto &[value, name] : regionNames)
        result << value;
    return result;
}

QByteArray serialize(const Session &session)
{
    QJsonArray tabs;
    for (const Tab &tab : session.tabs) {
        QJsonObject object;
        object.insert(QStringLiteral("account"), tab.accountId);
        object.insert(QStringLiteral("mailbox"), tab.mailbox);
        if (tab.uid) {
            object.insert(QStringLiteral("uidValidity"), qint64(tab.uidValidity));
            object.insert(QStringLiteral("uid"), qint64(tab.uid));
            object.insert(QStringLiteral("subject"), tab.subject);
        }
        tabs.append(object);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("current"), session.current);
    root.insert(QStringLiteral("tabs"), tabs);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

std::optional<Session> deserialize(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject() || document.object().value(QStringLiteral("version")).toInt() != 1)
        return std::nullopt;
    const QJsonObject root = document.object();
    Session session;
    for (const QJsonValue &value : root.value(QStringLiteral("tabs")).toArray()) {
        const QJsonObject object = value.toObject();
        Tab tab;
        tab.accountId = object.value(QStringLiteral("account")).toString();
        tab.mailbox = object.value(QStringLiteral("mailbox")).toString();
        tab.uidValidity = uint(object.value(QStringLiteral("uidValidity")).toInteger());
        tab.uid = uint(object.value(QStringLiteral("uid")).toInteger());
        tab.subject = object.value(QStringLiteral("subject")).toString();
        if (tab.accountId.isEmpty() || tab.mailbox.isEmpty())
            continue;
        session.tabs << tab;
    }
    session.current = std::clamp(root.value(QStringLiteral("current")).toInt(), 0,
                                 std::max(0, int(session.tabs.size()) - 1));
    return session;
}

Session restorable(const Session &session, const QStringList &accountIds)
{
    Session result;
    for (int i = 0; i < session.tabs.size(); ++i) {
        if (!accountIds.contains(session.tabs.at(i).accountId))
            continue;
        if (i <= session.current)
            result.current = int(result.tabs.size());
        result.tabs << session.tabs.at(i);
    }
    result.current = std::clamp(result.current, 0, std::max(0, int(result.tabs.size()) - 1));
    return result;
}

Session withoutAccount(const Session &session, const QString &accountId)
{
    QStringList others;
    for (const Tab &tab : session.tabs) {
        if (tab.accountId != accountId && !others.contains(tab.accountId))
            others << tab.accountId;
    }
    return restorable(session, others);
}

Session withRenamedFolder(const Session &session, const QString &accountId, const QString &from, const QString &to,
                          QChar separator)
{
    Session result = session;
    for (Tab &tab : result.tabs) {
        if (tab.accountId != accountId)
            continue;
        if (tab.mailbox == from)
            tab.mailbox = to;
        else if (tab.mailbox.startsWith(from + separator))
            tab.mailbox = to + tab.mailbox.mid(from.size());
    }
    return result;
}

int currentAfterClose(int count, int closed, int current)
{
    if (count <= 1 || closed < 0 || closed >= count)
        return count <= 1 ? -1 : current;
    if (closed < current)
        return current - 1;
    if (closed > current)
        return current;
    // The closed tab was shown: show its right neighbour, or the new last tab
    return std::min(closed, count - 2);
}

}

}
