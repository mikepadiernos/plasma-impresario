/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "UiUtils/MailRules.h"

namespace UiUtils {

/** @short Which new mail is announced, how and when

Each account has at most one new-mail notification, which is updated as mail arrives instead of adding another one; see
Digest. A message counts as new mail when it arrives unread in a folder whose mail is announced. Mail which was received
before the folder was last checked is old mail moved into the folder and is not announced, nor is a reply in the
conversation the user is reading.

The announcements of an account can come one notification at a time (Mode::Each), as one summary at most every few
minutes (Mode::Summary) or not at all (Mode::Off). While the window is active, announcements wait until it is left, so
that focused work is not interrupted; the window's list and the task manager's count show the new mail meanwhile.

The INBOX of every account is announced unless it is turned off; other folders only when they are turned on. A rule's
Notify action announces a message in any folder of an account whose announcements are not off.
*/
namespace NewMail {

enum class Mode {
    /** One notification per account, updated as mail arrives */
    Each,
    /** One summary at most every Settings::summaryMinutes */
    Summary,
    /** Nothing is announced */
    Off,
};

struct Settings {
    /** The mode of each account; Mode::Each when not set */
    QHash<QString, Mode> accountModes;
    /** Folders whose announcement differs from the default, per account: true to announce, false to keep quiet */
    QHash<QString, QHash<QString, bool>> folders;
    /** Whether announcements wait while the window is active */
    bool quietWhileActive = true;
    /** Whether notifications show senders and subjects; otherwise only counts and folders */
    bool showContent = true;
    int summaryMinutes = 15;

    Mode mode(const QString &accountId) const;
    /** Whether new mail of @arg folder in @arg accountId is announced: the INBOX by default, other folders when on */
    bool announces(const QString &accountId, const QString &folder) const;
    /** Whether @arg folder has a setting of its own, as opposed to the default */
    bool hasFolderSetting(const QString &accountId, const QString &folder) const;
    /** Set whether @arg folder is announced; a value equal to the default removes the folder's own setting */
    void setAnnounces(const QString &accountId, const QString &folder, bool announce);
    /** The folders of @arg accountId announced besides the INBOX */
    QStringList extraFolders(const QString &accountId) const;
    bool operator==(const Settings &other) const = default;
};

/** @short Why a message is not announced */
enum class Skip {
    /** It is announced */
    None,
    /** The account's announcements are off */
    AccountOff,
    /** The folder's mail is not announced and no rule asked for it */
    FolderQuiet,
    /** It was read already, here or elsewhere */
    Read,
    /** It was received before the folder was last checked: old mail moved into the folder */
    Old,
    /** It belongs to the conversation being read */
    Reading,
};

/** Mail received this long before the last check still counts as new, for delivery delays and clock differences */
constexpr int graceSeconds = 10 * 60;

/** @short Decide whether @arg message is announced

@arg lastCheck is when the folder was last checked before the message showed up; an invalid time skips the age check.
@arg readingIds are the Message-IDs of the conversation being read. @arg ruleNotify is set when a rule's Notify action
matched the message.
*/
Skip classify(const MailRules::Message &message, const Settings &settings, const QDateTime &lastCheck,
              const QStringList &readingIds, bool ruleNotify = false);

/** @short When an account's announcement is shown */
enum class Delivery {
    Now,
    /** Later: when the window is left, or when the next summary is due */
    Hold,
    Never,
};

/** @short When to show the announcement of an account in @arg mode

@arg lastShown is when the account's last summary was shown; summaries come at most every @arg summaryMinutes.
*/
Delivery delivery(Mode mode, bool windowActive, bool quietWhileActive, const QDateTime &lastShown, const QDateTime &now,
                  int summaryMinutes);
/** When the next summary of an account may be shown, or an invalid time when it may be shown now */
QDateTime nextSummary(const QDateTime &lastShown, const QDateTime &now, int summaryMinutes);

/** @short One announced message */
struct Entry {
    QString folder;
    uint uid = 0;
    QString sender;
    QString subject;
    QString messageId;
    QDateTime received;
    bool operator==(const Entry &other) const = default;
};

/** @short The text of an account's notification */
struct Text {
    QString title;
    QString body;
    /** The messages announced, counted ones included */
    int count = 0;
    /** Whether the notification is about a single known message, which can then be moved to the trash from it */
    bool single = false;
};

/** @short The new mail announced for each account, until it is read, removed or dismissed

Messages are known by folder and UID; adding one again changes nothing. Folders checked only by their counts, which is
how folders other than the one open on the account's connection are checked, add counts instead of messages.
*/
class Digest
{
public:
    /** Add @arg entries for @arg accountId; returns whether something was new */
    bool add(const QString &accountId, const QList<Entry> &entries);
    /** Add @arg count new messages of @arg folder, known only by their number; returns whether @arg count is positive */
    bool addCount(const QString &accountId, const QString &folder, int count);
    /** Forget a message which was read, moved or deleted; returns whether it was announced */
    bool remove(const QString &accountId, const QString &folder, uint uid);
    /** Forget everything announced for @arg accountId, as when the notification is dismissed or the account is shown */
    void clear(const QString &accountId);
    /** Forget the counted messages of @arg folder, as when the folder is opened */
    bool clearCount(const QString &accountId, const QString &folder);

    QList<Entry> entries(const QString &accountId) const;
    int count(const QString &accountId) const;
    bool isEmpty(const QString &accountId) const;
    QStringList accounts() const;

    /** The notification of @arg accountId, newest messages first */
    Text text(const QString &accountId, const QString &accountName, bool showContent) const;

private:
    struct Account {
        QList<Entry> entries;
        QHash<QString, int> counts;
    };
    QHash<QString, Account> m_accounts;
};

/** The most messages listed in a notification; the rest are counted */
constexpr int listedMessages = 4;

}

}
