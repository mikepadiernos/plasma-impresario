/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusMessage>
#include <QList>
#include <QString>
#include <QVariantMap>

class KConfigBase;

namespace UiUtils {

/** @short Feedback on the desktop which never interrupts reading

The application reports state on the desktop only through passive surfaces, which neither take the keyboard focus
nor raise a window. This namespace holds the unread count shown on the application's task manager entry: the unread
messages in the INBOX of every account, published with the com.canonical.Unity.LauncherEntry signal which Plasma's
task manager reads. Only the number leaves the application. It is shown by default and can be turned off.
*/
namespace DesktopFeedback {

/** Whether the task manager entry shows the unread count, as stored in @arg config */
bool showsUnreadCount(const KConfigBase &config);
void setShowsUnreadCount(KConfigBase &config, bool enabled);

/** @short The unread messages in @arg counts, one per account; a negative count is not known yet and is skipped */
int totalUnread(const QList<int> &counts);

/** The application URI which names the task manager entry of the application installed as @arg desktopFileName */
QString launcherEntryUri(const QString &desktopFileName);
/** The properties which show @arg unread on the task manager entry, or hide the count when there is nothing unread */
QVariantMap launcherEntryProperties(int unread);
/** The signal which sends @arg unread to the task manager entry of @arg desktopFileName */
QDBusMessage launcherEntryUpdate(const QString &desktopFileName, int unread);

/** @short The unread count last sent to the task manager, so that it is sent only when it changes */
class UnreadBadge
{
public:
    /** @short Whether @arg unread has to be sent: it is the first count, or differs from the last one sent

    A negative count is the same as nothing unread.
    */
    bool update(int unread);
    /** The count last sent, or -1 when nothing was sent */
    int shown() const;
    /** Forget the count sent, so that the next one is sent again */
    void reset();

private:
    int m_shown = -1;
};

}

}
