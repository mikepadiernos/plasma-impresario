/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/DesktopFeedback.h"

#include <KConfigGroup>

#include <algorithm>
#include <limits>

namespace UiUtils {

namespace DesktopFeedback {

namespace {

const char groupName[] = "DesktopFeedback";
const char unreadCountKey[] = "UnreadCountOnTaskManager";
const QLatin1String launcherEntryPath("/io/github/mikepadiernos/plasma_impresario/UnreadCount");
const QLatin1String launcherEntryInterface("com.canonical.Unity.LauncherEntry");

}

bool showsUnreadCount(const KConfigBase &config)
{
    return config.group(QLatin1String(groupName)).readEntry(unreadCountKey, true);
}

void setShowsUnreadCount(KConfigBase &config, bool enabled)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry(unreadCountKey, enabled);
    group.sync();
}

int totalUnread(const QList<int> &counts)
{
    int total = 0;
    for (const int count : counts) {
        if (count > 0)
            total = count > std::numeric_limits<int>::max() - total ? std::numeric_limits<int>::max() : total + count;
    }
    return total;
}

QString launcherEntryUri(const QString &desktopFileName)
{
    return QLatin1String("application://") + desktopFileName + QLatin1String(".desktop");
}

QVariantMap launcherEntryProperties(int unread)
{
    const int count = std::max(unread, 0);
    return {
        {QStringLiteral("count"), qint64(count)},
        {QStringLiteral("count-visible"), count > 0},
    };
}

QDBusMessage launcherEntryUpdate(const QString &desktopFileName, int unread)
{
    QDBusMessage message = QDBusMessage::createSignal(launcherEntryPath, launcherEntryInterface, QStringLiteral("Update"));
    message << launcherEntryUri(desktopFileName) << launcherEntryProperties(unread);
    return message;
}

bool UnreadBadge::update(int unread)
{
    const int count = std::max(unread, 0);
    if (count == m_shown)
        return false;
    m_shown = count;
    return true;
}

int UnreadBadge::shown() const
{
    return m_shown;
}

void UnreadBadge::reset()
{
    m_shown = -1;
}

}

}
