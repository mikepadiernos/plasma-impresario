/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "NotificationsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace Gui {

using UiUtils::NewMail::Mode;

namespace {

const QString inbox = QStringLiteral("INBOX");

}

NotificationsDialog::NotificationsDialog(const Accounts &accounts, const UiUtils::NewMail::Settings &settings,
                                         QWidget *parent)
    : QDialog(parent)
    , m_accounts(accounts)
    , m_settings(settings)
    , m_account(new QComboBox(this))
    , m_mode(new QComboBox(this))
    , m_inbox(new QCheckBox(i18nc("@option:check", "Announce new mail in the Inbox"), this))
    , m_folders(new QPlainTextEdit(this))
    , m_summaryMinutes(new QSpinBox(this))
    , m_quietWhileActive(new QCheckBox(i18nc("@option:check", "Wait while the window is in use"), this))
    , m_showContent(new QCheckBox(i18nc("@option:check", "Show senders and subjects"), this))
{
    setObjectName(QStringLiteral("notificationsDialog"));
    setWindowTitle(i18nc("@title:window", "Configure Notifications"));
    m_account->setObjectName(QStringLiteral("notifyAccount"));
    m_mode->setObjectName(QStringLiteral("notifyMode"));
    m_inbox->setObjectName(QStringLiteral("notifyInbox"));
    m_folders->setObjectName(QStringLiteral("notifyFolders"));
    m_summaryMinutes->setObjectName(QStringLiteral("notifySummaryMinutes"));
    m_quietWhileActive->setObjectName(QStringLiteral("notifyQuietWhileActive"));
    m_showContent->setObjectName(QStringLiteral("notifyShowContent"));

    for (const auto &account : std::as_const(m_accounts))
        m_account->addItem(account.second, account.first);
    m_mode->addItem(i18nc("@item:inlistbox notification mode", "Each message, in one updated notification"),
                    static_cast<int>(Mode::Each));
    m_mode->addItem(i18nc("@item:inlistbox notification mode", "A summary"), static_cast<int>(Mode::Summary));
    m_mode->addItem(i18nc("@item:inlistbox notification mode", "Nothing"), static_cast<int>(Mode::Off));
    m_folders->setPlaceholderText(i18nc("@info:placeholder", "One folder per line, such as Lists/KDE"));
    m_folders->setTabChangesFocus(true);
    m_summaryMinutes->setRange(1, 240);
    m_summaryMinutes->setSuffix(i18nc("@item:valuesuffix minutes", " min"));
    m_summaryMinutes->setValue(m_settings.summaryMinutes);
    m_quietWhileActive->setChecked(m_settings.quietWhileActive);
    m_quietWhileActive->setToolTip(i18nc("@info:tooltip", "Mail which arrives in the account shown while you use the "
                                                          "window appears in the list and the status bar only"));
    m_showContent->setChecked(m_settings.showContent);
    m_showContent->setToolTip(i18nc("@info:tooltip", "Otherwise notifications show only counts and folders"));

    auto *layout = new QVBoxLayout(this);
    auto *accountBox = new QGroupBox(i18nc("@title:group", "Account"), this);
    auto *accountForm = new QFormLayout(accountBox);
    accountForm->addRow(i18nc("@label:listbox", "Account:"), m_account);
    accountForm->addRow(i18nc("@label:listbox", "Announce:"), m_mode);
    accountForm->addRow(QString(), m_inbox);
    accountForm->addRow(i18nc("@label:textbox", "Other folders:"), m_folders);
    layout->addWidget(accountBox);
    auto *generalBox = new QGroupBox(i18nc("@title:group", "Every Account"), this);
    auto *generalForm = new QFormLayout(generalBox);
    generalForm->addRow(i18nc("@label:spinbox", "Summary at most every:"), m_summaryMinutes);
    generalForm->addRow(QString(), m_quietWhileActive);
    generalForm->addRow(QString(), m_showContent);
    layout->addWidget(generalBox);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(box);

    if (m_accounts.isEmpty()) {
        accountBox->setEnabled(false);
        m_account->addItem(i18nc("@item:inlistbox", "No accounts"));
    }

    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_account, &QComboBox::currentIndexChanged, this, [this]() {
        storeAccount();
        loadAccount();
    });
    connect(m_mode, &QComboBox::currentIndexChanged, this, &NotificationsDialog::updateEnabled);
    loadAccount();
}

void NotificationsDialog::selectAccount(const QString &accountId)
{
    const int index = m_account->findData(accountId);
    if (index >= 0)
        m_account->setCurrentIndex(index);
}

void NotificationsDialog::storeAccount() const
{
    const QString id = m_shownAccount;
    if (id.isEmpty())
        return;
    const auto mode = static_cast<Mode>(m_mode->currentData().toInt());
    if (mode == Mode::Each)
        m_settings.accountModes.remove(id);
    else
        m_settings.accountModes.insert(id, mode);
    m_settings.setAnnounces(id, inbox, m_inbox->isChecked());
    QStringList wanted;
    const QStringList lines = m_folders->toPlainText().split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString folder = line.trimmed();
        if (!folder.isEmpty() && folder.compare(inbox, Qt::CaseInsensitive) != 0 && !wanted.contains(folder))
            wanted << folder;
    }
    for (const QString &folder : m_settings.extraFolders(id)) {
        if (!wanted.contains(folder))
            m_settings.setAnnounces(id, folder, false);
    }
    for (const QString &folder : std::as_const(wanted))
        m_settings.setAnnounces(id, folder, true);
}

void NotificationsDialog::loadAccount()
{
    m_shownAccount = m_account->currentData().toString();
    const QString id = m_shownAccount;
    QSignalBlocker blocker(m_mode);
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(m_settings.mode(id))));
    m_inbox->setChecked(m_settings.announces(id, inbox));
    m_folders->setPlainText(m_settings.extraFolders(id).join(QLatin1Char('\n')));
    updateEnabled();
}

void NotificationsDialog::updateEnabled()
{
    const bool on = static_cast<Mode>(m_mode->currentData().toInt()) != Mode::Off;
    m_inbox->setEnabled(on);
    m_folders->setEnabled(on);
}

UiUtils::NewMail::Settings NotificationsDialog::settings() const
{
    storeAccount();
    m_settings.summaryMinutes = m_summaryMinutes->value();
    m_settings.quietWhileActive = m_quietWhileActive->isChecked();
    m_settings.showContent = m_showContent->isChecked();
    return m_settings;
}

}
