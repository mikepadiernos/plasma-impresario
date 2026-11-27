/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <QDialog>

#include "UiUtils/ServerDiscovery.h"

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

namespace Gui {

/** @short Ask for a new account's address and find its IMAP and SMTP servers

Find Settings looks the address up through UiUtils::ServerDiscovery and shows what it found and where. Nothing signs in
here: the password or the OAuth 2.0 sign-in is entered in the account settings afterwards. The account can also be
added without servers, which are then entered by hand.
*/
class AddAccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddAccountDialog(QWidget *parent = nullptr,
                              std::unique_ptr<UiUtils::ServerDiscovery::Network> network = nullptr);
    ~AddAccountDialog() override;

    /** @short The account name, which is the email address unless another name was typed */
    QString accountName() const;
    QString emailAddress() const;
    QString realName() const;
    /** @short The servers found for the current address; invalid when none were found or none were looked up */
    UiUtils::ServerDiscovery::Result result() const;
    /** @short Whether the account signs in with OAuth 2.0 rather than a password */
    bool useOAuth() const;

private:
    void findSettings();
    void stageChanged(UiUtils::ServerDiscovery::Source source);
    void discoveryFinished(const UiUtils::ServerDiscovery::Result &result);
    void addressEdited();
    void showResult();
    void setBusy(bool busy);
    void updateButtons();
    QString domainForDisplay() const;
    QString providerForDisplay() const;

    UiUtils::ServerDiscovery::Discovery *m_discovery;
    UiUtils::ServerDiscovery::Result m_result;
    QLineEdit *m_realName;
    QLineEdit *m_emailAddress;
    QLineEdit *m_accountName;
    QPushButton *m_find;
    QPushButton *m_stop;
    QProgressBar *m_progress;
    QLabel *m_status;
    QGroupBox *m_servers;
    QLabel *m_provider;
    QLabel *m_imap;
    QLabel *m_smtp;
    QLabel *m_source;
    QLabel *m_guidance;
    QCheckBox *m_oauth;
    QLabel *m_oauthNote;
    QLabel *m_warning;
    QCheckBox *m_confirm;
    QDialogButtonBox *m_buttons;
};

}
