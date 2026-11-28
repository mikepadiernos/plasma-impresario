/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Gui/NextcloudShareReviewDialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "Gui/NextcloudHub.h"
#include "Nextcloud/Shares.h"

namespace Gui {

NextcloudShareReviewDialog::NextcloudShareReviewDialog(NextcloudHub *hub, const QUrl &link, QWidget *parent)
    : QDialog(parent)
    , m_hub(hub)
    , m_link(link)
    , m_linkLabel(new QLabel(this))
    , m_accountLabel(new QLabel(this))
    , m_detailsLabel(new QLabel(this))
    , m_status(new QLabel(this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this))
    , m_insert(new QPushButton(i18nc("@action:button", "Insert into New Message"), this))
{
    setWindowTitle(i18nc("@title:window", "Shared Nextcloud Link"));
    setObjectName(QStringLiteral("nextcloudShareReview"));

    auto *intro = new QLabel(i18nc("@info",
                                 "Another application asked to put this "
                                 "Nextcloud link into a new message. Check "
                                 "who can open it before you send it."),
        this);
    intro->setWordWrap(true);
    for (QLabel *label : {m_linkLabel, m_accountLabel, m_detailsLabel, m_status}) {
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    m_linkLabel->setObjectName(QStringLiteral("nextcloudShareLink"));
    m_linkLabel->setText(link.toDisplayString());
    m_accountLabel->setObjectName(QStringLiteral("nextcloudShareAccount"));
    m_detailsLabel->setObjectName(QStringLiteral("nextcloudShareDetails"));
    m_status->setObjectName(QStringLiteral("nextcloudShareStatus"));
    m_insert->setObjectName(QStringLiteral("nextcloudShareInsert"));
    m_insert->setEnabled(false);
    m_buttons->addButton(m_insert, QDialogButtonBox::AcceptRole);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label", "Link:"), m_linkLabel);
    form->addRow(i18nc("@label", "Shared from:"), m_accountLabel);
    form->addRow(i18nc("@label", "Who can open it:"), m_detailsLabel);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->addWidget(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (!canInsert())
            return;
        emit insertRequested(m_link);
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!isShareLink(link)) {
        m_accountLabel->setText(i18nc("@info", "Unknown"));
        m_detailsLabel->setText(i18nc("@info", "Unknown"));
        m_status->setText(i18nc("@info",
            "This is not a Nextcloud share link, so "
            "it is not put into a message."));
        return;
    }
    if (m_hub) {
        for (const Nextcloud::Account &account : m_hub->accounts()) {
            if (!Nextcloud::shareToken(link, account.server).isEmpty())
                m_candidates << account.id;
        }
    }
    m_busy = true;
    m_accountLabel->setText(i18nc("@info", "Looking up the share…"));
    lookUp(0);
}

NextcloudShareReviewDialog::~NextcloudShareReviewDialog()
{
    if (auto *client = qobject_cast<Nextcloud::Client *>(m_client.data()); client && m_request)
        client->abort(m_request);
}

QUrl NextcloudShareReviewDialog::link() const
{
    return m_link;
}

bool NextcloudShareReviewDialog::canInsert() const
{
    return m_insert->isEnabled();
}

bool NextcloudShareReviewDialog::isBusy() const
{
    return m_busy;
}

QString NextcloudShareReviewDialog::accountId() const
{
    return m_accountId;
}

QString NextcloudShareReviewDialog::detailsText() const
{
    return m_detailsLabel->text();
}

bool NextcloudShareReviewDialog::isShareLink(const QUrl &link)
{
    const QList<QUrl> found = Nextcloud::findShareLinks(link.toString(QUrl::FullyEncoded));
    return found.size() == 1 && found.constFirst() == link;
}

void NextcloudShareReviewDialog::lookUp(int index)
{
    if (index >= m_candidates.size() || !m_hub) {
        finish();
        return;
    }
    const QString id = m_candidates.at(index);
    const QString title = m_hub->account(id).title();
    const QString token = Nextcloud::shareToken(m_link, m_hub->account(id).server);
    QPointer<NextcloudShareReviewDialog> self(this);
    auto next = [self, index, title](const QString &problem) {
        if (!self)
            return;
        self->m_client = nullptr;
        self->m_request = 0;
        if (!problem.isEmpty())
            self->m_problems << i18nc("@info %1 is an account, %2 an error message", "%1: %2", title, problem);
        self->lookUp(index + 1);
    };
    m_hub->withClient(
        id, this,
        [self, id, token, next](Nextcloud::Client *client) {
            if (!self)
                return;
            self->m_client = client;
            // Only the user's own shares are read; nothing is created or changed
            self->m_request = client->shares(
                QString(),
                [self, id, token, next](const QList<Nextcloud::Share> &shares) {
                    if (!self)
                        return;
                    if (const Nextcloud::Share *share = Nextcloud::findLinkShare(shares, token)) {
                        self->m_client = nullptr;
                        self->m_request = 0;
                        self->m_accountId = id;
                        self->m_detailLines = Nextcloud::describe(share);
                        self->finish();
                        return;
                    }
                    next(QString());
                },
                [self, id, next](Nextcloud::Client::Failure failure, const QString &message) {
                    if (failure == Nextcloud::Client::Failure::Authentication && self && self->m_hub) {
                        const QString refused = self->m_hub->refusedMessage(id);
                        self->m_hub->forgetClient(id);
                        next(refused);
                        return;
                    }
                    next(message);
                });
        },
        next);
}

void NextcloudShareReviewDialog::finish()
{
    m_busy = false;
    QStringList status;
    if (m_accountId.isEmpty()) {
        m_detailLines = Nextcloud::describe(nullptr);
        m_accountLabel->setText(m_candidates.isEmpty()
                ? i18nc("@info", "No connected Nextcloud account is on this server.")
                : i18nc("@info",
                      "Not one of the shares of your connected "
                      "Nextcloud accounts."));
        status << i18nc("@info",
            "Insert the link only when you trust where it "
            "came from; the people you send it to can "
            "open whatever it leads to.");
        if (!m_problems.isEmpty()) {
            status << i18nc("@info %1 is a list of error messages", "The shares could not be checked: %1",
                m_problems.join(QLatin1Char(' ')));
        }
    } else {
        m_accountLabel->setText(m_hub ? m_hub->account(m_accountId).title() : QString());
    }
    m_detailsLabel->setText(m_detailLines.join(QLatin1Char('\n')));
    m_status->setText(status.join(QLatin1Char('\n')));
    m_status->setVisible(!status.isEmpty());
    m_insert->setEnabled(true);
    m_insert->setFocus();
}

} // namespace Gui
