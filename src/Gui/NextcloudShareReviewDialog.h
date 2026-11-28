/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QPointer>
#include <QUrl>

#include "Gui/NextcloudHub.h"

class QDialogButtonBox;
class QLabel;
class QPushButton;

namespace Gui {

/**
 * @short Shows a Nextcloud share link handed over by another application before
 * it goes into a new draft
 *
 * The link is looked up among the shares of the connected accounts on its
 * server, so that who can open it, what they can do there, its password and its
 * expiry are shown. A link which is not one of the user's shares is shown with
 * a warning that nothing about it is known. Shares are only read: none is
 * created, changed or removed.
 */
class NextcloudShareReviewDialog : public QDialog
{
    Q_OBJECT
public:
    NextcloudShareReviewDialog(NextcloudHub *hub, const QUrl &link, QWidget *parent = nullptr);
    ~NextcloudShareReviewDialog() override;

    QUrl link() const;
    /** @short Whether the link can go into a draft, which needs a share link
     * whose lookup has finished */
    bool canInsert() const;
    bool isBusy() const;
    /** @short The account whose share the link is, or an empty string */
    QString accountId() const;
    /** @short What is shown about the link */
    QString detailsText() const;

    /** @short Whether @arg link looks like a public share link of a Nextcloud
     * which may go into a draft */
    static bool isShareLink(const QUrl &link);

signals:
    /** @short The user chose to put the link into a new draft */
    void insertRequested(const QUrl &link);

private:
    void finish(const NextcloudHub::LinkShare &share);

    QPointer<NextcloudHub> m_hub;
    QUrl m_link;
    QString m_accountId;
    bool m_busy = false;
    QLabel *m_linkLabel;
    QLabel *m_accountLabel;
    QLabel *m_detailsLabel;
    QLabel *m_status;
    QDialogButtonBox *m_buttons;
    QPushButton *m_insert;
};

} // namespace Gui
