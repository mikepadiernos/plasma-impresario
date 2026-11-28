/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/NextcloudTransfers.h"

#include <memory>

#include <KLocalizedString>
#include <QWidget>

#include "Gui/NextcloudHub.h"
#include "Nextcloud/Account.h"
#include "UiUtils/ActivityLog.h"

namespace Gui {

using Nextcloud::Client;

namespace {

/** Counts a transfer as running for as long as one of its callbacks may still
 * be called */
class RunningToken
{
public:
    explicit RunningToken(std::function<void()> finished)
        : m_finished(std::move(finished))
    {
    }
    ~RunningToken()
    {
        m_finished();
    }
    RunningToken(const RunningToken &) = delete;
    RunningToken &operator=(const RunningToken &) = delete;

private:
    std::function<void()> m_finished;
};

} // namespace

NextcloudTransfers::NextcloudTransfers(NextcloudHub *hub, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
{
}

NextcloudTransfers::~NextcloudTransfers() = default;

int NextcloudTransfers::runningCount() const
{
    return m_running;
}

void NextcloudTransfers::setActivityLog(UiUtils::ActivityLog *log)
{
    m_log = log;
}

QString NextcloudTransfers::nextJobKey()
{
    return QStringLiteral("nextcloud-transfer:%1").arg(++m_lastJob);
}

void NextcloudTransfers::beginJob(const QString &job, const QString &accountId, const QString &title)
{
    if (!m_log)
        return;
    UiUtils::ActivityLog::Entry entry;
    entry.key = job;
    entry.kind = UiUtils::ActivityLog::Kind::Job;
    entry.state = UiUtils::ActivityLog::State::Running;
    entry.title = title;
    if (m_hub)
        entry.detail = m_hub->account(accountId).title();
    entry.progress = 0;
    m_log->record(entry);
}

void NextcloudTransfers::jobProgress(const QString &job, qint64 done, qint64 total)
{
    if (!m_log || total <= 0)
        return;
    const int row = m_log->find(job);
    if (row < 0)
        return;
    UiUtils::ActivityLog::Entry entry = m_log->entry(row);
    const int progress = int(qBound(qint64(0), done * 100 / total, qint64(100)));
    if (entry.state != UiUtils::ActivityLog::State::Running || entry.progress == progress)
        return;
    entry.progress = progress;
    entry.time = QDateTime();
    m_log->record(entry);
}

void NextcloudTransfers::endJob(const QString &job, const QString &detail, const QString &error)
{
    if (!m_log)
        return;
    const int row = m_log->find(job);
    if (row < 0)
        return;
    UiUtils::ActivityLog::Entry entry = m_log->entry(row);
    entry.state = error.isEmpty() ? UiUtils::ActivityLog::State::Done : UiUtils::ActivityLog::State::Failed;
    entry.error = error;
    entry.action = UiUtils::ActivityLog::Action::None;
    entry.progress = -1;
    entry.time = QDateTime();
    if (!detail.isEmpty())
        entry.detail = detail;
    m_log->record(entry);
}

QWidget *NextcloudTransfers::dialogParent(QObject *context)
{
    auto *widget = qobject_cast<QWidget *>(context);
    return widget ? widget->window() : nullptr;
}

QString NextcloudTransfers::stoppedMessage()
{
    return i18nc("@info", "Stopped because the window it was for was closed.");
}

void NextcloudTransfers::run(const QString &accountId, QPointer<QObject> context, Attempt attempt, Failed failed)
{
    if (!m_hub) {
        failed(i18nc("@info", "Nextcloud is not available."));
        return;
    }
    // The hub reports to this object, so that the transfer finishes even when its
    // context is gone
    m_hub->withClient(
        accountId, this,
        [this, accountId, context, attempt, failed](Client *client) {
            if (!context) {
                failed(QString());
                return;
            }
            QPointer<Client> guardedClient(client);
            attempt(client,
                [this, accountId, context, attempt, failed, guardedClient](
                    Client::Failure failure, const QString &message) {
                    if (!context) {
                        failed(QString());
                        return;
                    }
                    if (failure == Client::Failure::Certificate && guardedClient && m_hub
                        && m_hub->askTrust(guardedClient, dialogParent(context))) {
                        run(accountId, context, attempt, failed);
                        return;
                    }
                    if (failure == Client::Failure::Authentication) {
                        const QString refused = m_hub ? m_hub->refusedMessage(accountId) : message;
                        if (m_hub)
                            m_hub->forgetClient(accountId);
                        failed(refused);
                        return;
                    }
                    failed(message);
                });
        },
        [context, failed](const QString &message) { failed(context ? message : QString()); });
}

void NextcloudTransfers::fetch(const QString &accountId, const Nextcloud::Entry &file, QObject *context,
    std::function<void(const QByteArray &data)> done, Failed failed)
{
    ++m_running;
    QPointer<NextcloudTransfers> self(this);
    auto token = std::make_shared<RunningToken>([self]() {
        if (self)
            --self->m_running;
    });
    QPointer<QObject> guard(context);
    const QString path = file.path;
    const QString job = nextJobKey();
    beginJob(job, accountId,
        i18nc("@info %1 is a file name", "Downloading \"%1\" from Nextcloud",
            file.name.isEmpty() ? path.section(QLatin1Char('/'), -1) : file.name));
    run(
        accountId, guard,
        [self, job, token, guard, path, done](Client *client, Client::Failed onFailure) {
            client->download(
                path,
                [self, job, token, guard, done](const QByteArray &data) {
                    if (self)
                        self->endJob(job, QString());
                    if (guard)
                        done(data);
                },
                onFailure,
                [self, job](qint64 received, qint64 total) {
                    if (self)
                        self->jobProgress(job, received, total);
                });
        },
        [self, job, token, guard, failed](const QString &message) {
            if (self)
                self->endJob(job, QString(), guard ? message : stoppedMessage());
            if (guard)
                failed(message);
        });
}

void NextcloudTransfers::store(const QString &accountId, const QString &folder, const QString &name,
    const QByteArray &data, QObject *context, std::function<void(const QString &path)> done, Failed failed)
{
    storeJob(nextJobKey(), accountId, folder, name, data, context, std::move(done), std::move(failed));
}

void NextcloudTransfers::storeJob(const QString &job, const QString &accountId, const QString &folder,
    const QString &name, const QByteArray &data, QObject *context, std::function<void(const QString &path)> done,
    Failed failed)
{
    ++m_running;
    QPointer<NextcloudTransfers> self(this);
    auto token = std::make_shared<RunningToken>([self]() {
        if (self)
            --self->m_running;
    });
    QPointer<QObject> guard(context);
    const QString cleanFolder = Nextcloud::cleanPath(folder);
    QString fileName = Nextcloud::safeFileName(name);
    if (fileName.isEmpty())
        fileName = i18nc("@info a file name for an attachment without one", "attachment");
    beginJob(job, accountId, i18nc("@info %1 is a file name", "Uploading \"%1\" to Nextcloud", fileName));
    storeAs(
        accountId, cleanFolder, fileName, data, 1, guard,
        [self, job, token, guard, done](const QString &path) {
            if (self)
                self->endJob(job, path);
            if (guard)
                done(path);
        },
        [self, job, token, guard, accountId, folder, name, data, done, failed](const QString &message) {
            if (!self)
                return;
            if (!guard) {
                self->endJob(job, QString(), stoppedMessage());
                return;
            }
            self->endJob(job, QString(), message);
            if (self->m_log) {
                // The same data goes to the same folder, reporting to the same
                // callbacks
                self->m_log->setRetry(job, [self, job, guard, accountId, folder, name, data, done, failed]() {
                    if (self)
                        self->storeJob(job, accountId, folder, name, data, guard, done, failed);
                });
            }
            failed(message);
        },
        [self, job](qint64 sent, qint64 total) {
            if (self)
                self->jobProgress(job, sent, total);
        });
}

void NextcloudTransfers::storeAs(const QString &accountId, const QString &folder, const QString &name,
    const QByteArray &data, int number, QPointer<QObject> context, std::function<void(const QString &path)> done,
    Failed failed, Client::Progress progress)
{
    const QString path = Nextcloud::childPath(folder, number == 1 ? name : Nextcloud::numberedName(name, number));
    run(
        accountId, context,
        [this, accountId, folder, name, data, number, context, done, failed, path, progress](
            Client *client, Client::Failed onFailure) {
            QPointer<NextcloudTransfers> self(this);
            client->upload(
                path, data, false, [done, path]() { done(path); },
                [self, accountId, folder, name, data, number, context, done, failed, onFailure, progress](
                    Client::Failure failure, const QString &message) {
                    if (failure == Client::Failure::Exists && number < maxNumberedNames && self && context) {
                        self->storeAs(accountId, folder, name, data, number + 1, context, done, failed, progress);
                        return;
                    }
                    if (failure == Client::Failure::Exists) {
                        failed(i18nc("@info", "Too many files named like %1 exist in %2.", name, folder));
                        return;
                    }
                    if (failure == Client::Failure::Permission) {
                        failed(i18nc("@info", "You may not create files in the Nextcloud folder %1.", folder));
                        return;
                    }
                    if (failure == Client::Failure::NotFound) {
                        failed(i18nc("@info", "The Nextcloud folder %1 no longer exists.", folder));
                        return;
                    }
                    onFailure(failure, message);
                },
                progress);
        },
        failed);
}

void NextcloudTransfers::linkShares(const QString &accountId, const QList<Nextcloud::Entry> &files, QObject *context,
    std::function<void(const QList<Link> &links)> done)
{
    struct State {
        QList<Link> links;
        int remaining = 0;
    };
    auto state = std::make_shared<State>();
    for (const Nextcloud::Entry &file : files)
        state->links.append(Link{file, {}, {}});
    state->remaining = files.size();
    QPointer<QObject> guard(context);
    if (files.isEmpty()) {
        if (guard)
            done({});
        return;
    }

    ++m_running;
    QPointer<NextcloudTransfers> self(this);
    auto token = std::make_shared<RunningToken>([self]() {
        if (self)
            --self->m_running;
    });
    auto finishOne = [token, state, guard, done]() {
        if (--state->remaining == 0 && guard)
            done(state->links);
    };

    for (int i = 0; i < files.size(); ++i) {
        const QString path = files[i].path;
        run(
            accountId, guard,
            [state, i, path, finishOne](Client *client, Client::Failed onFailure) {
                client->shares(
                    path,
                    [state, i, finishOne](const QList<Nextcloud::Share> &shares) {
                        for (const Nextcloud::Share &share : shares) {
                            if (share.isLink() && share.url.isValid()) {
                                state->links[i].url = share.url;
                                break;
                            }
                        }
                        finishOne();
                    },
                    onFailure);
            },
            [state, i, finishOne](const QString &message) {
                state->links[i].error = message;
                finishOne();
            });
    }
}

} // namespace Gui
