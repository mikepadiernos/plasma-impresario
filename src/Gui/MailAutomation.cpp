/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MailAutomation.h"

#include <algorithm>
#include <numeric>

#include <QTimer>

#include <KConfigGroup>
#include <KLocalizedString>

#include "configure.cmake.h"
#ifdef TROJITA_HAVE_KNOTIFICATIONS
#include <KNotification>
#endif
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/ImapTask.h"
#include "UiUtils/ActivityLog.h"
#include "UiUtils/MailRulesSettings.h"
#include "UiUtils/NewMailSettings.h"

namespace Gui {

using UiUtils::ActivityLog;
using UiUtils::NewMailWatcher;
namespace MailRules = UiUtils::MailRules;
namespace NewMail = UiUtils::NewMail;

namespace {

const QString inbox = QStringLiteral("INBOX");

/** How often the unread counts of announced folders other than the open one are looked at */
constexpr int countIntervalMs = 2 * 60 * 1000;

QString folderLabel(const QString &folder)
{
    return folder.compare(inbox, Qt::CaseInsensitive) == 0 ? i18nc("@item the INBOX folder", "Inbox") : folder;
}

#ifdef TROJITA_HAVE_KNOTIFICATIONS
/** @short The new-mail notification of each account, updated in place while it shows the same kind of content */
class DesktopNotifier : public MailAutomation::Notifier
{
public:
    explicit DesktopNotifier(MailAutomation *owner)
        : m_owner(owner)
    {
    }

    ~DesktopNotifier() override
    {
        const auto shown = m_shown;
        m_shown.clear();
        for (const Shown &notification : shown) {
            if (notification.notification)
                notification.notification->close();
        }
    }

    void show(const QString &accountId, const NewMail::Text &text) override
    {
        Shown current = m_shown.value(accountId);
        // Deleting from a notification only fits a single message, so a change of kind needs a new notification
        if (current.notification && current.single == text.single) {
            current.notification->setTitle(text.title);
            current.notification->setText(text.body.toHtmlEscaped());
            return;
        }
        close(accountId);
        auto *notification = new KNotification(QStringLiteral("newMail"), KNotification::CloseOnTimeout);
        notification->setComponentName(QStringLiteral("plasma-impresario"));
        notification->setTitle(text.title);
        notification->setText(text.body.toHtmlEscaped());
        notification->setIconName(QStringLiteral("mail-unread-new"));
        QPointer<MailAutomation> owner = m_owner;
        KNotificationAction *open = notification->addDefaultAction(i18nc("@action:button", "Open"));
        QObject::connect(open, &KNotificationAction::activated, m_owner, [owner, accountId, notification]() {
            if (owner)
                owner->activate(accountId, notification->xdgActivationToken());
        });
        KNotificationAction *read = notification->addAction(i18nc("@action:button", "Mark as Read"));
        QObject::connect(read, &KNotificationAction::activated, m_owner, [owner, accountId]() {
            if (owner)
                owner->markAnnouncedRead(accountId);
        });
        if (text.single) {
            KNotificationAction *trash = notification->addAction(i18nc("@action:button", "Delete"));
            QObject::connect(trash, &KNotificationAction::activated, m_owner, [owner, accountId]() {
                if (owner)
                    owner->trashAnnounced(accountId);
            });
        }
        QObject::connect(notification, &KNotification::closed, m_owner, [this, owner, accountId, notification]() {
            if (!owner || m_shown.value(accountId).notification != notification)
                return;
            m_shown.remove(accountId);
            owner->dismiss(accountId);
        });
        m_shown.insert(accountId, {notification, text.single});
        notification->sendEvent();
    }

    void close(const QString &accountId) override
    {
        const Shown shown = m_shown.take(accountId);
        if (shown.notification)
            shown.notification->close();
    }

private:
    struct Shown {
        QPointer<KNotification> notification;
        bool single = false;
    };
    MailAutomation *m_owner;
    QHash<QString, Shown> m_shown;
};
#endif

/** Notifications which are not shown anywhere, for builds without desktop notifications */
class SilentNotifier : public MailAutomation::Notifier
{
public:
    void show(const QString &, const NewMail::Text &) override
    {
    }
    void close(const QString &) override
    {
    }
};

}

MailAutomation::Notifier::~Notifier() = default;

struct MailAutomation::Account {
    QString id;
    QString name;
    QPointer<Imap::Mailbox::Model> model;
    QHash<QString, NewMailWatcher *> watchers;
    /** The last unread count of each announced folder which is checked by its count */
    QHash<QString, int> unread;
    QDateTime lastShown;
    QList<QMetaObject::Connection> connections;
};

MailAutomation::MailAutomation(KSharedConfigPtr config, KSharedConfigPtr state, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_state(std::move(state))
    , m_activity(new ActivityLog(this))
    , m_summaryTimer(new QTimer(this))
    , m_countTimer(new QTimer(this))
{
#ifdef TROJITA_HAVE_KNOTIFICATIONS
    m_notifier = std::make_unique<DesktopNotifier>(this);
#else
    m_notifier = std::make_unique<SilentNotifier>();
#endif
    m_rules = UiUtils::MailRulesSettings::load(*m_config);
    m_settings = UiUtils::NewMailSettings::load(*m_config);
    m_summaryTimer->setSingleShot(true);
    connect(m_summaryTimer, &QTimer::timeout, this, &MailAutomation::deliverHeld);
    m_countTimer->setInterval(countIntervalMs);
    connect(m_countTimer, &QTimer::timeout, this, [this]() {
        for (const QString &accountId : m_accounts.keys())
            checkCounts(accountId);
    });
    m_countTimer->start();
}

MailAutomation::~MailAutomation()
{
    for (const QString &accountId : m_accounts.keys())
        removeAccount(accountId);
    m_notifier.reset();
}

void MailAutomation::setNotifier(std::unique_ptr<Notifier> notifier)
{
    m_notifier = notifier ? std::move(notifier) : std::make_unique<SilentNotifier>();
}

void MailAutomation::setFolderResolver(const FolderResolver &resolver)
{
    m_folders = resolver;
}

void MailAutomation::addAccount(const QString &accountId, const QString &name, Imap::Mailbox::Model *model)
{
    if (accountId.isEmpty() || !model)
        return;
    if (Account *existing = m_accounts.value(accountId)) {
        existing->name = name;
        if (existing->model == model)
            return;
        removeAccount(accountId);
    }
    auto *account = new Account;
    account->id = accountId;
    account->name = name;
    account->model = model;
    m_accounts.insert(accountId, account);
    account->connections << connect(model, &Imap::Mailbox::Model::mailboxRenameSucceeded, this,
                                    [this, accountId](const QString &oldName, const QString &newName) {
        renameFolder(accountId, oldName, newName);
    });
    account->connections << connect(model, &Imap::Mailbox::Model::messageCountPossiblyChanged, this,
                                    [this, accountId]() { checkCounts(accountId); });
    // Mail read, moved or deleted elsewhere drops out of the notification
    account->connections << connect(model, &QAbstractItemModel::dataChanged, this,
                                    [this, accountId](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        if (m_digest.isEmpty(accountId) || topLeft.parent() != bottomRight.parent())
            return;
        const QModelIndex list = topLeft.parent();
        if (!list.isValid() || !list.parent().isValid() || topLeft.data(Imap::Mailbox::RoleMessageUid).isNull())
            return;
        const QString folder = list.parent().data(Imap::Mailbox::RoleMailboxName).toString();
        for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
            const QModelIndex message = topLeft.sibling(row, 0);
            if (message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool()
                || message.data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool())
                forgetMessage(accountId, folder, message.data(Imap::Mailbox::RoleMessageUid).toUInt());
        }
    });
    account->connections << connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                                    [this, accountId](const QModelIndex &list, int first, int last) {
        if (m_digest.isEmpty(accountId) || !list.isValid() || !list.parent().isValid())
            return;
        const QString folder = list.parent().data(Imap::Mailbox::RoleMailboxName).toString();
        for (int row = first; row <= last; ++row) {
            const QModelIndex message = list.model()->index(row, 0, list);
            if (const uint uid = message.data(Imap::Mailbox::RoleMessageUid).toUInt())
                forgetMessage(accountId, folder, uid);
        }
    });
    refreshWatchers(*account);
}

void MailAutomation::removeAccount(const QString &accountId)
{
    Account *account = m_accounts.take(accountId);
    if (!account)
        return;
    for (const QMetaObject::Connection &connection : std::as_const(account->connections))
        disconnect(connection);
    qDeleteAll(account->watchers);
    delete account;
    m_digest.clear(accountId);
    m_notifier->close(accountId);
}

void MailAutomation::forgetAccount(const QString &accountId)
{
    removeAccount(accountId);
    UiUtils::MailRulesSettings::removeAccount(*m_config, accountId);
    UiUtils::NewMailSettings::removeAccount(*m_config, accountId);
    m_config->sync();
    UiUtils::NewMailSettings::removeBaselines(*m_state, accountId);
    m_state->sync();
    m_activity->removeAccount(accountId);
    m_rules = UiUtils::MailRulesSettings::load(*m_config);
    m_settings = UiUtils::NewMailSettings::load(*m_config);
    emit rulesChanged();
    emit notificationSettingsChanged();
}

void MailAutomation::setAccountName(const QString &accountId, const QString &name)
{
    if (Account *account = m_accounts.value(accountId))
        account->name = name;
}

QStringList MailAutomation::accounts() const
{
    return m_accounts.keys();
}

void MailAutomation::setActiveAccount(const QString &accountId)
{
    if (m_activeAccount == accountId)
        return;
    m_activeAccount = accountId;
    refreshWatchers();
    if (m_windowActive)
        dismiss(accountId);
}

QString MailAutomation::activeAccount() const
{
    return m_activeAccount;
}

void MailAutomation::setWindowActive(bool active)
{
    if (m_windowActive == active)
        return;
    m_windowActive = active;
    if (active) {
        // The shown account's new mail is in sight now
        dismiss(m_activeAccount);
    } else {
        deliverHeld();
    }
}

void MailAutomation::setReading(const QStringList &messageIds)
{
    m_reading = messageIds;
}

QList<MailRules::Rule> MailAutomation::rules() const
{
    return m_rules;
}

void MailAutomation::setRules(const QList<MailRules::Rule> &rules)
{
    if (m_rules == rules)
        return;
    m_rules = rules;
    saveRules();
    refreshWatchers();
    emit rulesChanged();
}

NewMail::Settings MailAutomation::notificationSettings() const
{
    return m_settings;
}

void MailAutomation::setNotificationSettings(const NewMail::Settings &settings)
{
    if (m_settings == settings)
        return;
    m_settings = settings;
    saveSettings();
    for (const QString &accountId : m_digest.accounts()) {
        if (m_settings.mode(accountId) == NewMail::Mode::Off)
            dismiss(accountId);
    }
    refreshWatchers();
    emit notificationSettingsChanged();
}

ActivityLog *MailAutomation::activity() const
{
    return m_activity;
}

const NewMail::Digest &MailAutomation::digest() const
{
    return m_digest;
}

QStringList MailAutomation::watchedFolders(const QString &accountId) const
{
    const Account *account = m_accounts.value(accountId);
    if (!account)
        return {};
    QStringList folders = account->watchers.keys();
    std::sort(folders.begin(), folders.end());
    return folders;
}

void MailAutomation::refreshWatchers()
{
    for (Account *account : std::as_const(m_accounts))
        refreshWatchers(*account);
}

void MailAutomation::refreshWatchers(Account &account)
{
    QSet<QString> wanted;
    const bool announces = m_settings.mode(account.id) != NewMail::Mode::Off;
    if (announces)
        wanted.insert(inbox);
    for (const MailRules::Rule &rule : std::as_const(m_rules)) {
        if (!rule.enabled || (!rule.accountId.isEmpty() && rule.accountId != account.id))
            continue;
        for (const QString &folder : rule.checkedFolders())
            wanted.insert(folder);
    }
    if (announces) {
        for (const QString &folder : m_settings.extraFolders(account.id))
            wanted.insert(folder);
    }

    for (auto it = account.watchers.begin(); it != account.watchers.end();) {
        if (wanted.contains(it.key())) {
            ++it;
            continue;
        }
        delete it.value();
        it = account.watchers.erase(it);
    }
    const QString accountId = account.id;
    for (const QString &folder : std::as_const(wanted)) {
        NewMailWatcher *watcher = account.watchers.value(folder);
        if (!watcher) {
            watcher = new NewMailWatcher(account.model, accountId, folder, this);
            watcher->setBaseline(UiUtils::NewMailSettings::loadBaseline(*m_state, accountId, folder));
            connect(watcher, &NewMailWatcher::arrived, this,
                    [this, accountId, folder](const QList<MailRules::Message> &messages, const QDateTime &lastCheck) {
                arrived(accountId, folder, messages, lastCheck);
            });
            connect(watcher, &NewMailWatcher::baselineChanged, this, [this, watcher, accountId, folder]() {
                UiUtils::NewMailSettings::saveBaseline(*m_state, accountId, folder, watcher->baseline());
                m_state->sync();
                // The folder is open and checked, so its count says nothing new
                if (Account *account = m_accounts.value(accountId))
                    account->unread.remove(folder);
            });
            account.watchers.insert(folder, watcher);
        }
        // The connection shows one folder at a time; only an account which is not shown keeps its INBOX open
        watcher->setOpensMailbox(folder == inbox && accountId != m_activeAccount);
    }
    for (auto it = account.unread.begin(); it != account.unread.end();) {
        if (m_settings.extraFolders(account.id).contains(it.key()))
            ++it;
        else
            it = account.unread.erase(it);
    }
}

void MailAutomation::arrived(const QString &accountId, const QString &folder, const QList<MailRules::Message> &messages,
                             const QDateTime &lastCheck)
{
    Account *account = m_accounts.value(accountId);
    if (!account || !account->model)
        return;
    m_digest.clearCount(accountId, folder);

    QList<QPair<MailRules::Message, MailRules::Outcome>> outcomes;
    QStringList waiting;
    for (const MailRules::Message &message : messages) {
        const MailRules::Outcome outcome = MailRules::evaluate(m_rules, message, MailRules::Mode::Automatic);
        for (const QString &ruleId : outcome.waiting) {
            if (!waiting.contains(ruleId))
                waiting << ruleId;
        }
        outcomes.append({message, outcome});
    }
    for (const QString &ruleId : std::as_const(waiting)) {
        const auto rule = std::find_if(m_rules.cbegin(), m_rules.cend(),
                                       [&ruleId](const MailRules::Rule &r) { return r.id == ruleId; });
        if (rule == m_rules.cend())
            continue;
        ActivityLog::Entry entry;
        entry.key = QStringLiteral("rule-review:") + ruleId;
        entry.kind = ActivityLog::Kind::Rule;
        entry.state = ActivityLog::State::Waiting;
        entry.accountId = accountId;
        entry.title = i18nc("@info %1 is a rule name", "Rule “%1” waits for a preview", rule->name);
        entry.detail = i18nc("@info", "It matched new mail, but it moves messages. Preview it once in the rules to "
                                      "let it run on its own.");
        entry.action = ActivityLog::Action::ReviewRule;
        entry.target = ruleId;
        m_activity->record(entry);
    }
    QList<QPair<MailRules::Message, MailRules::Outcome>> matched;
    for (const auto &pair : std::as_const(outcomes)) {
        if (pair.second.matched())
            matched << pair;
    }
    if (!matched.isEmpty())
        execute(*account, folder, matched, false);

    if (m_settings.mode(accountId) == NewMail::Mode::Off)
        return;
    const bool inSight = m_windowActive && accountId == m_activeAccount;
    QList<NewMail::Entry> entries;
    QHash<QString, int> movedAway;
    for (const auto &[original, outcome] : std::as_const(outcomes)) {
        MailRules::Message message = original;
        if (outcome.markRead)
            message.unread = false;
        // Archived or deleted by a rule: put away on purpose, so not news unless the rule says so
        using Destination = MailRules::Outcome::Destination;
        const bool putAway = outcome.destination == Destination::Archive || outcome.destination == Destination::Trash;
        if (putAway && !outcome.notify)
            continue;
        if (outcome.destination == Destination::Move)
            message.folder = outcome.folder;
        if (NewMail::classify(message, m_settings, lastCheck, m_reading, outcome.notify) != NewMail::Skip::None)
            continue;
        if (outcome.moves()) {
            // Its UID in the new folder is not known yet
            ++movedAway[message.folder];
            continue;
        }
        NewMail::Entry entry;
        entry.folder = folder;
        entry.uid = message.uid;
        entry.sender = message.senders.value(0);
        entry.subject = message.subject;
        entry.messageId = message.messageId;
        entry.received = message.received;
        entries << entry;
    }
    const int count = static_cast<int>(entries.size())
        + std::accumulate(movedAway.cbegin(), movedAway.cend(), 0);
    if (count == 0)
        return;
    if (inSight) {
        emit statusMessage(i18ncp("@info:status", "%1 new message", "%1 new messages", count));
        return;
    }
    bool added = m_digest.add(accountId, entries);
    for (auto it = movedAway.cbegin(); it != movedAway.cend(); ++it)
        added = m_digest.addCount(accountId, it.key(), it.value()) || added;
    if (added)
        announce(accountId);
}

int MailAutomation::apply(const QList<MailRules::Rule> &rules, const QString &accountId, const QString &folder,
                          const QList<MailRules::Message> &messages)
{
    Account *account = m_accounts.value(accountId);
    if (!account || !account->model)
        return 0;
    QList<QPair<MailRules::Message, MailRules::Outcome>> matched;
    for (const MailRules::Message &message : messages) {
        const MailRules::Outcome outcome = MailRules::evaluate(rules, message, MailRules::Mode::Manual);
        if (outcome.matched())
            matched.append({message, outcome});
    }
    if (matched.isEmpty())
        return 0;
    return execute(*account, folder, matched, true);
}

int MailAutomation::execute(Account &account, const QString &folder,
                            const QList<QPair<MailRules::Message, MailRules::Outcome>> &outcomes, bool manual)
{
    using Destination = MailRules::Outcome::Destination;
    Imap::Mailbox::Model *model = account.model;
    const QModelIndex mailbox = model->mailboxIndexByName(folder);
    const QModelIndex list = mailbox.isValid() ? model->index(0, 0, mailbox) : QModelIndex();
    QHash<uint, QPersistentModelIndex> byUid;
    if (list.isValid()) {
        const int rows = model->rowCount(list);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex message = model->index(row, 0, list);
            if (const uint uid = message.data(Imap::Mailbox::RoleMessageUid).toUInt())
                byUid.insert(uid, message);
        }
    }
    const Folders folders = m_folders ? m_folders(account.id) : Folders();

    QModelIndexList toFlag;
    QModelIndexList toRead;
    QHash<QString, QModelIndexList> toTag;
    QHash<QString, Imap::Uids> toMove;
    QStringList problems;
    QStringList ruleNames;
    int changed = 0;
    for (const auto &[message, outcome] : outcomes) {
        for (const QString &name : outcome.ruleNames) {
            if (!ruleNames.contains(name))
                ruleNames << name;
        }
        const QPersistentModelIndex index = byUid.value(message.uid);
        bool touched = false;
        if (index.isValid()) {
            if (outcome.flag && !message.flags.contains(QLatin1String("\\Flagged"))) {
                toFlag << index;
                touched = true;
            }
            if (outcome.markRead && message.unread) {
                toRead << index;
                touched = true;
            }
            for (const QString &tag : outcome.tags) {
                if (!message.flags.contains(tag)) {
                    toTag[tag] << index;
                    touched = true;
                }
            }
        } else if (outcome.flag || outcome.markRead || !outcome.tags.isEmpty()) {
            problems << i18nc("@info", "A message is no longer in “%1”.", folderLabel(folder));
        }
        QString destination;
        switch (outcome.destination) {
        case Destination::Stay:
            break;
        case Destination::Move:
            destination = outcome.folder;
            break;
        case Destination::Archive:
            destination = folders.archive;
            if (destination.isEmpty())
                problems << i18nc("@info", "The account has no archive folder.");
            break;
        case Destination::Trash:
            destination = folders.trash;
            if (destination.isEmpty())
                problems << i18nc("@info", "The account has no trash folder.");
            break;
        }
        if (!destination.isEmpty() && destination != folder) {
            toMove[destination] << message.uid;
            touched = true;
        }
        if (touched)
            ++changed;
    }
    problems.removeDuplicates();

    ActivityLog::Entry entry;
    entry.key = QStringLiteral("rules:%1:%2").arg(account.id, folder);
    entry.kind = ActivityLog::Kind::Rule;
    entry.accountId = account.id;
    entry.title = manual ? i18nc("@info %1 is a folder", "Rules applied to “%1”", folderLabel(folder))
                         : i18nc("@info %1 is a folder", "Rules sorted new mail in “%1”", folderLabel(folder));
    entry.detail = i18ncp("@info %2 lists rule names", "%1 message changed by %2", "%1 messages changed by %2", changed,
                          ruleNames.join(i18nc("@info list separator", ", ")));
    entry.state = problems.isEmpty() ? ActivityLog::State::Running : ActivityLog::State::Failed;
    entry.error = problems.join(QLatin1Char(' '));
    if (!problems.isEmpty()) {
        entry.action = ActivityLog::Action::OpenSettings;
        entry.target = account.id;
    }

    QList<QPointer<Imap::Mailbox::ImapTask>> tasks;
    auto track = [&tasks, &problems](Imap::Mailbox::ImapTask *task) {
        if (task)
            tasks << task;
        else
            problems << i18nc("@info", "The account is offline.");
    };
    if (!toFlag.isEmpty())
        track(model->setMessageFlags(toFlag, QStringLiteral("\\Flagged"), Imap::Mailbox::FLAG_ADD));
    if (!toRead.isEmpty())
        track(model->markMessagesRead(toRead, Imap::Mailbox::FLAG_ADD));
    for (auto it = toTag.cbegin(); it != toTag.cend(); ++it)
        track(model->setMessageFlags(it.value(), it.key(), Imap::Mailbox::FLAG_ADD));
    const uint uidValidity = mailbox.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt();
    for (auto it = toMove.cbegin(); it != toMove.cend(); ++it)
        track(model->copyMoveMessagesByUid(mailbox, it.value(), it.key(), Imap::Mailbox::MOVE, uidValidity));
    problems.removeDuplicates();
    if (!problems.isEmpty()) {
        entry.state = ActivityLog::State::Failed;
        entry.error = problems.join(QLatin1Char(' '));
    }
    if (tasks.isEmpty() && entry.state == ActivityLog::State::Running)
        entry.state = ActivityLog::State::Done;
    m_activity->record(entry);
    if (changed > 0 && entry.state != ActivityLog::State::Failed) {
        const QString status = manual
            ? i18ncp("@info:status", "Rules changed %1 message", "Rules changed %1 messages", changed)
            : i18ncp("@info:status", "Rules sorted %1 new message", "Rules sorted %1 new messages", changed);
        if (account.id == m_activeAccount)
            emit statusMessage(status);
    }

    if (tasks.isEmpty())
        return changed;
    auto pending = std::make_shared<int>(static_cast<int>(tasks.size()));
    auto failure = std::make_shared<QString>();
    const QString key = entry.key;
    QPointer<ActivityLog> log = m_activity;
    auto settle = [pending, failure, key, log]() {
        if (--*pending > 0 || !log)
            return;
        if (log->find(key) < 0 || log->entry(log->find(key)).state != ActivityLog::State::Running)
            return;
        log->finish(key, failure->isEmpty() ? ActivityLog::State::Done : ActivityLog::State::Failed, *failure);
    };
    for (const QPointer<Imap::Mailbox::ImapTask> &task : std::as_const(tasks)) {
        connect(task, &Imap::Mailbox::ImapTask::completed, this, [settle]() { settle(); });
        connect(task, &Imap::Mailbox::ImapTask::failed, this, [settle, failure](const QString &message) {
            if (failure->isEmpty())
                *failure = message;
            settle();
        });
    }
    return changed;
}

void MailAutomation::announce(const QString &accountId)
{
    Account *account = m_accounts.value(accountId);
    if (!account)
        return;
    if (m_digest.isEmpty(accountId)) {
        m_notifier->close(accountId);
        return;
    }
    const NewMail::Mode mode = m_settings.mode(accountId);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    switch (NewMail::delivery(mode, m_windowActive, m_settings.quietWhileActive, account->lastShown, now,
                              m_settings.summaryMinutes)) {
    case NewMail::Delivery::Never:
        dismiss(accountId);
        return;
    case NewMail::Delivery::Hold: {
        const QDateTime due = NewMail::nextSummary(account->lastShown, now, m_settings.summaryMinutes);
        if (due.isValid()) {
            const qint64 wait = std::max<qint64>(1000, now.msecsTo(due));
            if (!m_summaryTimer->isActive() || m_summaryTimer->remainingTime() > wait)
                m_summaryTimer->start(static_cast<int>(wait));
        }
        return;
    }
    case NewMail::Delivery::Now:
        break;
    }
    const NewMail::Text text = m_digest.text(accountId, account->name, m_settings.showContent);
    m_notifier->show(accountId, text);
    if (mode == NewMail::Mode::Summary)
        account->lastShown = now;
    ActivityLog::Entry entry;
    entry.key = QStringLiteral("new-mail:") + accountId;
    entry.kind = ActivityLog::Kind::Notification;
    entry.state = ActivityLog::State::Done;
    entry.accountId = accountId;
    entry.title = text.title;
    entry.action = ActivityLog::Action::OpenAccount;
    entry.target = accountId;
    m_activity->record(entry);
}

void MailAutomation::deliverHeld()
{
    for (const QString &accountId : m_digest.accounts())
        announce(accountId);
}

void MailAutomation::checkCounts(const QString &accountId)
{
    Account *account = m_accounts.value(accountId);
    if (!account || !account->model || m_settings.mode(accountId) == NewMail::Mode::Off)
        return;
    bool added = false;
    for (const QString &folder : m_settings.extraFolders(accountId)) {
        const QModelIndex mailbox = account->model->mailboxIndexByName(folder);
        if (!mailbox.isValid())
            continue;
        if (account->model->isMailboxOpen(mailbox)) {
            // Its watcher reports each message
            account->unread.remove(folder);
            continue;
        }
        const QVariant unread = mailbox.data(Imap::Mailbox::RoleUnreadMessageCount);
        if (!unread.isValid())
            continue;
        const int now = unread.toInt();
        const int before = account->unread.value(folder, -1);
        account->unread.insert(folder, now);
        if (before >= 0 && now > before && !(m_windowActive && accountId == m_activeAccount))
            added = m_digest.addCount(accountId, folder, now - before) || added;
    }
    if (added)
        announce(accountId);
}

void MailAutomation::forgetMessage(const QString &accountId, const QString &folder, uint uid)
{
    if (m_digest.remove(accountId, folder, uid))
        announce(accountId);
}

void MailAutomation::markAnnouncedRead(const QString &accountId)
{
    Account *account = m_accounts.value(accountId);
    if (account && account->model) {
        QHash<QString, QSet<uint>> byFolder;
        for (const NewMail::Entry &entry : m_digest.entries(accountId))
            byFolder[entry.folder].insert(entry.uid);
        for (auto it = byFolder.cbegin(); it != byFolder.cend(); ++it) {
            const QModelIndex mailbox = account->model->mailboxIndexByName(it.key());
            const QModelIndex list = mailbox.isValid() ? account->model->index(0, 0, mailbox) : QModelIndex();
            if (!list.isValid())
                continue;
            QModelIndexList messages;
            const int rows = account->model->rowCount(list);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex message = account->model->index(row, 0, list);
                if (it.value().contains(message.data(Imap::Mailbox::RoleMessageUid).toUInt()))
                    messages << message;
            }
            if (!messages.isEmpty())
                account->model->markMessagesRead(messages, Imap::Mailbox::FLAG_ADD);
        }
    }
    dismiss(accountId);
}

void MailAutomation::trashAnnounced(const QString &accountId)
{
    Account *account = m_accounts.value(accountId);
    const QList<NewMail::Entry> entries = m_digest.entries(accountId);
    if (account && account->model && entries.size() == 1) {
        const NewMail::Entry entry = entries.first();
        const QString trash = m_folders ? m_folders(accountId).trash : QString();
        const QModelIndex mailbox = account->model->mailboxIndexByName(entry.folder);
        if (trash.isEmpty() || !mailbox.isValid()) {
            ActivityLog::Entry failure;
            failure.key = QStringLiteral("new-mail-delete:") + accountId;
            failure.kind = ActivityLog::Kind::Notification;
            failure.state = ActivityLog::State::Failed;
            failure.accountId = accountId;
            failure.title = i18nc("@info", "The new message could not be deleted");
            failure.error = trash.isEmpty() ? i18nc("@info", "The account has no trash folder.")
                                            : i18nc("@info", "Its folder is gone.");
            m_activity->record(failure);
        } else {
            account->model->copyMoveMessagesByUid(mailbox, Imap::Uids{entry.uid}, trash, Imap::Mailbox::MOVE,
                                                  mailbox.data(Imap::Mailbox::RoleMailboxUidValidity).toUInt());
        }
    }
    dismiss(accountId);
}

void MailAutomation::dismiss(const QString &accountId)
{
    if (accountId.isEmpty())
        return;
    m_digest.clear(accountId);
    m_notifier->close(accountId);
}

void MailAutomation::activate(const QString &accountId, const QString &activationToken)
{
    const QList<NewMail::Entry> entries = m_digest.entries(accountId);
    QString folder = inbox;
    uint uid = 0;
    if (entries.size() == 1 && m_digest.count(accountId) == 1) {
        folder = entries.first().folder;
        uid = entries.first().uid;
    } else if (!entries.isEmpty()) {
        folder = entries.first().folder;
        for (const NewMail::Entry &entry : entries) {
            if (entry.folder != folder) {
                folder = inbox;
                break;
            }
        }
    }
    dismiss(accountId);
    emit openRequested(accountId, folder, uid, activationToken);
}

void MailAutomation::renameFolder(const QString &accountId, const QString &oldName, const QString &newName)
{
    const bool rulesChanged = UiUtils::MailRulesSettings::renameFolder(*m_config, accountId, oldName, newName);
    const bool settingsChanged = UiUtils::NewMailSettings::renameFolder(*m_config, accountId, oldName, newName);
    if (!rulesChanged && !settingsChanged)
        return;
    m_config->sync();
    m_rules = UiUtils::MailRulesSettings::load(*m_config);
    m_settings = UiUtils::NewMailSettings::load(*m_config);
    refreshWatchers();
    if (rulesChanged)
        emit this->rulesChanged();
    if (settingsChanged)
        emit notificationSettingsChanged();
}

void MailAutomation::saveRules()
{
    UiUtils::MailRulesSettings::save(*m_config, m_rules);
    m_config->sync();
}

void MailAutomation::saveSettings()
{
    UiUtils::NewMailSettings::save(*m_config, m_settings);
    m_config->sync();
}

}
