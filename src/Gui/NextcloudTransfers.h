/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QUrl>

#include "Nextcloud/Client.h"

class QWidget;

namespace Gui {

class NextcloudHub;

/**
 * @short Files moved between the mail and a connected Nextcloud account:
 * attaching files, saving attachments and looking up existing share links
 *
 * Owned by the main window. Each transfer reports to its continuation or its
 * failure callback, once, and only while its context object exists. A
 * certificate the system does not trust is offered to the user, and the
 * transfer runs again when it is trusted; an app password the server refuses
 * makes the account sign in again next time.
 *
 * Transfers are held in memory. Shares are only read: a file without a link
 * share gets none.
 */
class NextcloudTransfers : public QObject
{
    Q_OBJECT
public:
    using Failed = std::function<void(const QString &message)>;

    /** @short The link share found for a file, if any */
    struct Link {
        Nextcloud::Entry file;
        /** Empty when the file has no link share */
        QUrl url;
        /** Why the shares could not be read, when they could not */
        QString error;
    };

    explicit NextcloudTransfers(NextcloudHub *hub, QObject *parent = nullptr);
    ~NextcloudTransfers() override;

    /** @short Download @arg file of account @arg accountId */
    void fetch(const QString &accountId, const Nextcloud::Entry &file, QObject *context,
        std::function<void(const QByteArray &data)> done, Failed failed);
    /**
     * @short Upload @arg data as @arg name into @arg folder, never replacing a
     * file
     *
     * When the name is taken, "name (2).ext" and so on are tried. @arg done gets
     * the path of the new file.
     */
    void store(const QString &accountId, const QString &folder, const QString &name, const QByteArray &data,
        QObject *context, std::function<void(const QString &path)> done, Failed failed);
    /** @short The existing link shares of @arg files, in their order; none is
     * created */
    void linkShares(const QString &accountId, const QList<Nextcloud::Entry> &files, QObject *context,
        std::function<void(const QList<Link> &links)> done);

    /** @short Transfers which have not finished */
    int runningCount() const;

    /** The most names tried by store() before it gives up */
    static constexpr int maxNumberedNames = 99;

private:
    using Attempt = std::function<void(Nextcloud::Client *client, Nextcloud::Client::Failed failed)>;

    /** Run @arg attempt with the client of @arg accountId, again after the user
     * trusts a certificate */
    void run(const QString &accountId, QPointer<QObject> context, Attempt attempt, Failed failed);
    void storeAs(const QString &accountId, const QString &folder, const QString &name, const QByteArray &data,
        int number, QPointer<QObject> context, std::function<void(const QString &path)> done, Failed failed);
    static QWidget *dialogParent(QObject *context);

    QPointer<NextcloudHub> m_hub;
    int m_running = 0;
};

} // namespace Gui
