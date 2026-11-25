/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SettingsOAuthControls.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include "Common/SettingsNames.h"

namespace Gui
{

SettingsOAuthControls::SettingsOAuthControls(QFormLayout *layout, int row, const QSettings &settings,
                                             const QString &accountId, OAuth::TokenStore::PluginGetter passwordPlugin,
                                             QWidget *page)
    : QObject(page)
    , m_layout(layout)
    , m_accountId(accountId)
    , m_passwordPlugin(std::move(passwordPlugin))
{
    using Common::SettingsNames;
    m_hadSettings = settings.contains(SettingsNames::oauthProviderKey)
        || settings.contains(SettingsNames::oauthClientIdKey) || settings.contains(SettingsNames::oauthClientSecretKey);

    provider = new QComboBox(page);
    provider->setObjectName(QStringLiteral("oauthProvider"));
    const auto &providers = OAuth::providers();
    for (const auto &item : providers)
        provider->addItem(item.displayName, item.id);
    const int index = provider->findData(settings.value(SettingsNames::oauthProviderKey).toString());
    provider->setCurrentIndex(index >= 0 ? index : 0);

    clientId = new QLineEdit(page);
    clientId->setObjectName(QStringLiteral("oauthClientId"));
    clientId->setText(settings.value(SettingsNames::oauthClientIdKey).toString());
    clientId->setToolTip(tr("The OAuth client ID of an application registered with the provider for desktop or "
                            "public clients."));

    clientSecret = new QLineEdit(page);
    clientSecret->setObjectName(QStringLiteral("oauthClientSecret"));
    clientSecret->setText(settings.value(SettingsNames::oauthClientSecretKey).toString());
    clientSecret->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    clientSecret->setPlaceholderText(tr("Only if the provider issued one for this client"));
    clientSecret->setToolTip(tr("Some providers issue a non-confidential secret to desktop clients. It is saved in "
                                "the account settings in clear text."));

    m_buttons = new QWidget(page);
    auto *buttonLayout = new QHBoxLayout(m_buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    signIn = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-password")), tr("Sign In…"), m_buttons);
    signIn->setObjectName(QStringLiteral("oauthSignIn"));
    signIn->setToolTip(tr("Authorize Plasma Impresario in your web browser"));
    cancelSignIn = new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), tr("Cancel"), m_buttons);
    cancelSignIn->setObjectName(QStringLiteral("oauthCancel"));
    signOut = new QPushButton(QIcon::fromTheme(QStringLiteral("system-log-out")), tr("Sign Out"), m_buttons);
    signOut->setObjectName(QStringLiteral("oauthSignOut"));
    signOut->setToolTip(tr("Remove the stored sign-in of this account"));
    buttonLayout->addWidget(signIn);
    buttonLayout->addWidget(cancelSignIn);
    buttonLayout->addWidget(signOut);
    buttonLayout->addStretch();

    status = new QLabel(page);
    status->setObjectName(QStringLiteral("oauthStatus"));
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->insertRow(row++, tr("OAuth provider:"), provider);
    layout->insertRow(row++, tr("OAuth client ID:"), clientId);
    layout->insertRow(row++, tr("OAuth client secret:"), clientSecret);
    layout->insertRow(row++, tr("Sign-in:"), m_buttons);
    layout->insertRow(row, status);

    auto *refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(400);
    connect(refreshTimer, &QTimer::timeout, this, &SettingsOAuthControls::refreshState);
    auto edited = [this, refreshTimer]() {
        configurationEdited();
        if (m_active)
            refreshTimer->start();
    };
    connect(provider, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, edited);
    connect(clientId, &QLineEdit::textChanged, this, edited);
    connect(clientSecret, &QLineEdit::textChanged, this, edited);

    connect(signIn, &QAbstractButton::clicked, this, [this]() {
        OAuth::Session *current = session();
        setState(State::Authorizing, tr("Waiting for you to finish signing in in your web browser…"));
        current->authorize();
    });
    connect(cancelSignIn, &QAbstractButton::clicked, this, [this]() {
        if (m_session)
            m_session->cancel();
    });
    connect(signOut, &QAbstractButton::clicked, this, [this]() {
        setState(State::Checking, tr("Removing the stored sign-in…"));
        session()->signOut();
    });

    updateClientIdPlaceholder();
    setState(State::Unknown);
}

SettingsOAuthControls::~SettingsOAuthControls() = default;

void SettingsOAuthControls::setActive(bool active)
{
    m_active = active;
    for (QWidget *widget : {static_cast<QWidget *>(provider), static_cast<QWidget *>(clientId),
                            static_cast<QWidget *>(clientSecret), m_buttons, static_cast<QWidget *>(status)}) {
        m_layout->setRowVisible(widget, active);
    }
    if (!active) {
        if (m_session)
            m_session->cancel();
    } else if (m_state == State::Unknown) {
        refreshState();
    }
}

void SettingsOAuthControls::setLoginHint(const QString &loginHint)
{
    if (m_loginHint == loginHint)
        return;
    m_loginHint = loginHint;
    if (m_session && !m_session->isAuthorizing())
        delete m_session.data();
}

OAuth::AccountConfig SettingsOAuthControls::accountConfig() const
{
    OAuth::AccountConfig config;
    config.accountId = m_accountId;
    config.provider = OAuth::provider(provider->currentData().toString());
    config.clientId = clientId->text().trimmed();
    if (config.clientId.isEmpty())
        config.clientId = config.provider.defaultClientId;
    config.clientSecret = clientSecret->text().trimmed();
    config.loginHint = m_loginHint;
    return config;
}

void SettingsOAuthControls::save(QSettings &settings) const
{
    using Common::SettingsNames;
    // Accounts which never used OAuth keep their settings untouched
    if (!m_active && !m_hadSettings)
        return;
    settings.setValue(SettingsNames::oauthProviderKey, provider->currentData().toString());
    settings.setValue(SettingsNames::oauthClientIdKey, clientId->text().trimmed());
    if (clientSecret->text().trimmed().isEmpty())
        settings.remove(SettingsNames::oauthClientSecretKey);
    else
        settings.setValue(SettingsNames::oauthClientSecretKey, clientSecret->text().trimmed());
}

SettingsOAuthControls::State SettingsOAuthControls::state() const
{
    return m_state;
}

OAuth::Session *SettingsOAuthControls::session()
{
    if (m_session)
        return m_session;
    auto *current = new OAuth::Session(accountConfig(), m_passwordPlugin, this);
    m_session = current;
    connect(current, &OAuth::Session::authorized, this, [this]() {
        setState(State::SignedIn, tr("Signed in. This account authenticates with OAuth and no password is stored."));
    });
    connect(current, &OAuth::Session::authorizationFailed, this, [this](const QString &message) {
        setState(State::Failed, message);
    });
    connect(current, &OAuth::Session::signedOut, this, [this]() {
        setState(State::SignedOut, tr("Signed out. The stored sign-in of this account was removed."));
    });
    connect(current, &OAuth::Session::signOutFailed, this, [this](const QString &message) {
        setState(State::Failed, message);
    });
    connect(current, &OAuth::Session::signInStateKnown, this, [this, current](bool signedIn) {
        if (current->lastError() == OAuth::Error::StorageUnavailable) {
            setState(State::Failed, OAuth::errorMessage(OAuth::Error::StorageUnavailable));
        } else if (signedIn) {
            setState(State::SignedIn, tr("Signed in. This account authenticates with OAuth and no password is "
                                         "stored."));
        } else {
            setState(State::SignedOut, tr("Not signed in. Choose Sign In to authorize Plasma Impresario in your "
                                          "web browser."));
        }
    });
    return current;
}

void SettingsOAuthControls::configurationEdited()
{
    // A running sign-in belongs to the previous values; deleting the session stops it
    delete m_session.data();
    updateClientIdPlaceholder();
    if (m_state == State::Authorizing)
        setState(State::Unknown);
}

void SettingsOAuthControls::refreshState()
{
    if (m_state == State::Authorizing)
        return;
    if (!accountConfig().isValid()) {
        setState(State::NotConfigured, OAuth::errorMessage(OAuth::Error::NotConfigured));
        return;
    }
    setState(State::Checking, tr("Checking the stored sign-in…"));
    session()->checkSignedIn();
}

void SettingsOAuthControls::setState(State state, const QString &message)
{
    m_state = state;
    status->setText(message);
    status->setVisible(!message.isEmpty() && m_active);
    signIn->setEnabled(state != State::NotConfigured && state != State::Authorizing && state != State::Checking);
    signIn->setText(state == State::SignedIn ? tr("Sign In Again…") : tr("Sign In…"));
    cancelSignIn->setVisible(state == State::Authorizing);
    signOut->setEnabled(state == State::SignedIn || state == State::Failed);
    emit stateChanged();
}

void SettingsOAuthControls::updateClientIdPlaceholder()
{
    const QString builtIn = OAuth::provider(provider->currentData().toString()).defaultClientId;
    clientId->setPlaceholderText(builtIn.isEmpty() ? tr("Required") : tr("Default: %1").arg(builtIn));
}

}
