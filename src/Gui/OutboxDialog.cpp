/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "OutboxDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Composer/Outbox.h"
#include "StateColors.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

using Composer::Outbox;

namespace {

QString when(const QDateTime &time)
{
    const QDateTime local = time.toLocalTime();
    if (local.date() == QDate::currentDate())
        return QLocale().toString(local.time(), QLocale::ShortFormat);
    return QLocale().toString(local, QLocale::ShortFormat);
}

QString stateName(const Outbox::Entry &entry)
{
    switch (entry.state) {
    case Outbox::State::Queued:
        return entry.attempts > 0 ? i18nc("@info outbox state", "Failed") : i18nc("@info outbox state", "Waiting");
    case Outbox::State::Sending:
        return i18nc("@info outbox state", "Sending");
    case Outbox::State::Submitted:
        return i18nc("@info outbox state", "Sent, Not Filed");
    case Outbox::State::Interrupted:
        return i18nc("@info outbox state", "Interrupted");
    case Outbox::State::Refused:
        return i18nc("@info outbox state", "Refused");
    }
    return {};
}

QString details(const Outbox::Entry &entry)
{
    switch (entry.state) {
    case Outbox::State::Queued:
    case Outbox::State::Submitted:
        if (entry.attempts == 0)
            return entry.state == Outbox::State::Queued
                ? i18nc("@info", "Sent once the account is online")
                : i18nc("@info folder name", "The copy is filed in %1 next", entry.sentFolder);
        return entry.nextAttempt.isValid()
            ? i18nc("@info time, error", "Trying again at %1: %2", when(entry.nextAttempt), entry.lastError)
            : entry.lastError;
    case Outbox::State::Sending:
        return i18nc("@info", "Handing the message to the outgoing server");
    case Outbox::State::Interrupted:
        return i18nc("@info",
            "The message may or may not have been sent. Send it "
            "again, or move it to the drafts.");
    case Outbox::State::Refused:
        return entry.lastError;
    }
    return {};
}

bool needsDecision(Outbox::State state)
{
    return state == Outbox::State::Interrupted || state == Outbox::State::Refused;
}

} // namespace

OutboxDialog::OutboxDialog(Outbox *outbox, const AccountName &accountName, QWidget *parent)
    : QDialog(parent)
    , m_outbox(outbox)
    , m_accountName(accountName)
    , m_tree(new QTreeWidget(this))
    , m_problem(new QLabel(this))
    , m_sendNow(
          new QPushButton(UiUtils::loadIcon(QStringLiteral("mail-send")), i18nc("@action:button", "Send Now"), this))
    , m_sendAgain(
          new QPushButton(UiUtils::loadIcon(QStringLiteral("mail-send")), i18nc("@action:button", "Send Again…"), this))
    , m_moveToDrafts(new QPushButton(
          UiUtils::loadIcon(QStringLiteral("document-edit")), i18nc("@action:button", "Move to Drafts"), this))
    , m_forget(new QPushButton(
          UiUtils::loadIcon(QStringLiteral("edit-delete")), i18nc("@action:button", "Stop Filing…"), this))
{
    setObjectName(QStringLiteral("outboxDialog"));
    setWindowTitle(i18nc("@title:window", "Outbox"));
    m_tree->setObjectName(QStringLiteral("outboxTree"));
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setWordWrap(true);
    m_tree->setHeaderLabels({i18nc("@title:column", "Subject"), i18nc("@title:column", "Account"),
        i18nc("@title:column", "State"), i18nc("@title:column", "Details")});
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tree->setAccessibleName(i18nc("@label", "Messages waiting to be sent, oldest first"));
    m_problem->setObjectName(QStringLiteral("outboxProblem"));
    m_problem->setWordWrap(true);
    m_problem->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_problem->hide();
    m_sendNow->setObjectName(QStringLiteral("outboxSendNow"));
    m_sendAgain->setObjectName(QStringLiteral("outboxSendAgain"));
    m_moveToDrafts->setObjectName(QStringLiteral("outboxMoveToDrafts"));
    m_forget->setObjectName(QStringLiteral("outboxForget"));
    m_sendAgain->setToolTip(i18nc("@info:tooltip",
        "Send the message once more, even though the "
        "recipients may get it twice"));
    m_forget->setToolTip(i18nc("@info:tooltip", "The server sent the message; give up keeping a copy in Sent"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_problem);
    auto *row = new QHBoxLayout;
    row->addWidget(m_sendNow);
    row->addWidget(m_sendAgain);
    row->addWidget(m_moveToDrafts);
    row->addWidget(m_forget);
    row->addStretch();
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    row->addWidget(box);
    layout->addLayout(row);
    resize(760, 360);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_sendNow, &QPushButton::clicked, this, &OutboxDialog::sendNow);
    connect(m_sendAgain, &QPushButton::clicked, this, &OutboxDialog::sendAgain);
    connect(m_moveToDrafts, &QPushButton::clicked, this, &OutboxDialog::moveToDrafts);
    connect(m_forget, &QPushButton::clicked, this, &OutboxDialog::forget);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &OutboxDialog::updateButtons);
    connect(m_outbox, &Outbox::changed, this, &OutboxDialog::rebuild);
    rebuild();
}

QTreeWidget *OutboxDialog::tree() const
{
    return m_tree;
}

void OutboxDialog::setConfirmation(const Confirmation &confirm)
{
    m_confirm = confirm;
}

QString OutboxDialog::selectedId() const
{
    return m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
}

void OutboxDialog::rebuild()
{
    const QString selected = selectedId();
    const int selectedRow = m_tree->currentItem() ? m_tree->indexOfTopLevelItem(m_tree->currentItem()) : -1;
    QSignalBlocker blocker(m_tree);
    m_tree->clear();
    QTreeWidgetItem *current = nullptr;
    const QList<Outbox::Entry> entries = m_outbox->entries();
    for (const Outbox::Entry &entry : entries) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, entry.subject.trimmed().isEmpty() ? i18nc("@info", "(no subject)") : entry.subject);
        item->setText(1, m_accountName ? m_accountName(entry.accountId) : entry.accountId);
        item->setText(2, stateName(entry));
        item->setText(3, details(entry));
        item->setToolTip(3, item->text(3));
        item->setData(0, Qt::UserRole, entry.id);
        if (needsDecision(entry.state) || entry.attempts > 0) {
            const QColor negative = StateColors::text(StateColors::State::Negative);
            item->setForeground(2, negative);
            item->setForeground(3, negative);
        }
        if (entry.id == selected)
            current = item;
    }
    if (!current && m_tree->topLevelItemCount() > 0)
        current = m_tree->topLevelItem(std::clamp(selectedRow, 0, m_tree->topLevelItemCount() - 1));
    if (current)
        m_tree->setCurrentItem(current);
    blocker.unblock();
    updateButtons();
}

void OutboxDialog::updateButtons()
{
    const QString id = selectedId();
    const Outbox::Entry entry = m_outbox->entry(id);
    const bool known = !entry.id.isEmpty();
    const bool busy = known && m_outbox->isBusy(id);
    m_sendNow->setEnabled(
        known && !busy && (entry.state == Outbox::State::Queued || entry.state == Outbox::State::Submitted));
    m_sendAgain->setEnabled(known && needsDecision(entry.state));
    m_moveToDrafts->setEnabled(known && !busy && entry.state != Outbox::State::Submitted);
    m_forget->setEnabled(known && !busy && entry.state == Outbox::State::Submitted);
    // A decision is easier with the whole reason in view
    const bool showProblem = known && !entry.lastError.isEmpty();
    m_problem->setText(showProblem ? entry.lastError : QString());
    m_problem->setVisible(showProblem);
}

bool OutboxDialog::confirm(const QString &title, const QString &question)
{
    if (m_confirm)
        return m_confirm(title, question);
    return QMessageBox::question(this, title, question, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        == QMessageBox::Yes;
}

void OutboxDialog::sendNow()
{
    m_outbox->sendNow(selectedId());
}

void OutboxDialog::sendAgain()
{
    const QString id = selectedId();
    const Outbox::Entry entry = m_outbox->entry(id);
    if (entry.id.isEmpty() || !needsDecision(entry.state))
        return;
    const QString question = entry.state == Outbox::State::Interrupted
        ? i18nc("@info",
              "The outgoing server may already have this message. "
              "Sending it again can deliver it twice.")
        : i18nc("@info error",
              "The server refused this message: %1\nSending the same "
              "message again usually fails "
              "again. Send it anyway?",
              entry.lastError);
    if (confirm(i18nc("@title:window", "Send Again?"), question))
        m_outbox->sendAgain(id);
}

void OutboxDialog::moveToDrafts()
{
    QString error;
    const QString path = m_outbox->moveToDrafts(selectedId(), &error);
    if (path.isEmpty()) {
        if (!error.isEmpty()) {
            m_problem->setText(error);
            m_problem->show();
        }
        return;
    }
    emit draftMoved(path);
}

void OutboxDialog::forget()
{
    const QString id = selectedId();
    const Outbox::Entry entry = m_outbox->entry(id);
    if (entry.id.isEmpty() || entry.state != Outbox::State::Submitted)
        return;
    if (confirm(i18nc("@title:window", "Stop Filing?"),
            i18nc("@info folder name", "The message was sent, but no copy will be kept in %1.", entry.sentFolder)))
        m_outbox->forget(id);
}

} // namespace Gui
