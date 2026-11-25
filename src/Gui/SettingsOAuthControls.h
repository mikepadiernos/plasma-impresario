/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GUI_SETTINGSOAUTHCONTROLS_H
#define GUI_SETTINGSOAUTHCONTROLS_H

#include <QObject>
#include <QPointer>
#include "OAuth/Session.h"

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSettings;
class QWidget;

namespace Gui
{

/** @short The OAuth provider, client and sign-in rows of the IMAP settings page

The rows are inserted into the page's form layout. Sign-in uses the values currently shown, so an account can be
authorized before its settings are saved; the stored grant only applies while the saved provider and client ID match.
*/
class SettingsOAuthControls : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Unknown,
        NotConfigured,
        Checking,
        SignedOut,
        Authorizing,
        SignedIn,
        Failed,
    };

    SettingsOAuthControls(QFormLayout *layout, int row, const QSettings &settings, const QString &accountId,
                          OAuth::TokenStore::PluginGetter passwordPlugin, QWidget *page);
    ~SettingsOAuthControls() override;

    /** @short Show or hide all rows; the sign-in state is first checked when they are shown */
    void setActive(bool active);
    void setLoginHint(const QString &loginHint);
    OAuth::AccountConfig accountConfig() const;
    /** @short Write the provider and client to the account settings */
    void save(QSettings &settings) const;
    State state() const;
    /** @short The session for the values currently shown */
    OAuth::Session *session();

    QComboBox *provider;
    QLineEdit *clientId;
    QLineEdit *clientSecret;
    QPushButton *signIn;
    QPushButton *cancelSignIn;
    QPushButton *signOut;
    QLabel *status;

signals:
    void stateChanged();

private:
    void configurationEdited();
    void refreshState();
    void setState(State state, const QString &message = QString());
    void updateClientIdPlaceholder();

    QFormLayout *m_layout;
    QWidget *m_buttons;
    QString m_accountId;
    QString m_loginHint;
    OAuth::TokenStore::PluginGetter m_passwordPlugin;
    QPointer<OAuth::Session> m_session;
    State m_state = State::Unknown;
    bool m_active = false;
    bool m_hadSettings = false;
};

}

#endif
