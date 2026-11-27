/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AddAccountDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "OAuth/OAuth.h"

namespace Gui {

using namespace UiUtils::ServerDiscovery;

namespace {

QString securityName(Security security)
{
    switch (security) {
    case Security::Tls:
        return i18nc("@item connection security", "SSL/TLS");
    case Security::StartTls:
        return i18nc("@item connection security", "STARTTLS");
    case Security::None:
        break;
    }
    return i18nc("@item connection security", "None");
}

QString describeServer(const Server &server)
{
    if (!server.isValid())
        return i18nc("@info server settings", "Not found; enter it in the account settings");
    return i18nc("@info %1 is a host name, %2 a port number, %3 the connection security", "%1, port %2, %3",
                 server.host, QString::number(server.port), securityName(server.security));
}

QString discoveryIntroduction()
{
    return i18nc("@info", "Find Settings looks up the servers of the address. Nothing is signed in to, and no password is "
                          "sent. You can also add the account and enter the servers yourself.");
}

QLabel *wrappingLabel(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

}

AddAccountDialog::AddAccountDialog(QWidget *parent, std::unique_ptr<Network> network)
    : QDialog(parent)
    , m_discovery(new Discovery(network ? std::move(network) : createNetwork(), this))
    , m_realName(new QLineEdit(this))
    , m_emailAddress(new QLineEdit(this))
    , m_accountName(new QLineEdit(this))
    , m_protocol(new QComboBox(this))
    , m_pop3Note(wrappingLabel(this))
    , m_find(new QPushButton(i18nc("@action:button", "Find Settings"), this))
    , m_stop(new QPushButton(i18nc("@action:button", "Stop"), this))
    , m_progress(new QProgressBar(this))
    , m_status(wrappingLabel(this))
    , m_servers(new QGroupBox(i18nc("@title:group", "Servers"), this))
    , m_provider(wrappingLabel(m_servers))
    , m_imap(wrappingLabel(m_servers))
    , m_smtp(wrappingLabel(m_servers))
    , m_source(wrappingLabel(m_servers))
    , m_guidance(new QLabel(m_servers))
    , m_oauth(new QCheckBox(m_servers))
    , m_oauthNote(wrappingLabel(m_servers))
    , m_warning(wrappingLabel(m_servers))
    , m_confirm(new QCheckBox(m_servers))
    , m_buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    setWindowTitle(i18nc("@title:window", "Add Account"));

    m_realName->setObjectName(QStringLiteral("realName"));
    m_emailAddress->setObjectName(QStringLiteral("emailAddress"));
    m_accountName->setObjectName(QStringLiteral("accountName"));
    m_protocol->setObjectName(QStringLiteral("accountProtocol"));
    m_pop3Note->setObjectName(QStringLiteral("pop3Note"));
    m_find->setObjectName(QStringLiteral("findSettings"));
    m_stop->setObjectName(QStringLiteral("stopFinding"));
    m_status->setObjectName(QStringLiteral("discoveryStatus"));
    m_servers->setObjectName(QStringLiteral("servers"));
    m_provider->setObjectName(QStringLiteral("provider"));
    m_imap->setObjectName(QStringLiteral("imapServer"));
    m_smtp->setObjectName(QStringLiteral("smtpServer"));
    m_source->setObjectName(QStringLiteral("settingsSource"));
    m_guidance->setObjectName(QStringLiteral("guidance"));
    m_oauth->setObjectName(QStringLiteral("useOAuth"));
    m_oauthNote->setObjectName(QStringLiteral("oauthNote"));
    m_warning->setObjectName(QStringLiteral("unconfirmedWarning"));
    m_confirm->setObjectName(QStringLiteral("confirmServers"));

    m_realName->setPlaceholderText(i18nc("@info:placeholder", "Shown to the people you write to"));
    m_emailAddress->setPlaceholderText(i18nc("@info:placeholder", "name@example.com"));
    m_accountName->setPlaceholderText(i18nc("@info:placeholder", "Same as the email address"));
    m_find->setAutoDefault(false);
    m_stop->setAutoDefault(false);
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(m_progress->fontMetrics().height() / 2 + 2);
    m_guidance->setWordWrap(true);
    m_guidance->setTextFormat(Qt::RichText);
    m_guidance->setOpenExternalLinks(true);
    m_guidance->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_buttons->button(QDialogButtonBox::Ok)->setText(i18nc("@action:button", "Add Account"));
    m_protocol->addItem(i18nc("@item:inlistbox account type", "IMAP"), static_cast<int>(Imap::AccountProtocol::Imap));
    m_protocol->addItem(i18nc("@item:inlistbox account type", "POP3"), static_cast<int>(Imap::AccountProtocol::Pop3));
    m_protocol->addItem(i18nc("@item:inlistbox account type", "JMAP"), static_cast<int>(Imap::AccountProtocol::Jmap));
    m_protocol->setItemData(0, i18nc("@info:tooltip", "Mail stays on the server, in its folders"), Qt::ToolTipRole);
    m_protocol->setItemData(1, i18nc("@info:tooltip", "Mail is downloaded from the server's inbox to this computer"),
                            Qt::ToolTipRole);
    m_protocol->setItemData(2, i18nc("@info:tooltip", "Mail stays on the server and is synchronized with a copy on this "
                                                      "computer, over the web"),
                            Qt::ToolTipRole);

    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "Your name:"), m_realName);
    form->addRow(i18nc("@label:textbox", "Email address:"), m_emailAddress);
    form->addRow(i18nc("@label:textbox", "Account name:"), m_accountName);
    form->addRow(i18nc("@label:listbox", "Account type:"), m_protocol);

    auto *findRow = new QHBoxLayout;
    findRow->addWidget(m_find);
    findRow->addWidget(m_stop);
    findRow->addStretch();

    auto *serverForm = new QFormLayout;
    serverForm->addRow(i18nc("@label", "Provider:"), m_provider);
    serverForm->addRow(i18nc("@label", "Incoming mail (IMAP):"), m_imap);
    serverForm->addRow(i18nc("@label", "Outgoing mail (SMTP):"), m_smtp);
    serverForm->addRow(i18nc("@label", "Found in:"), m_source);
    auto *serverLayout = new QVBoxLayout(m_servers);
    serverLayout->addLayout(serverForm);
    serverLayout->addWidget(m_guidance);
    serverLayout->addWidget(m_oauth);
    serverLayout->addWidget(m_oauthNote);
    serverLayout->addWidget(m_warning);
    serverLayout->addWidget(m_confirm);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(findRow);
    layout->addWidget(m_pop3Note);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    layout->addWidget(m_servers);
    layout->addStretch();
    layout->addWidget(m_buttons);

    m_status->setText(discoveryIntroduction());

    connect(m_emailAddress, &QLineEdit::textChanged, this, &AddAccountDialog::addressEdited);
    connect(m_accountName, &QLineEdit::textChanged, this, &AddAccountDialog::updateButtons);
    connect(m_protocol, &QComboBox::currentIndexChanged, this, &AddAccountDialog::protocolChanged);
    connect(m_find, &QPushButton::clicked, this, &AddAccountDialog::findSettings);
    connect(m_stop, &QPushButton::clicked, this, [this]() {
        m_discovery->cancel();
        setBusy(false);
        m_status->setText(i18nc("@info", "Stopped. You can add the account and enter the servers in its settings."));
    });
    connect(m_confirm, &QCheckBox::toggled, this, &AddAccountDialog::updateButtons);
    connect(m_discovery, &Discovery::stageChanged, this, &AddAccountDialog::stageChanged);
    connect(m_discovery, &Discovery::finished, this, &AddAccountDialog::discoveryFinished);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_servers->hide();
    m_pop3Note->hide();
    setBusy(false);
    setMinimumWidth(fontMetrics().averageCharWidth() * 70);
    m_emailAddress->setFocus();
}

AddAccountDialog::~AddAccountDialog() = default;

QString AddAccountDialog::accountName() const
{
    const QString typed = m_accountName->text().trimmed();
    return typed.isEmpty() ? emailAddress() : typed;
}

QString AddAccountDialog::emailAddress() const
{
    return m_emailAddress->text().trimmed();
}

QString AddAccountDialog::realName() const
{
    return m_realName->text().trimmed();
}

Result AddAccountDialog::result() const
{
    return m_result;
}

bool AddAccountDialog::useOAuth() const
{
    return protocol() == Imap::AccountProtocol::Imap && m_result.isValid() && !m_result.oauthProvider.isEmpty()
        && m_oauth->isChecked();
}

Imap::AccountProtocol AddAccountDialog::protocol() const
{
    return static_cast<Imap::AccountProtocol>(m_protocol->currentData().toInt());
}

void AddAccountDialog::protocolChanged()
{
    const bool imap = protocol() == Imap::AccountProtocol::Imap;
    // Discovery finds IMAP servers, whose settings would not fit a POP3 account
    m_discovery->cancel();
    m_result = Result();
    m_servers->hide();
    m_pop3Note->setVisible(!imap);
    if (protocol() == Imap::AccountProtocol::Jmap)
        m_pop3Note->setText(i18nc("@info", "JMAP keeps the mail on the server and a copy on this computer, which is "
                                           "synchronized every few minutes and soon after each change. Enter the JMAP "
                                           "server's address, or only its host name, in the account settings, which "
                                           "open next. Messages are sent through the JMAP server unless you choose "
                                           "an SMTP server there."));
    else if (protocol() == Imap::AccountProtocol::Pop3)
        m_pop3Note->setText(i18nc("@info", "POP3 downloads the server's inbox to this computer, and the mail is then "
                                           "read only here. Read and flagged states, folders and searches stay on this "
                                           "computer. Enter the POP3 and SMTP servers in the account settings, which "
                                           "open next."));
    m_status->setVisible(imap);
    m_status->setText(imap ? discoveryIntroduction() : QString());
    setBusy(false);
}

QString AddAccountDialog::domainForDisplay() const
{
    return parseAddress(emailAddress()).domain;
}

QString AddAccountDialog::providerForDisplay() const
{
    return m_result.providerName.isEmpty() ? i18nc("@info used in place of a provider name", "the provider")
                                           : m_result.providerName;
}

void AddAccountDialog::addressEdited()
{
    if (m_discovery->isRunning() || m_result.isValid() || m_servers->isVisible()) {
        m_discovery->cancel();
        m_result = Result();
        m_servers->hide();
        m_status->clear();
        setBusy(false);
    }
    m_accountName->setPlaceholderText(emailAddress().isEmpty() ? i18nc("@info:placeholder", "Same as the email address")
                                                               : emailAddress());
    updateButtons();
}

void AddAccountDialog::findSettings()
{
    if (!parseAddress(emailAddress()).isValid())
        return;
    m_result = Result();
    m_servers->hide();
    m_discovery->start(emailAddress());
    setBusy(true);
}

void AddAccountDialog::stageChanged(Source source)
{
    const QString domain = domainForDisplay();
    switch (source) {
    case Source::BuiltIn:
        m_status->setText(i18nc("@info:status", "Checking the providers Plasma Impresario knows…"));
        break;
    case Source::Autoconfig:
        m_status->setText(i18nc("@info:status %1 is a mail domain", "Asking %1 for its mail settings…", domain));
        break;
    case Source::Ispdb:
        m_status->setText(i18nc("@info:status", "Searching Thunderbird's database of mail providers…"));
        break;
    case Source::Srv:
        m_status->setText(i18nc("@info:status %1 is a mail domain", "Looking up the mail servers of %1 in DNS…", domain));
        break;
    case Source::Mx:
        m_status->setText(i18nc("@info:status %1 is a mail domain", "Checking which provider receives mail for %1…", domain));
        break;
    case Source::Guess:
        m_status->setText(i18nc("@info:status", "Trying common server names…"));
        break;
    case Source::None:
        break;
    }
}

void AddAccountDialog::discoveryFinished(const Result &result)
{
    setBusy(false);
    m_result = result;
    if (!result.isValid()) {
        m_servers->hide();
        m_status->setText(i18nc("@info %1 is a mail domain",
                                "No mail settings were found for %1. You can add the account and enter the servers in "
                                "its settings.",
                                domainForDisplay()));
        updateButtons();
        return;
    }
    m_status->setText(i18nc("@info", "Check the servers below. You sign in from the account settings, which open next."));
    showResult();
}

void AddAccountDialog::showResult()
{
    const QString domain = domainForDisplay();
    m_provider->setText(m_result.providerName.isEmpty() ? i18nc("@info unknown mail provider", "Unknown") : m_result.providerName);
    m_imap->setText(describeServer(m_result.imap));
    m_smtp->setText(describeServer(m_result.smtp));
    switch (m_result.source) {
    case Source::BuiltIn:
        m_source->setText(i18nc("@info where settings came from", "Settings built into Plasma Impresario"));
        break;
    case Source::Autoconfig:
        m_source->setText(i18nc("@info where settings came from; %1 is a mail domain", "Settings published by %1", domain));
        break;
    case Source::Ispdb:
        m_source->setText(i18nc("@info where settings came from", "Thunderbird's database of mail providers"));
        break;
    case Source::Srv:
        m_source->setText(i18nc("@info where settings came from; %1 is a mail domain", "DNS service records of %1", domain));
        break;
    case Source::Mx:
        m_source->setText(i18nc("@info where settings came from; %1 is a mail domain",
                                "The provider that receives mail for %1 (its MX record)", domain));
        break;
    case Source::Guess:
        m_source->setText(i18nc("@info where settings came from",
                                "Common server names that answered; the provider did not publish them"));
        break;
    case Source::None:
        m_source->clear();
        break;
    }

    QString guidance;
    switch (m_result.guidance) {
    case Guidance::AppPassword:
        guidance = i18nc("@info %1 is a mail provider", "%1 may need an app password instead of your account password, for "
                                                         "example when two-step verification is on.",
                         providerForDisplay());
        break;
    case Guidance::EnableImap:
        guidance = i18nc("@info %1 is a mail provider", "Turn on IMAP access in the web settings of %1 before signing in.",
                         providerForDisplay());
        break;
    case Guidance::OAuthOnly:
        guidance = m_result.oauthProvider.isEmpty()
            ? i18nc("@info %1 is a mail provider", "%1 does not accept passwords from mail applications, and offers no "
                                                    "sign-in that Plasma Impresario supports.",
                    providerForDisplay())
            : i18nc("@info %1 is a mail provider", "%1 does not accept passwords from mail applications; sign in with "
                                                    "OAuth 2.0 instead.",
                    providerForDisplay());
        break;
    case Guidance::None:
        break;
    }
    if (m_result.helpUrl.isValid() && !m_result.helpUrl.isEmpty()) {
        const QString link = QStringLiteral("<a href=\"%1\">%2</a>")
                                 .arg(m_result.helpUrl.toString(QUrl::FullyEncoded).toHtmlEscaped(),
                                      i18nc("@info link to a provider's help page", "Provider help").toHtmlEscaped());
        guidance = guidance.isEmpty() ? link : guidance.toHtmlEscaped() + QLatin1Char(' ') + link;
    } else {
        guidance = guidance.toHtmlEscaped();
    }
    m_guidance->setText(guidance);
    m_guidance->setVisible(!guidance.isEmpty());

    const OAuth::Provider oauth = OAuth::provider(m_result.oauthProvider);
    const bool offersOAuth = !m_result.oauthProvider.isEmpty() && oauth.isValid();
    m_oauth->setVisible(offersOAuth);
    m_oauthNote->setVisible(false);
    if (offersOAuth) {
        m_oauth->setText(i18nc("@option:check %1 is a sign-in provider such as Google", "Use OAuth 2.0 sign-in with %1",
                               oauth.displayName));
        // Without passwords, OAuth is the only way in
        m_oauth->setEnabled(m_result.acceptsPasswords);
        m_oauth->setChecked(!m_result.acceptsPasswords || !oauth.defaultClientId.isEmpty());
        if (oauth.defaultClientId.isEmpty()) {
            m_oauthNote->setText(i18nc("@info %1 is a sign-in provider such as Google",
                                       "OAuth 2.0 with %1 needs a client ID, which this build does not include. Enter "
                                       "one in the account settings before signing in.",
                                       oauth.displayName));
            m_oauthNote->setVisible(true);
        }
    }

    const bool confirm = m_result.needsConfirmation();
    m_warning->setVisible(confirm);
    m_confirm->setVisible(confirm);
    m_confirm->setChecked(false);
    if (confirm) {
        m_warning->setText(i18nc("@info %1 is a mail domain",
                                 "These servers were found through DNS replies that could not be verified, and they do "
                                 "not belong to %1 or to a provider Plasma Impresario knows. Someone on your network "
                                 "could have sent them in order to receive your password.",
                                 domain));
        m_confirm->setText(i18nc("@option:check %1 is a list of host names", "I trust %1",
                                 m_result.unconfirmedHosts.join(QStringLiteral(", "))));
    }
    m_servers->show();
    updateButtons();
}

void AddAccountDialog::setBusy(bool busy)
{
    const bool imap = protocol() == Imap::AccountProtocol::Imap;
    m_progress->setVisible(busy);
    m_stop->setVisible(busy);
    m_find->setVisible(!busy && imap);
    updateButtons();
}

void AddAccountDialog::updateButtons()
{
    const Address address = parseAddress(emailAddress());
    const bool running = m_discovery->isRunning();
    m_find->setEnabled(address.isValid() && !running && protocol() == Imap::AccountProtocol::Imap);
    const bool addressUsable = emailAddress().isEmpty() || address.isValid();
    const bool confirmed = !m_result.needsConfirmation() || m_confirm->isChecked();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!running && addressUsable && !accountName().isEmpty() && confirmed);
}

}
