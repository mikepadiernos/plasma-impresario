/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class KConfigBase;

namespace Imap {
namespace Mailbox {
class SearchIndex;
}
} // namespace Imap

namespace UiUtils {

/** @short What the optional Plasma widgets may learn about the mail

The widgets run in Plasma, outside the application, and ask it over D-Bus; they never read the mail storage or the
credentials. This namespace holds the setting which limits what they are told, the records they receive and the query
for the unread messages. By default the widgets learn the unread counts and the state of each account and nothing about
the messages; senders, and senders with subjects, are shared only once the user chooses so. Message text is never
shared. The D-Bus side lives in IPC, and the text shown in the GUI.
*/
namespace MailStatus {

/** @short What the widgets are told, each level including the one before */
enum class Sharing {
    /** Nothing; the widgets say that the application does not share its status */
    Nothing,
    /** The unread count and the state of each account */
    Counts,
    /** Also the sender, folder and date of unread messages */
    Senders,
    /** Also their subjects */
    Subjects,
};

/** The sharing chosen in @arg config; Counts when nothing is stored */
Sharing sharing(const KConfigBase &config);
void setSharing(KConfigBase &config, Sharing sharing);

/** The name the widgets read: "nothing", "counts", "senders" or "subjects" */
QString sharingName(Sharing sharing);
/** The sharing named @arg name, or Nothing when @arg name is not one of sharingName() */
Sharing sharingFromName(const QString &name);

/** @short The state of an account, as the widgets show it */
enum class State {
    /** Not connected yet in this session */
    Idle,
    Connecting,
    Online,
    /** Offline by choice or without a network */
    Offline,
    /** The password or the sign-in is needed */
    SignInNeeded,
    /** The last attempt failed */
    Failed,
};

/** The name the widgets read: "idle", "connecting", "online", "offline", "signInNeeded" or "failed" */
QString stateName(State state);

struct Account {
    QString id;
    QString name;
    /** Unread messages in the INBOX; negative while not known */
    int unread = -1;
    State state = State::Idle;
};

/** @short An unread message, as the widgets may show it */
struct Message {
    /** The ID which opens the message, as PlasmaSearch::matchId() makes it */
    QString id;
    QString accountId;
    QString accountName;
    QString folder;
    QString sender;
    QString subject;
    /** Day of the Date header; invalid when the message has none */
    QDate sent;
};

/** The most unread messages given for one request */
constexpr int maximumMessages = 50;

/** @short The entry sent for @arg account, or an empty map when @arg sharing is Nothing */
QVariantMap toMap(const Account &account, Sharing sharing);
/** @short The entry sent for @arg message, with the sender and the subject only when @arg sharing allows them

Empty when @arg sharing is below Senders.
*/
QVariantMap toMap(const Message &message, Sharing sharing);

/** @short Whether @arg value is chosen by @arg filter; an empty filter chooses everything

Names of the INBOX match in any case, as IMAP treats them.
*/
bool isChosen(const QStringList &filter, const QString &value, bool isFolder = false);

/** @short An account whose unread messages are asked for, with its open index */
struct Source {
    QString accountId;
    QString accountName;
    const Imap::Mailbox::SearchIndex *index = nullptr;
};

/** @short The newest unread messages of the chosen @arg accounts in the chosen @arg folders, at most @arg limit

Empty filters choose every account and every indexed folder. Each account gives its newest messages, and of them all the
newest are kept; messages whose folder has no known UIDVALIDITY are left out, as they could not be opened.
*/
QList<Message> unreadMessages(
    const QList<Source> &sources, const QStringList &accounts, const QStringList &folders, int limit = maximumMessages);

} // namespace MailStatus

} // namespace UiUtils
