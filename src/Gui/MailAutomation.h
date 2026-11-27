/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>
#include <memory>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <KSharedConfig>

#include "UiUtils/MailRules.h"
#include "UiUtils/NewMail.h"
#include "UiUtils/NewMailWatcher.h"

class QTimer;

namespace Imap {
namespace Mailbox {
class Model;
}
}

namespace UiUtils {
class ActivityLog;
}

namespace Gui {

/** @short Runs the mail rules on new mail and announces it, one updatable notification per account

The automation watches the folders of each account which the rules or the announcements need: the INBOX, which an
account other than the shown one keeps open on its connection, and the other folders while they are open anyway. New
messages go through the rules first, in their order; then the ones still unread and in sight are announced, as
UiUtils::NewMail decides. Everything it does, and everything that fails, is written to the activity log.

The shell and the reader do not depend on this class; the window only tells it which account is shown, whether the
window is active and which conversation is read, and reacts to openRequested().
*/
class MailAutomation : public QObject
{
    Q_OBJECT

public:
    /** @short Where an account's new-mail notification is shown; replaced in tests */
    class Notifier
    {
    public:
        virtual ~Notifier();
        /** Show or update the notification of @arg accountId */
        virtual void show(const QString &accountId, const UiUtils::NewMail::Text &text) = 0;
        /** Close the notification of @arg accountId, if it is shown */
        virtual void close(const QString &accountId) = 0;
    };

    /** @short The archive and trash folders of an account, which moving rules need */
    struct Folders {
        QString archive;
        QString trash;
    };
    using FolderResolver = std::function<Folders(const QString &accountId)>;

    /** Rules and notification settings are kept in @arg config, what was seen in each folder in @arg state */
    MailAutomation(KSharedConfigPtr config, KSharedConfigPtr state, QObject *parent = nullptr);
    ~MailAutomation() override;

    /** Replace the desktop notifications, for example in tests */
    void setNotifier(std::unique_ptr<Notifier> notifier);
    void setFolderResolver(const FolderResolver &resolver);

    /** @short Watch the account @arg accountId, called @arg name, whose mail is in @arg model */
    void addAccount(const QString &accountId, const QString &name, Imap::Mailbox::Model *model);
    /** Stop watching an account, as when its connection is replaced; its settings stay */
    void removeAccount(const QString &accountId);
    /** @short Forget an account which was removed, with its rules, its notification settings and what was seen */
    void forgetAccount(const QString &accountId);
    void setAccountName(const QString &accountId, const QString &name);
    QStringList accounts() const;

    /** The account shown in the window; its folders are not opened by the automation */
    void setActiveAccount(const QString &accountId);
    QString activeAccount() const;
    /** Whether the window is active; announcements may wait meanwhile */
    void setWindowActive(bool active);
    /** The Message-IDs of the conversation being read, whose replies are not announced */
    void setReading(const QStringList &messageIds);

    QList<UiUtils::MailRules::Rule> rules() const;
    /** Replace and save the rules */
    void setRules(const QList<UiUtils::MailRules::Rule> &rules);
    UiUtils::NewMail::Settings notificationSettings() const;
    /** Replace and save the notification settings */
    void setNotificationSettings(const UiUtils::NewMail::Settings &settings);

    UiUtils::ActivityLog *activity() const;
    /** What is announced now */
    const UiUtils::NewMail::Digest &digest() const;

    /** @short Run @arg rules, as the user asked, on @arg messages of @arg folder in @arg accountId

    Every enabled rule counts, including those waiting for a preview. Returns the number of messages which a rule
    changed or moved.
    */
    int apply(const QList<UiUtils::MailRules::Rule> &rules, const QString &accountId, const QString &folder,
              const QList<UiUtils::MailRules::Message> &messages);

    /** Mark the mail announced for @arg accountId as read and close its notification */
    void markAnnouncedRead(const QString &accountId);
    /** Move the single message announced for @arg accountId to the trash and close its notification */
    void trashAnnounced(const QString &accountId);
    /** Forget what is announced for @arg accountId, as when its notification is dismissed */
    void dismiss(const QString &accountId);
    /** The user opened the notification of @arg accountId, handing over @arg activationToken for the window */
    void activate(const QString &accountId, const QString &activationToken = QString());

    /** The folders watched for @arg accountId, for tests and the activity view */
    QStringList watchedFolders(const QString &accountId) const;

signals:
    /** @short The user asked to see the announced mail: the message @arg uid in @arg folder, or the folder when 0 */
    void openRequested(const QString &accountId, const QString &folder, uint uid, const QString &activationToken);
    /** @short A short report for the window's status line */
    void statusMessage(const QString &text);
    void rulesChanged();
    void notificationSettingsChanged();

private:
    struct Account;

    void arrived(const QString &accountId, const QString &folder, const QList<UiUtils::MailRules::Message> &messages,
                 const QDateTime &lastCheck);
    /** Carry out @arg outcomes; returns how many messages were changed */
    int execute(Account &account, const QString &folder,
                const QList<QPair<UiUtils::MailRules::Message, UiUtils::MailRules::Outcome>> &outcomes, bool manual);
    void announce(const QString &accountId);
    void deliverHeld();
    void refreshWatchers();
    void refreshWatchers(Account &account);
    void checkCounts(const QString &accountId);
    void forgetMessage(const QString &accountId, const QString &folder, uint uid);
    void renameFolder(const QString &accountId, const QString &oldName, const QString &newName);
    void saveRules();
    void saveSettings();

    KSharedConfigPtr m_config;
    KSharedConfigPtr m_state;
    std::unique_ptr<Notifier> m_notifier;
    FolderResolver m_folders;
    QHash<QString, Account *> m_accounts;
    QString m_activeAccount;
    bool m_windowActive = false;
    QStringList m_reading;
    QList<UiUtils::MailRules::Rule> m_rules;
    UiUtils::NewMail::Settings m_settings;
    UiUtils::NewMail::Digest m_digest;
    UiUtils::ActivityLog *m_activity;
    QTimer *m_summaryTimer;
    QTimer *m_countTimer;
};

}
