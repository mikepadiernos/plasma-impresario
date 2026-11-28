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
    m_busy = true;
    m_accountLabel->setText(i18nc("@info", "Looking up the share…"));
    if (!m_hub) {
        finish(NextcloudHub::LinkShare{link, {}, Nextcloud::describe(nullptr), {}, false});
        return;
    }
    m_hub->lookUpShares({link}, this, [this, link](const QList<NextcloudHub::LinkShare> &shares) {
        finish(shares.value(0, NextcloudHub::LinkShare{link, {}, Nextcloud::describe(nullptr), {}, false}));
    });
}

NextcloudShareReviewDialog::~NextcloudShareReviewDialog() = default;

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

void NextcloudShareReviewDialog::finish(const NextcloudHub::LinkShare &share)
{
    m_busy = false;
    m_accountId = share.accountId;
    QStringList status;
    if (m_accountId.isEmpty()) {
        m_accountLabel->setText(!share.knownServer ? i18nc("@info", "No connected Nextcloud account is on this server.")
                                                   : i18nc("@info",
                                                         "Not one of the shares of your connected "
                                                         "Nextcloud accounts."));
        status << i18nc("@info",
            "Insert the link only when you trust where it "
            "came from; the people you send it to can "
            "open whatever it leads to.");
        if (!share.problems.isEmpty()) {
            status << i18nc("@info %1 is a list of error messages", "The shares could not be checked: %1",
                share.problems.join(QLatin1Char(' ')));
        }
    } else {
        m_accountLabel->setText(m_hub ? m_hub->account(m_accountId).title() : QString());
    }
    m_detailsLabel->setText(share.details.join(QLatin1Char('\n')));
    m_status->setText(status.join(QLatin1Char('\n')));
    m_status->setVisible(!status.isEmpty());
    m_insert->setEnabled(true);
    m_insert->setFocus();
}

} // namespace Gui
