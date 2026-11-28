/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QDeadlineTimer>
#include <QDialog>
#include <QPointer>

#include "Nextcloud/Client.h"

class QButtonGroup;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTimer;
class QWidget;

namespace Gui {

class NextcloudHub;

/**
 * @short Connect a Nextcloud account, by signing in in the browser or with an
 * app password
 *
 * Signing in in the browser uses Login Flow v2: the server's sign-in page opens
 * in the default browser, where the user grants access, and the dialog asks the
 * server every second, for at most 20 minutes, whether that happened. The
 * server then hands out an app password for Plasma Impresario, which the user
 * can revoke on the server. An app password the user made under Security in
 * the Nextcloud personal settings works the same way.
 *
 * The account and its app password are kept by the hub only after the server
 * confirmed who they belong to. Connecting the same user on the same server
 * again replaces the account.
 */
class NextcloudConnectDialog : public QDialog
{
    Q_OBJECT
public:
    using UrlOpener = std::function<void(const QUrl &)>;

    explicit NextcloudConnectDialog(NextcloudHub *hub, QWidget *parent = nullptr);
    ~NextcloudConnectDialog() override;

    void setServer(const QString &server);
    /** @short Whether to sign in with an app password instead of in the browser
     */
    void setUseAppPassword(bool useAppPassword);
    void setLoginName(const QString &loginName);
    void setAppPassword(const QString &appPassword);
    /** @short How the sign-in page opens; in the default browser unless replaced
     */
    void setUrlOpener(UrlOpener opener);
    /** @short Milliseconds between asking whether the sign-in is finished; 1000
     * unless changed */
    void setPollInterval(int milliseconds);
    /** @short How long the sign-in in the browser may take; 20 minutes unless
     * changed */
    void setPollLimit(int milliseconds);

    QString statusText() const;
    bool isBusy() const;
    /** @short The account connected last, or an empty string */
    QString accountId() const;

public slots:
    /** @short Start connecting with what is filled in */
    void connectAccount();
    /** @short Stop the sign-in and close */
    void reject() override;

signals:
    void connected(const QString &id);

private:
    void updateWidgets();
    void setStatus(const QString &text);
    void setBusy(bool busy);
    void stop();
    void startLogin();
    void schedulePoll();
    void poll();
    void identify(const QUrl &server, const QString &loginName, const QString &appPassword);
    void fail(Nextcloud::Client::Failure failure, const QString &message, const std::function<void()> &retry);

    QPointer<NextcloudHub> m_hub;
    QLineEdit *m_server;
    QRadioButton *m_browser;
    QRadioButton *m_password;
    QButtonGroup *m_method;
    QWidget *m_passwordFields;
    QLineEdit *m_loginName;
    QLineEdit *m_appPassword;
    QLabel *m_status;
    QPushButton *m_reopen;
    QDialogButtonBox *m_buttons;
    QPushButton *m_connect;
    QTimer *m_pollTimer;

    Nextcloud::Client *m_client = nullptr;
    Nextcloud::Client::LoginFlow m_flow;
    QDeadlineTimer m_deadline;
    int m_pollLimit = 20 * 60 * 1000;
    bool m_busy = false;
    QString m_accountId;
    UrlOpener m_opener;
};

} // namespace Gui
