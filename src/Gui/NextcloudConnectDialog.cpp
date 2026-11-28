/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Gui/NextcloudConnectDialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>

#include "Gui/NextcloudHub.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

using Nextcloud::Client;

NextcloudConnectDialog::NextcloudConnectDialog(NextcloudHub *hub, QWidget *parent)
    : QDialog(parent)
    , m_hub(hub)
    , m_server(new QLineEdit(this))
    , m_browser(new QRadioButton(i18nc("@option:radio", "Sign in in the &browser"), this))
    , m_password(new QRadioButton(i18nc("@option:radio", "Use an &app password"), this))
    , m_method(new QButtonGroup(this))
    , m_passwordFields(new QWidget(this))
    , m_loginName(new QLineEdit(this))
    , m_appPassword(new QLineEdit(this))
    , m_status(new QLabel(this))
    , m_reopen(new QPushButton(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("internet-web-browser")),
          i18nc("@action:button", "Reopen Sign-In Page"), this))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Cancel, this))
    , m_connect(new QPushButton(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("network-connect")),
          i18nc("@action:button", "&Connect"), this))
    , m_pollTimer(new QTimer(this))
{
    setWindowTitle(i18nc("@title:window", "Connect Nextcloud Account"));
    m_server->setObjectName(QStringLiteral("nextcloudServer"));
    m_server->setPlaceholderText(i18nc("@info:placeholder", "https://cloud.example.com"));
    m_server->setClearButtonEnabled(true);
    m_browser->setObjectName(QStringLiteral("nextcloudBrowserSignIn"));
    m_browser->setChecked(true);
    m_password->setObjectName(QStringLiteral("nextcloudAppPasswordSignIn"));
    m_method->addButton(m_browser);
    m_method->addButton(m_password);
    m_loginName->setObjectName(QStringLiteral("nextcloudLoginName"));
    m_appPassword->setObjectName(QStringLiteral("nextcloudAppPassword"));
    m_appPassword->setEchoMode(QLineEdit::Password);
    m_status->setObjectName(QStringLiteral("nextcloudConnectStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_reopen->setObjectName(QStringLiteral("nextcloudReopenSignIn"));
    m_reopen->setToolTip(i18nc("@info:tooltip", "Open the server's sign-in page in the browser again"));
    m_reopen->hide();
    m_connect->setObjectName(QStringLiteral("nextcloudConnect"));
    m_connect->setDefault(true);
    m_pollTimer->setSingleShot(true);
    m_pollTimer->setInterval(1000);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "&Server:"), m_server);
    form->addRow(i18nc("@label", "Sign in:"), m_browser);
    form->addRow(QString(), m_password);

    auto *passwordForm = new QFormLayout(m_passwordFields);
    passwordForm->setContentsMargins(0, 0, 0, 0);
    passwordForm->addRow(i18nc("@label:textbox", "&Login name:"), m_loginName);
    passwordForm->addRow(i18nc("@label:textbox", "A&pp password:"), m_appPassword);
    auto *passwordNote = new QLabel(i18nc("@info",
                                        "Create an app password under Security in your Nextcloud personal "
                                        "settings. Your account password is never kept."),
        m_passwordFields);
    passwordNote->setWordWrap(true);
    passwordForm->addRow(passwordNote);

    auto *browserNote = new QLabel(i18nc("@info",
                                       "Signing in in the browser works with two-factor authentication and "
                                       "single sign-on. Nextcloud then makes an app password for Plasma "
                                       "Impresario, which you can revoke on the server at any time."),
        this);
    browserNote->setWordWrap(true);

    m_buttons->addButton(m_connect, QDialogButtonBox::ActionRole);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_reopen, 0, Qt::AlignTop);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(browserNote);
    layout->addWidget(m_passwordFields);
    layout->addLayout(statusRow);
    layout->addStretch();
    layout->addWidget(m_buttons);

    m_opener = [this](const QUrl &url) {
        auto *job = new KIO::OpenUrlJob(url, QStringLiteral("text/html"));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    };

    connect(m_server, &QLineEdit::textChanged, this, &NextcloudConnectDialog::updateWidgets);
    connect(m_loginName, &QLineEdit::textChanged, this, &NextcloudConnectDialog::updateWidgets);
    connect(m_appPassword, &QLineEdit::textChanged, this, &NextcloudConnectDialog::updateWidgets);
    connect(m_method, &QButtonGroup::buttonToggled, this, [this, browserNote] {
        browserNote->setVisible(m_browser->isChecked());
        m_passwordFields->setVisible(m_password->isChecked());
        updateWidgets();
    });
    connect(m_connect, &QPushButton::clicked, this, &NextcloudConnectDialog::connectAccount);
    connect(m_reopen, &QPushButton::clicked, this, [this] {
        if (m_flow.login.isValid())
            m_opener(m_flow.login);
    });
    connect(m_pollTimer, &QTimer::timeout, this, &NextcloudConnectDialog::poll);

    m_passwordFields->hide();
    if (!m_hub || !m_hub->canStoreCredentials()) {
        setStatus(i18nc("@info",
            "Nextcloud accounts can be connected only when "
            "encrypted password storage, such as "
            "KWallet, is available, because their app "
            "passwords are never kept in plain text."));
    }
    updateWidgets();
    resize(qMax(width(), 480), sizeHint().height());
}

NextcloudConnectDialog::~NextcloudConnectDialog()
{
    stop();
}

void NextcloudConnectDialog::setServer(const QString &server)
{
    m_server->setText(server);
}

void NextcloudConnectDialog::setUseAppPassword(bool useAppPassword)
{
    (useAppPassword ? m_password : m_browser)->setChecked(true);
}

void NextcloudConnectDialog::setLoginName(const QString &loginName)
{
    m_loginName->setText(loginName);
}

void NextcloudConnectDialog::setAppPassword(const QString &appPassword)
{
    m_appPassword->setText(appPassword);
}

void NextcloudConnectDialog::setUrlOpener(UrlOpener opener)
{
    m_opener = std::move(opener);
}

void NextcloudConnectDialog::setPollInterval(int milliseconds)
{
    m_pollTimer->setInterval(milliseconds);
}

void NextcloudConnectDialog::setPollLimit(int milliseconds)
{
    m_pollLimit = milliseconds;
}

QString NextcloudConnectDialog::statusText() const
{
    return m_status->text();
}

bool NextcloudConnectDialog::isBusy() const
{
    return m_busy;
}

QString NextcloudConnectDialog::accountId() const
{
    return m_accountId;
}

void NextcloudConnectDialog::updateWidgets()
{
    const bool canStore = m_hub && m_hub->canStoreCredentials();
    const bool filled = !m_server->text().trimmed().isEmpty()
        && (m_browser->isChecked() || (!m_loginName->text().trimmed().isEmpty() && !m_appPassword->text().isEmpty()));
    m_server->setEnabled(!m_busy);
    m_browser->setEnabled(!m_busy);
    m_password->setEnabled(!m_busy);
    m_passwordFields->setEnabled(!m_busy);
    m_connect->setEnabled(!m_busy && canStore && filled);
    if (!canStore)
        m_connect->setToolTip(i18nc("@info:tooltip", "Encrypted password storage is not available"));
    else
        m_connect->setToolTip(QString());
}

void NextcloudConnectDialog::setStatus(const QString &text)
{
    m_status->setText(text);
}

void NextcloudConnectDialog::setBusy(bool busy)
{
    m_busy = busy;
    updateWidgets();
}

void NextcloudConnectDialog::stop()
{
    m_pollTimer->stop();
    m_flow = {};
    if (m_reopen)
        m_reopen->hide();
    if (m_client) {
        m_client->abortAll();
        m_client->deleteLater();
        m_client = nullptr;
    }
}

void NextcloudConnectDialog::reject()
{
    stop();
    setBusy(false);
    QDialog::reject();
}

void NextcloudConnectDialog::connectAccount()
{
    if (m_busy || !m_hub)
        return;
    if (!m_hub->canStoreCredentials()) {
        updateWidgets();
        return;
    }
    QString error;
    const QUrl server = Nextcloud::normalizedServer(m_server->text(), &error);
    if (!server.isValid()) {
        setStatus(error);
        return;
    }
    if (!Nextcloud::isAcceptable(server)) {
        setStatus(i18nc("@info",
            "Use an address starting with https://. Plasma "
            "Impresario does not send "
            "passwords to a server without TLS."));
        return;
    }
    stop();
    m_client = new Client(this);
    m_hub->prepare(m_client);
    Nextcloud::Account account;
    account.server = server;
    m_client->setAccount(account);
    setBusy(true);
    if (m_password->isChecked())
        identify(server, m_loginName->text().trimmed(), m_appPassword->text());
    else
        startLogin();
}

void NextcloudConnectDialog::startLogin()
{
    if (!m_client)
        return;
    setStatus(i18nc("@info", "Asking the server to start the sign-in…"));
    m_client->startLogin(
        [this](const Client::LoginFlow &flow) {
            m_flow = flow;
            m_deadline = QDeadlineTimer(m_pollLimit);
            m_reopen->show();
            setStatus(i18nc("@info",
                "Grant access on the sign-in page which "
                "opened in your browser. This window "
                "continues by itself once you have."));
            m_opener(flow.login);
            schedulePoll();
        },
        [this](Client::Failure failure, const QString &message) { fail(failure, message, [this] { startLogin(); }); });
}

void NextcloudConnectDialog::schedulePoll()
{
    if (m_deadline.hasExpired()) {
        fail(Client::Failure::Expired,
            i18nc("@info",
                "The sign-in was not finished in time. Connect again "
                "to start over."),
            {});
        return;
    }
    m_pollTimer->start();
}

void NextcloudConnectDialog::poll()
{
    if (!m_client || !m_flow.endpoint.isValid())
        return;
    m_client->pollLogin(
        m_flow,
        [this](bool finished, const Client::Credentials &credentials) {
            if (!finished) {
                schedulePoll();
                return;
            }
            QUrl server = m_client->account().server;
            if (credentials.server.isValid() && !credentials.server.isEmpty()) {
                QString error;
                server = Nextcloud::normalizedServer(credentials.server.toString(), &error);
                if (!server.isValid() || !Nextcloud::isAcceptable(server)) {
                    fail(Client::Failure::Security,
                        i18nc("@info",
                            "The Nextcloud server named an address without "
                            "TLS for the account, so it "
                            "was not connected."),
                        {});
                    return;
                }
            }
            m_pollTimer->stop();
            m_reopen->hide();
            identify(server, credentials.loginName, credentials.appPassword);
        },
        [this](Client::Failure failure, const QString &message) {
            // Asking again is harmless; a moment without network should not end the
            // sign-in
            if (failure == Client::Failure::Network) {
                setStatus(i18nc("@info", "%1 Trying again…", message));
                schedulePoll();
                return;
            }
            fail(failure, message, [this] { schedulePoll(); });
        });
}

void NextcloudConnectDialog::identify(const QUrl &server, const QString &loginName, const QString &appPassword)
{
    if (!m_client)
        return;
    Nextcloud::Account account;
    account.server = server;
    account.loginName = loginName;
    m_client->setAccount(account);
    m_client->setAppPassword(appPassword);
    setStatus(i18nc("@info", "Checking the account…"));
    m_client->fetchUser(
        [this, account, appPassword](const Client::User &user) {
            if (user.id.isEmpty()) {
                fail(Client::Failure::Protocol, Client::failureName(Client::Failure::Protocol), {});
                return;
            }
            if (!m_hub) {
                fail(Client::Failure::Protocol, QString(), {});
                return;
            }
            Nextcloud::Account added = account;
            added.userId = user.id;
            added.displayName = user.displayName;
            added.id = m_hub->findAccount(added.server, user.id);
            if (added.id.isEmpty())
                added.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            setStatus(i18nc("@info", "Keeping the app password in encrypted password storage…"));
            const QString id = added.id;
            m_hub->addAccount(added, appPassword, this, [this, id](const QString &error) {
                stop();
                setBusy(false);
                if (!error.isEmpty()) {
                    setStatus(error);
                    return;
                }
                m_accountId = id;
                setStatus(i18nc("@info", "Connected."));
                emit connected(id);
                accept();
            });
        },
        [this, server, loginName, appPassword](Client::Failure failure, const QString &message) {
            QString text = message;
            if (failure == Client::Failure::Authentication) {
                text = i18nc("@info",
                    "The server refused the login name or app "
                    "password. Check them, or sign in in "
                    "the browser instead.");
            }
            fail(failure, text, [this, server, loginName, appPassword] { identify(server, loginName, appPassword); });
        });
}

void NextcloudConnectDialog::fail(Client::Failure failure, const QString &message, const std::function<void()> &retry)
{
    if (failure == Client::Failure::Certificate && retry && m_hub && m_client && m_hub->askTrust(m_client, this)) {
        retry();
        return;
    }
    stop();
    setBusy(false);
    setStatus(message.isEmpty() ? Client::failureName(failure) : message);
}

} // namespace Gui
