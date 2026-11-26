/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "EnvelopeView.h"
#include <KLocalizedString>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrlQuery>
#include <QGridLayout>
#include <QLayoutItem>
#include <QVBoxLayout>
#include "Gui/AddressRowWidget.h"
#include "Gui/MessageView.h"
#include "Gui/Util.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "UiUtils/Formatting.h"

namespace Gui {

EnvelopeView::EnvelopeView(QWidget *parent, Plugins::PluginManager *pluginManager): QWidget(parent), m_pluginManager(pluginManager)
    , m_openUrl([](const QUrl &url) { QDesktopServices::openUrl(url); })
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    setLayout(lay);

}

#define SET_LABEL_OPTIONS(LBL) \
{ \
    LBL->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); \
    LBL->setWordWrap(true); \
    LBL->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse); \
}

#define ADD_ROW(LBL, WDG) \
{ \
    QLabel *l = new QLabel(LBL, this); \
    l->setAlignment(Qt::AlignRight); \
    form->addWidget(l, form->rowCount(), 0, Qt::AlignTop); \
    form->addWidget(WDG, form->rowCount()-1, 1, Qt::AlignTop); \
}

void EnvelopeView::setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver)
{
    m_accountName = resolver;
}

void EnvelopeView::setUrlOpener(const std::function<void(const QUrl &)> &opener)
{
    m_openUrl = opener;
}

namespace {

using ListAction = UiUtils::MailingLists::Action;

QString listActionText(ListAction action)
{
    switch (action) {
    case ListAction::Post:
        return i18nc("@action mailing list", "Post");
    case ListAction::Subscribe:
        return i18nc("@action mailing list", "Subscribe");
    case ListAction::Unsubscribe:
        return i18nc("@action mailing list", "Unsubscribe");
    case ListAction::Help:
        return i18nc("@action mailing list", "Help");
    case ListAction::Archive:
        return i18nc("@action mailing list", "Archive");
    case ListAction::Owner:
        return i18nc("@action mailing list", "Owner");
    }
    return QString();
}

QString listActionQuestion(ListAction action, const QString &list)
{
    switch (action) {
    case ListAction::Post:
        return i18nc("@info %1 is a mailing list", "Write a message to %1?", list);
    case ListAction::Subscribe:
        return i18nc("@info %1 is a mailing list", "Subscribe to %1?", list);
    case ListAction::Unsubscribe:
        return i18nc("@info %1 is a mailing list", "Unsubscribe from %1?", list);
    case ListAction::Help:
        return i18nc("@info %1 is a mailing list", "Get help about %1?", list);
    case ListAction::Archive:
        return i18nc("@info %1 is a mailing list", "Open the archive of %1?", list);
    case ListAction::Owner:
        return i18nc("@info %1 is a mailing list", "Contact the owner of %1?", list);
    }
    return QString();
}

}

void EnvelopeView::requestListAction(int position)
{
    if (position < 0 || position >= m_listActions.size())
        return;
    const UiUtils::MailingLists::ActionTarget target = m_listActions.at(position);
    const bool mail = target.url.scheme().compare(QLatin1String("mailto"), Qt::CaseInsensitive) == 0;
    // The headers come from whoever sent the message, so the user sees where an action leads before following it
    const QString where = mail
        ? i18nc("@info", "This opens a new message to <b>%1</b>, which you can check before sending it.",
                target.url.path().toHtmlEscaped())
        : i18nc("@info", "This opens <b>%1</b> in your web browser.", target.url.toDisplayString().toHtmlEscaped());
    // Newsletters often offer List-Unsubscribe without naming their list
    const QString list = m_list.isValid() ? m_list.displayName().toHtmlEscaped()
                                          : i18nc("@info the list of a list action question", "this mailing list");
    auto *box = new QMessageBox(QMessageBox::Question, i18nc("@title:window", "Mailing List"),
                                QStringLiteral("<p>%1</p><p>%2</p>").arg(listActionQuestion(target.action, list), where),
                                QMessageBox::Cancel, this);
    box->setObjectName(QStringLiteral("listActionConfirmation"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setTextFormat(Qt::RichText);
    QPushButton *follow = box->addButton(mail ? i18nc("@action:button", "Write Message") : i18nc("@action:button", "Open Link"),
                                         QMessageBox::AcceptRole);
    follow->setObjectName(QStringLiteral("followListAction"));
    box->setDefaultButton(QMessageBox::Cancel);
    connect(box, &QMessageBox::finished, this, [this, box, follow, url = target.url]() {
        if (box->clickedButton() == follow && m_openUrl)
            m_openUrl(url);
    });
    box->open();
}

/** @short */
void EnvelopeView::setMessage(const QModelIndex &index)
{
    while (QLayoutItem *item = layout()->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        } else if (item->layout()) {
            item->layout()->deleteLater();
        } else {
            delete item;
        }
    }
    Q_FOREACH(QWidget *w, findChildren<QWidget*>()) {
        w->deleteLater();
    }

    m_list = UiUtils::MailingList();
    m_listActions.clear();
    if (!index.isValid()) {
        return;
    }

    const Imap::Message::Envelope e = index.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();

    // Subject & date
    QString subDate;
    // Date
    if (e.date.isValid()) {
        subDate = QStringLiteral("<table style=\"margin:0px; margin-left:4em; float:right;\"><tr style=\"margin:0px;\"><td style=\"margin:0px;\">%1</td></tr></table>")
                      .arg(QLocale::system().toString(e.date.toLocalTime(), QLocale::LongFormat));
    }
    subDate += QStringLiteral("<span style=\"font:bold large;\">%1</span>").arg(e.subject.toHtmlEscaped());
    auto lbl = new QLabel(subDate, this);
    SET_LABEL_OPTIONS(lbl)
    layout()->addWidget(lbl);

    QGridLayout *form = new QGridLayout();
    form->setSpacing(0);
    form->setContentsMargins(0, 0, 0, 0);
    static_cast<QBoxLayout*>(layout())->addLayout(form);

    // Sender
    AddressRowWidget *senderWidget = 0;
    QString senderLabel;
    if (!e.from.isEmpty()) {
        senderLabel = tr("From");
        senderWidget = new AddressRowWidget(this, QString(), e.from, m_pluginManager);
    }
    if (!e.sender.isEmpty() && e.sender != e.from) {
        if (senderWidget) {
            senderWidget->addAddresses(QStringLiteral(" %1").arg(tr("sent via")), e.sender, m_pluginManager);
        } else {
            senderLabel = tr("Sender");
            senderWidget = new AddressRowWidget(this, QString(), e.sender, m_pluginManager);
        }
    }
    if (!e.replyTo.isEmpty() && e.replyTo != e.from) {
        if (senderWidget) {
            senderWidget->addAddresses(QStringLiteral(", %1").arg(tr("replies to")), e.replyTo, m_pluginManager);
        } else {
            senderLabel = tr("Reply-To");
            senderWidget = new AddressRowWidget(this, QString(), e.replyTo, m_pluginManager);
        }
    }
    if (senderWidget)
        ADD_ROW(senderLabel, senderWidget)

    // Receiver
    AddressRowWidget *receiverWidget = 0;
    QString receiverLabel;
    if (!e.to.isEmpty()) {
        receiverLabel = tr("To");
        receiverWidget = new AddressRowWidget(this, QString(), e.to, m_pluginManager);
    }
    if (!e.cc.isEmpty()) {
        if (receiverWidget) {
            receiverWidget->addAddresses(QStringLiteral(" %1").arg(tr("Cc'd to")), e.cc, m_pluginManager);
        } else {
            receiverLabel = tr("Cc");
            receiverWidget = new AddressRowWidget(this, QString(), e.cc, m_pluginManager);
        }
    }
    if (!e.bcc.isEmpty()) {
        if (receiverWidget) {
            receiverWidget->addAddresses(QStringLiteral(" %1").arg(tr("Bcc'd to")), e.bcc, m_pluginManager);
        } else {
            receiverLabel = tr("Bcc");
            receiverWidget = new AddressRowWidget(this, QString(), e.bcc, m_pluginManager);
        }
    }
    if (receiverWidget)
        ADD_ROW(receiverLabel, receiverWidget)

    // Mailing list: name it, and offer the actions of its List-* headers, which ask before doing anything
    m_list = UiUtils::MailingLists::fromMessage(index);
    m_listActions = UiUtils::MailingLists::actions(index);
    if (m_list.isValid() || !m_listActions.isEmpty()) {
        QString text;
        if (m_list.isValid()) {
            text = m_list.name.isEmpty() ? m_list.searchValue().toHtmlEscaped()
                                         : i18nc("@info mailing list name and ID", "%1 (%2)", m_list.name.toHtmlEscaped(),
                                                 m_list.searchValue().toHtmlEscaped());
        }
        QStringList links;
        for (qsizetype i = 0; i < m_listActions.size(); ++i) {
            const QUrl &url = m_listActions.at(i).url;
            links << QStringLiteral("<a href=\"list-action:%1\" title=\"%2\">%3</a>")
                         .arg(QString::number(i), url.toDisplayString().toHtmlEscaped(),
                              listActionText(m_listActions.at(i).action).toHtmlEscaped());
        }
        if (!links.isEmpty())
            text += (text.isEmpty() ? QString() : QStringLiteral(" &nbsp; ")) + links.join(QStringLiteral(" &middot; "));
        auto lbl = new QLabel(QStringLiteral("<html>&nbsp;%1</html>").arg(text), this);
        lbl->setObjectName(QStringLiteral("envelopeMailingList"));
        SET_LABEL_OPTIONS(lbl)
        lbl->setOpenExternalLinks(false);
        connect(lbl, &QLabel::linkHovered, lbl, [this, lbl](const QString &link) {
            const int position = link.startsWith(QLatin1String("list-action:")) ? link.mid(12).toInt() : -1;
            lbl->setToolTip(position >= 0 && position < m_listActions.size()
                                ? m_listActions.at(position).url.toDisplayString()
                                : QString());
        });
        connect(lbl, &QLabel::linkActivated, this, [this](const QString &link) {
            if (link.startsWith(QLatin1String("list-action:")))
                requestListAction(link.mid(12).toInt());
        });
        ADD_ROW(i18nc("@label", "Mailing List"), lbl)
    }

    // The account matters once several of them are set up, above all in the Unified Inbox
    const QString account = m_accountName ? m_accountName(index) : QString();
    if (!account.isEmpty()) {
        auto lbl = new QLabel(QStringLiteral("<html>&nbsp;%1</html>").arg(account.toHtmlEscaped()), this);
        lbl->setObjectName(QStringLiteral("envelopeAccount"));
        SET_LABEL_OPTIONS(lbl)
        ADD_ROW(i18nc("@label the account a message belongs to", "Account"), lbl)
    }

    // separating the message
    QFrame *line = new QFrame(this);
    line->setFrameStyle(QFrame::HLine|QFrame::Plain);
    layout()->addWidget(line);
}


}
