/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>
#include <memory>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QSettings;

namespace UiUtils {

/** @short Finding the IMAP and SMTP servers of an email address

The sources are tried in this order, and the first one which names an IMAP server wins:

1. providers built into the application;
2. the provider's own autoconfig file, over HTTPS from the mail domain;
3. Mozilla's ISP database (ISPDB);
4. DNS SRV records (RFC 6186, RFC 8314);
5. the domain's MX hosts, as a known provider or through the ISPDB entry of their domain;
6. common host names, probed without signing in.

Servers which came from DNS replies that were not signed (DNSSEC), and which belong neither to the mail domain nor to a
known provider, are listed in Result::unconfirmedHosts. They must not receive a password before the user confirms them.

Nothing here signs in or sends a password. All network access goes through Network, which tests replace.
*/
namespace ServerDiscovery {

enum class Security {
    None,
    StartTls,
    Tls,
};

/** @short Where settings were found, in the order in which the sources are tried */
enum class Source {
    None,
    BuiltIn,
    Autoconfig,
    Ispdb,
    Srv,
    Mx,
    Guess,
};

/** @short What a provider asks of the user before a mail application can sign in */
enum class Guidance {
    None,
    /** Mail applications need an app password instead of the account password */
    AppPassword,
    /** IMAP access has to be turned on in the provider's web settings first */
    EnableImap,
    /** The provider does not accept passwords from mail applications; OAuth 2.0 is needed */
    OAuthOnly,
};

struct Server {
    QString host;
    quint16 port = 0;
    Security security = Security::Tls;
    /** The user name; in Provider, a pattern with %EMAILADDRESS%, %EMAILLOCALPART% or %EMAILDOMAIN% */
    QString username;

    bool isValid() const { return !host.isEmpty() && port != 0; }
};

/** @short A mail provider known to the application */
struct Provider {
    QString id;
    QString displayName;
    /** Mail domains of the provider's own addresses */
    QStringList domains;
    /** Domains whose MX hosts mean that the provider hosts the mail of another domain */
    QStringList mxDomains;
    /** Domains of the provider's IMAP and SMTP servers */
    QStringList serverDomains;
    Server imap;
    Server smtp;
    /** ID of the OAuth provider (OAuth::provider) the servers accept; empty when there is none */
    QString oauthProvider;
    bool acceptsPasswords = true;
    Guidance guidance = Guidance::None;
    QUrl helpUrl;
};

/** @short All built-in providers */
const QList<Provider> &providers();

/** @short The built-in provider of a mail domain, or nullptr */
const Provider *providerForDomain(const QString &domain);

/** @short The built-in provider which runs the MX host @arg host, or nullptr */
const Provider *providerForMx(const QString &host);

/** @short The built-in provider which runs the IMAP or SMTP server @arg host, or nullptr */
const Provider *providerForServer(const QString &host);

struct Address {
    QString address;
    QString localPart;
    /** Lower case, as typed */
    QString domain;
    /** The domain in ASCII (IDNA), for DNS and URLs */
    QString asciiDomain;

    bool isValid() const { return !localPart.isEmpty() && !asciiDomain.isEmpty(); }
};

/** @short Split an email address; the result is invalid unless it has a local part and a usable domain */
Address parseAddress(const QString &address);

/** @short Replace %EMAILADDRESS%, %EMAILLOCALPART% and %EMAILDOMAIN% in @arg pattern */
QString expand(const QString &pattern, const Address &address);

/** @short Whether @arg host is @arg domain or one of its subdomains */
bool isWithin(const QString &host, const QString &domain);

struct Result {
    Source source = Source::None;
    /** The provider's name, when it is known */
    QString providerName;
    Server imap;
    /** Invalid when no SMTP server was found */
    Server smtp;
    /** ID of the OAuth provider the servers accept; empty when there is none */
    QString oauthProvider;
    bool acceptsPasswords = true;
    Guidance guidance = Guidance::None;
    QUrl helpUrl;
    /** Servers found through unsigned DNS outside the mail domain and the known providers */
    QStringList unconfirmedHosts;

    bool isValid() const { return imap.isValid(); }
    bool needsConfirmation() const { return !unconfirmedHosts.isEmpty(); }
};

/** @short Read a Mozilla autoconfig document (clientConfig 1.1), as served by mail domains and the ISPDB

The first IMAP and SMTP servers which use TLS or STARTTLS are taken; servers without encryption are ignored. The first
HTTPS documentation link becomes the help URL. The result is invalid when the document names no such IMAP server.
*/
Result parseAutoconfig(const QByteArray &xml, const Address &address);

/** @short Store @arg result as the IMAP and SMTP settings of an account

With @arg useOAuth and an OAuth provider in the result, both servers authenticate through it; otherwise with a
password. Certificates accepted for earlier servers are forgotten.
*/
void applyTo(QSettings &settings, const Result &result, bool useOAuth);

/** @short The network requests of a discovery, replaceable in tests

Callbacks are called at most once and never from within the call which started the request. After abort(), no
callback of an earlier request is called.
*/
class Network
{
public:
    struct SrvRecord {
        QString target;
        quint16 port = 0;
        quint16 priority = 0;
        quint16 weight = 0;
    };
    /** @arg ok is true for a successful HTTP response */
    using FetchDone = std::function<void(bool ok, const QByteArray &body)>;
    /** @arg authenticated is true when the resolver validated the reply with DNSSEC */
    using SrvDone = std::function<void(const QList<SrvRecord> &records, bool authenticated)>;
    /** @arg hosts are sorted by preference, most preferred first */
    using MxDone = std::function<void(const QStringList &hosts, bool authenticated)>;
    using ProbeDone = std::function<void(bool reachable)>;

    virtual ~Network();
    virtual void fetch(const QUrl &url, FetchDone done) = 0;
    virtual void lookupSrv(const QString &name, SrvDone done) = 0;
    virtual void lookupMx(const QString &domain, MxDone done) = 0;
    /** @short Check that a server answers at @arg host and @arg port, without signing in */
    virtual void probe(const QString &host, quint16 port, Security security, ProbeDone done) = 0;
    virtual void abort() = 0;
};

/** @short The network implementation used by the application */
std::unique_ptr<Network> createNetwork();

/** @short Find the servers of an email address, trying each source in order */
class Discovery : public QObject
{
    Q_OBJECT
public:
    explicit Discovery(std::unique_ptr<Network> network, QObject *parent = nullptr);
    ~Discovery() override;

    /** @short Start looking up @arg address, cancelling a running discovery */
    void start(const QString &address);
    /** @short Stop without emitting finished() */
    void cancel();
    bool isRunning() const;

signals:
    /** @short A new source is being tried */
    void stageChanged(UiUtils::ServerDiscovery::Source source);
    /** @short Emitted once per start(); the result is invalid when nothing was found */
    void finished(const UiUtils::ServerDiscovery::Result &result);

private:
    void enter(Source source);
    void tryAutoconfig(int step);
    void tryIspdb(const QString &domain, bool fromMx);
    void trySrv(int step);
    void tryMx();
    void tryGuess();
    void guessProbed();
    void finish(Result result);
    /** Fill in what the built-in provider which runs the IMAP server tells about it */
    void describeProvider(Result &result) const;
    /** List the servers outside the mail domain and the known providers, unless DNSSEC vouched for them */
    void markUnconfirmed(Result &result, bool authenticated) const;
    std::unique_ptr<Network> m_network;
    Address m_address;
    quint64 m_generation = 0;
    bool m_running = false;
    Result m_srv;
    bool m_srvAuthenticated = true;
    QString m_mxHost;
    bool m_mxAuthenticated = false;
    struct Probe {
        bool imap = true;
        QString host;
        quint16 port = 0;
        Security security = Security::Tls;
        int state = -1; // -1 pending, 0 unreachable, 1 reachable
    };
    QList<Probe> m_probes;
};

}
}

Q_DECLARE_METATYPE(UiUtils::ServerDiscovery::Result)
Q_DECLARE_METATYPE(UiUtils::ServerDiscovery::Source)
