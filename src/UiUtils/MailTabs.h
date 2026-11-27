/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace UiUtils {

/** @short The optional mail tabs: what each tab shows, where the tab bar goes and how tabs are restored

A tab remembers a folder of an account and, when a message was open, that message. Tabs hold no drafts or other
work: messages are written in their own windows, so closing a tab or turning tabs off loses nothing.
*/
namespace MailTabs {

struct Tab {
    QString accountId;
    QString mailbox;
    uint uidValidity = 0;
    /** The open message, or 0 for the folder alone */
    uint uid = 0;
    /** The subject of the open message, shown as the label */
    QString subject;

    bool operator==(const Tab &other) const = default;
};

/** @short The label of @arg tab: the message's subject, or the folder's last part */
QString label(const Tab &tab, QChar separator = QLatin1Char('/'));
/** @short The tooltip of @arg tab, naming the account and the full folder */
QString toolTip(const Tab &tab, const QString &accountName);

/** @short The shell regions which can hold the tab bar */
enum class Region {
    Header,
    Left,
    Main,
    Right,
    Footer,
};
QString configName(Region region);
/** @short The region called @arg name; the header for an unknown name */
Region regionFromConfigName(const QString &name);
QList<Region> regions();

struct Session {
    QList<Tab> tabs;
    int current = 0;

    bool operator==(const Session &other) const = default;
};

QByteArray serialize(const Session &session);
/** @short The session in @arg data; nothing for data which is not a session */
std::optional<Session> deserialize(const QByteArray &data);

/** @short @arg session without the tabs of accounts which no longer exist, the current tab kept where possible */
Session restorable(const Session &session, const QStringList &accountIds);
/** @short @arg session without the tabs of @arg accountId, as when the account is removed */
Session withoutAccount(const Session &session, const QString &accountId);
/** @short @arg session with @arg from and its subfolders renamed to @arg to in @arg accountId */
Session withRenamedFolder(const Session &session, const QString &accountId, const QString &from, const QString &to,
                          QChar separator);

/** @short The tab to show after closing tab @arg closed of @arg count while tab @arg current was shown

The index is counted after the tab is gone; -1 when no tab is left.
*/
int currentAfterClose(int count, int closed, int current);

}

}
