/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ServerDiscovery.h"

#include <algorithm>
#include <QSettings>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>

#include "Common/SettingsNames.h"

namespace UiUtils {
namespace ServerDiscovery {

namespace {

QStringList words(const char *list)
{
    return QString::fromLatin1(list).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

Server server(const char *host, quint16 port, Security security, const char *username)
{
    Server s;
    s.host = QString::fromLatin1(host);
    s.port = port;
    s.security = security;
    s.username = QString::fromLatin1(username);
    return s;
}

QString normalizedHost(const QString &host)
{
    QString out = host.trimmed().toLower();
    while (out.endsWith(QLatin1Char('.')))
        out.chop(1);
    return out;
}

/** The SRV record to use: lowest priority, then highest weight; none when the service is declared unavailable */
const Network::SrvRecord *bestSrv(const QList<Network::SrvRecord> &records)
{
    const Network::SrvRecord *best = nullptr;
    for (const auto &record : records) {
        // RFC 2782: a target of "." means that the service is not offered
        if (normalizedHost(record.target).isEmpty() || record.port == 0)
            continue;
        if (!best || record.priority < best->priority || (record.priority == best->priority && record.weight > best->weight))
            best = &record;
    }
    return best;
}

/** The domain under which a host is registered, approximated by its last two labels */
QString baseDomain(const QString &host)
{
    const QStringList labels = normalizedHost(host).split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (labels.size() < 2)
        return QString();
    return labels.mid(labels.size() - 2).join(QLatin1Char('.'));
}

Result fromProvider(const Provider &provider, const Address &address, Source source)
{
    Result result;
    result.source = source;
    result.providerName = provider.displayName;
    result.imap = provider.imap;
    result.imap.username = expand(provider.imap.username, address);
    result.smtp = provider.smtp;
    result.smtp.username = expand(provider.smtp.username, address);
    result.oauthProvider = provider.oauthProvider;
    result.acceptsPasswords = provider.acceptsPasswords;
    result.guidance = provider.guidance;
    result.helpUrl = provider.helpUrl;
    return result;
}

constexpr quint16 portImaps = 993;
constexpr quint16 portImap = 143;
constexpr quint16 portSubmissions = 465;
constexpr quint16 portSubmission = 587;

}

const QList<Provider> &providers()
{
    static const QList<Provider> list = [] {
        QList<Provider> out;
        // Server names, ports and user names follow the providers' entries in Mozilla's ISPDB

        Provider google;
        google.id = QStringLiteral("google");
        google.displayName = QStringLiteral("Gmail");
        google.domains = words("gmail.com googlemail.com");
        google.mxDomains = words("google.com googlemail.com");
        google.serverDomains = words("gmail.com googlemail.com google.com");
        google.imap = server("imap.gmail.com", portImaps, Security::Tls, "%EMAILADDRESS%");
        google.smtp = server("smtp.gmail.com", portSubmissions, Security::Tls, "%EMAILADDRESS%");
        google.oauthProvider = QStringLiteral("google");
        google.guidance = Guidance::AppPassword;
        google.helpUrl = QUrl(QStringLiteral("https://support.google.com/accounts/answer/185833"));
        google.sentCopyHosts = words("smtp.gmail.com smtp.googlemail.com");
        out << google;

        Provider outlook;
        outlook.id = QStringLiteral("outlook");
        outlook.displayName = QStringLiteral("Outlook.com");
        outlook.domains = words(
            "outlook.com hotmail.com live.com msn.com windowslive.com "
            "outlook.at outlook.be outlook.cl outlook.cz outlook.de outlook.dk outlook.es outlook.fr outlook.hu "
            "outlook.ie outlook.in outlook.it outlook.jp outlook.kr outlook.lv outlook.my outlook.ph outlook.pt "
            "outlook.sa outlook.sg outlook.sk outlook.co.id outlook.co.il outlook.co.th outlook.com.ar outlook.com.au "
            "outlook.com.br outlook.com.gr outlook.com.tr outlook.com.vn "
            "hotmail.be hotmail.ca hotmail.cl hotmail.cz hotmail.de hotmail.dk hotmail.es hotmail.fi hotmail.fr "
            "hotmail.gr hotmail.hu hotmail.it hotmail.lt hotmail.lv hotmail.my hotmail.nl hotmail.no hotmail.ph "
            "hotmail.rs hotmail.se hotmail.sg hotmail.sk hotmail.co.id hotmail.co.il hotmail.co.in hotmail.co.jp "
            "hotmail.co.kr hotmail.co.th hotmail.co.uk hotmail.co.za hotmail.com.ar hotmail.com.au hotmail.com.br "
            "hotmail.com.hk hotmail.com.tr hotmail.com.tw hotmail.com.vn "
            "live.at live.be live.ca live.cl live.cn live.de live.dk live.fi live.fr live.hk live.ie live.in live.it "
            "live.jp live.nl live.no live.ru live.se live.co.jp live.co.kr live.co.uk live.co.za live.com.ar "
            "live.com.au live.com.mx live.com.my live.com.ph live.com.pt live.com.sg livemail.tw");
        outlook.mxDomains = words("olc.protection.outlook.com");
        outlook.serverDomains = words("outlook.com office365.com");
        outlook.imap = server("outlook.office365.com", portImaps, Security::Tls, "%EMAILADDRESS%");
        outlook.smtp = server("smtp-mail.outlook.com", portSubmission, Security::StartTls, "%EMAILADDRESS%");
        outlook.oauthProvider = QStringLiteral("microsoft");
        outlook.acceptsPasswords = false;
        outlook.guidance = Guidance::OAuthOnly;
        outlook.helpUrl = QUrl(QStringLiteral(
            "https://support.microsoft.com/office/pop-imap-and-smtp-settings-for-outlook-com-d088b986-291d-42b8-9564-9c414e2aa040"));
        out << outlook;

        Provider microsoft365;
        microsoft365.id = QStringLiteral("microsoft365");
        microsoft365.displayName = QStringLiteral("Microsoft 365");
        microsoft365.mxDomains = words("mail.protection.outlook.com");
        microsoft365.serverDomains = words("outlook.com office365.com");
        microsoft365.imap = server("outlook.office365.com", portImaps, Security::Tls, "%EMAILADDRESS%");
        microsoft365.smtp = server("smtp.office365.com", portSubmission, Security::StartTls, "%EMAILADDRESS%");
        microsoft365.oauthProvider = QStringLiteral("microsoft");
        microsoft365.acceptsPasswords = false;
        microsoft365.guidance = Guidance::OAuthOnly;
        out << microsoft365;

        Provider yahoo;
        yahoo.id = QStringLiteral("yahoo");
        yahoo.displayName = QStringLiteral("Yahoo Mail");
        yahoo.domains = words("yahoo.com yahoo.ca yahoo.de yahoo.it yahoo.fr yahoo.es yahoo.se yahoo.co.in yahoo.co.uk "
                              "yahoo.co.nz yahoo.com.au yahoo.com.ar yahoo.com.br yahoo.com.mx ymail.com myyahoo.com "
                              "rocketmail.com");
        yahoo.serverDomains = words("yahoo.com");
        yahoo.imap = server("imap.mail.yahoo.com", portImaps, Security::Tls, "%EMAILADDRESS%");
        yahoo.smtp = server("smtp.mail.yahoo.com", portSubmissions, Security::Tls, "%EMAILADDRESS%");
        yahoo.guidance = Guidance::AppPassword;
        yahoo.helpUrl = QUrl(QStringLiteral("https://help.yahoo.com/kb/SLN15241.html"));
        out << yahoo;

        Provider icloud;
        icloud.id = QStringLiteral("icloud");
        icloud.displayName = QStringLiteral("iCloud Mail");
        icloud.domains = words("icloud.com me.com mac.com");
        icloud.serverDomains = words("me.com icloud.com");
        icloud.imap = server("imap.mail.me.com", portImaps, Security::Tls, "%EMAILLOCALPART%");
        icloud.smtp = server("smtp.mail.me.com", portSubmission, Security::StartTls, "%EMAILADDRESS%");
        icloud.guidance = Guidance::AppPassword;
        icloud.helpUrl = QUrl(QStringLiteral("https://support.apple.com/102654"));
        out << icloud;

        Provider aol;
        aol.id = QStringLiteral("aol");
        aol.displayName = QStringLiteral("AOL Mail");
        aol.domains = words("aol.com aim.com netscape.net netscape.com compuserve.com cs.com wmconnect.com aol.de aol.it "
                            "aol.fr aol.es aol.se aol.co.uk aol.co.nz aol.com.au aol.com.ar aol.com.br aol.com.mx");
        aol.serverDomains = words("aol.com");
        aol.imap = server("imap.aol.com", portImaps, Security::Tls, "%EMAILADDRESS%");
        aol.smtp = server("smtp.aol.com", portSubmissions, Security::Tls, "%EMAILADDRESS%");
        aol.guidance = Guidance::AppPassword;
        aol.helpUrl = QUrl(QStringLiteral("https://help.aol.com/articles/Create-and-manage-app-password"));
        out << aol;

        Provider gmx;
        gmx.id = QStringLiteral("gmx");
        gmx.displayName = QStringLiteral("GMX");
        gmx.domains = words("gmx.net gmx.de gmx.at gmx.ch gmx.eu gmx.biz gmx.org gmx.info");
        gmx.serverDomains = words("gmx.net");
        gmx.imap = server("imap.gmx.net", portImaps, Security::Tls, "%EMAILADDRESS%");
        gmx.smtp = server("mail.gmx.net", portSubmissions, Security::Tls, "%EMAILADDRESS%");
        gmx.guidance = Guidance::EnableImap;
        gmx.helpUrl = QUrl(QStringLiteral("https://hilfe.gmx.net/pop-imap/einschalten.html"));
        out << gmx;

        Provider webde;
        webde.id = QStringLiteral("webde");
        webde.displayName = QStringLiteral("WEB.DE");
        webde.domains = words("web.de");
        webde.serverDomains = words("web.de");
        webde.imap = server("imap.web.de", portImaps, Security::Tls, "%EMAILLOCALPART%");
        webde.smtp = server("smtp.web.de", portSubmissions, Security::Tls, "%EMAILLOCALPART%");
        webde.guidance = Guidance::EnableImap;
        webde.helpUrl = QUrl(QStringLiteral("https://hilfe.web.de/pop-imap/einschalten.html"));
        out << webde;

        Provider posteo;
        posteo.id = QStringLiteral("posteo");
        posteo.displayName = QStringLiteral("Posteo");
        posteo.domains = words(
            "posteo.de posteo.at posteo.be posteo.ca posteo.ch posteo.cl posteo.co posteo.co.uk posteo.com "
            "posteo.com.br posteo.cr posteo.cz posteo.dk posteo.ee posteo.es posteo.eu posteo.fi posteo.gl posteo.gr "
            "posteo.hn posteo.hr posteo.hu posteo.ie posteo.in posteo.is posteo.it posteo.jp posteo.la posteo.li "
            "posteo.lt posteo.lu posteo.me posteo.mx posteo.my posteo.net posteo.nl posteo.no posteo.nz posteo.org "
            "posteo.pe posteo.pl posteo.pm posteo.pt posteo.ro posteo.se posteo.sg posteo.si posteo.tn posteo.uk "
            "posteo.us");
        posteo.serverDomains = words("posteo.de");
        posteo.imap = server("posteo.de", portImaps, Security::Tls, "%EMAILADDRESS%");
        posteo.smtp = server("posteo.de", portSubmissions, Security::Tls, "%EMAILADDRESS%");
        out << posteo;

        return out;
    }();
    return list;
}

const Provider *providerForDomain(const QString &domain)
{
    const QString wanted = normalizedHost(domain);
    for (const Provider &provider : providers()) {
        if (provider.domains.contains(wanted))
            return &provider;
    }
    return nullptr;
}

const Provider *providerForMx(const QString &host)
{
    for (const Provider &provider : providers()) {
        for (const QString &domain : provider.mxDomains) {
            if (isWithin(host, domain))
                return &provider;
        }
    }
    return nullptr;
}

const Provider *providerForServer(const QString &host)
{
    for (const Provider &provider : providers()) {
        for (const QString &domain : provider.serverDomains) {
            if (isWithin(host, domain))
                return &provider;
        }
    }
    return nullptr;
}

bool keepsSentCopies(const QString &smtpHost)
{
    const QString wanted = normalizedHost(smtpHost);
    if (wanted.isEmpty())
        return false;
    for (const Provider &provider : providers()) {
        if (provider.sentCopyHosts.contains(wanted))
            return true;
    }
    return false;
}

Address parseAddress(const QString &address)
{
    Address out;
    const QString trimmed = address.trimmed();
    const qsizetype at = trimmed.lastIndexOf(QLatin1Char('@'));
    if (at <= 0)
        return out;
    const QString local = trimmed.left(at);
    QString domain = trimmed.mid(at + 1).toLower();
    while (domain.endsWith(QLatin1Char('.')))
        domain.chop(1);
    for (const QChar c : local) {
        if (c.isSpace() || c == QLatin1Char('<') || c == QLatin1Char('>'))
            return out;
    }
    if (!domain.contains(QLatin1Char('.')) || domain.startsWith(QLatin1Char('.')))
        return out;
    const QString ascii = QString::fromLatin1(QUrl::toAce(domain, QUrl::AceTransitionalProcessing)).toLower();
    if (ascii.isEmpty())
        return out;
    for (const QChar c : ascii) {
        if (!(c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('.')))
            return out;
    }
    if (ascii.contains(QLatin1String("..")))
        return out;
    out.address = local + QLatin1Char('@') + domain;
    out.localPart = local;
    out.domain = domain;
    out.asciiDomain = ascii;
    return out;
}

QString expand(const QString &pattern, const Address &address)
{
    QString out = pattern;
    out.replace(QLatin1String("%EMAILADDRESS%"), address.address);
    out.replace(QLatin1String("%EMAILLOCALPART%"), address.localPart);
    out.replace(QLatin1String("%EMAILDOMAIN%"), address.domain);
    return out;
}

bool isWithin(const QString &host, const QString &domain)
{
    const QString h = normalizedHost(host);
    const QString d = normalizedHost(domain);
    if (h.isEmpty() || d.isEmpty())
        return false;
    return h == d || h.endsWith(QLatin1Char('.') + d);
}

Result parseAutoconfig(const QByteArray &xml, const Address &address)
{
    Result result;
    QXmlStreamReader reader(xml);
    bool inProvider = false;
    if (!reader.readNextStartElement() || reader.name() != QLatin1String("clientConfig"))
        return result;
    struct Parsed {
        Server server;
        QStringList authentication;
        bool usable = false;
    };
    const auto readServer = [&reader, &address]() {
        Parsed parsed;
        QString socketType;
        while (reader.readNextStartElement()) {
            const auto name = reader.name();
            if (name == QLatin1String("hostname")) {
                parsed.server.host = normalizedHost(reader.readElementText());
            } else if (name == QLatin1String("port")) {
                bool ok = false;
                const uint port = reader.readElementText().trimmed().toUInt(&ok);
                parsed.server.port = ok && port <= 65535 ? quint16(port) : 0;
            } else if (name == QLatin1String("socketType")) {
                socketType = reader.readElementText().trimmed();
            } else if (name == QLatin1String("username")) {
                parsed.server.username = expand(reader.readElementText().trimmed(), address);
            } else if (name == QLatin1String("authentication")) {
                parsed.authentication << reader.readElementText().trimmed();
            } else {
                reader.skipCurrentElement();
            }
        }
        if (socketType == QLatin1String("SSL"))
            parsed.server.security = Security::Tls;
        else if (socketType == QLatin1String("STARTTLS"))
            parsed.server.security = Security::StartTls;
        else
            return parsed; // no encryption, or unknown
        parsed.usable = parsed.server.isValid();
        return parsed;
    };
    const auto acceptsPassword = [](const QStringList &authentication) {
        if (authentication.isEmpty())
            return true;
        return std::any_of(authentication.cbegin(), authentication.cend(), [](const QString &method) {
            return method.startsWith(QLatin1String("password-"));
        });
    };
    bool imapPasswords = true;
    while (!reader.atEnd()) {
        if (!reader.readNextStartElement()) {
            if (reader.isEndElement() && reader.name() == QLatin1String("emailProvider"))
                inProvider = false;
            continue;
        }
        const auto name = reader.name();
        if (name == QLatin1String("emailProvider")) {
            inProvider = true;
            continue;
        }
        if (!inProvider) {
            reader.skipCurrentElement();
            continue;
        }
        const QString type = reader.attributes().value(QLatin1String("type")).toString();
        if (name == QLatin1String("displayName") && result.providerName.isEmpty()) {
            result.providerName = reader.readElementText().trimmed();
        } else if (name == QLatin1String("incomingServer") && type == QLatin1String("imap")) {
            const Parsed parsed = readServer();
            if (parsed.usable && !result.imap.isValid()) {
                result.imap = parsed.server;
                imapPasswords = acceptsPassword(parsed.authentication);
            }
        } else if (name == QLatin1String("outgoingServer") && type == QLatin1String("smtp")) {
            const Parsed parsed = readServer();
            if (parsed.usable && !result.smtp.isValid())
                result.smtp = parsed.server;
        } else if (name == QLatin1String("documentation")) {
            const QUrl url(reader.attributes().value(QLatin1String("url")).toString());
            if (result.helpUrl.isEmpty() && url.isValid() && url.scheme() == QLatin1String("https") && !url.host().isEmpty())
                result.helpUrl = url;
            reader.skipCurrentElement();
        } else {
            reader.skipCurrentElement();
        }
    }
    if (reader.hasError() && reader.error() != QXmlStreamReader::PrematureEndOfDocumentError)
        return Result();
    if (!result.imap.isValid())
        return Result();
    if (result.imap.username.isEmpty())
        result.imap.username = address.address;
    if (result.smtp.isValid() && result.smtp.username.isEmpty())
        result.smtp.username = result.imap.username;
    result.acceptsPasswords = imapPasswords;
    return result;
}

void applyTo(QSettings &settings, const Result &result, bool useOAuth)
{
    using Common::SettingsNames;
    if (!result.isValid())
        return;
    if (settings.value(SettingsNames::imapHostKey).toString() != result.imap.host)
        settings.remove(SettingsNames::imapSslPemPubKey);
    switch (result.imap.security) {
    case Security::Tls:
        settings.setValue(SettingsNames::imapMethodKey, SettingsNames::methodSSL);
        settings.setValue(SettingsNames::imapStartTlsKey, true);
        break;
    case Security::StartTls:
        settings.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
        settings.setValue(SettingsNames::imapStartTlsKey, true);
        break;
    case Security::None:
        settings.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
        settings.setValue(SettingsNames::imapStartTlsKey, false);
        break;
    }
    settings.setValue(SettingsNames::imapHostKey, result.imap.host);
    settings.setValue(SettingsNames::imapPortKey, result.imap.port);
    settings.setValue(SettingsNames::imapUserKey, result.imap.username);

    if (result.smtp.isValid()) {
        if (settings.value(SettingsNames::smtpHostKey).toString() != result.smtp.host)
            settings.remove(SettingsNames::smtpSslPemPubKey);
        if (result.smtp.security == Security::Tls) {
            settings.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSSMTP);
        } else {
            settings.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSMTP);
            settings.setValue(SettingsNames::smtpStartTlsKey, result.smtp.security == Security::StartTls);
        }
        settings.setValue(SettingsNames::smtpHostKey, result.smtp.host);
        settings.setValue(SettingsNames::smtpPortKey, result.smtp.port);
        settings.setValue(SettingsNames::smtpAuthKey, true);
        settings.setValue(SettingsNames::smtpUserKey, result.smtp.username);
        settings.setValue(SettingsNames::smtpAuthReuseImapCredsKey, result.smtp.username == result.imap.username);
    }

    if (useOAuth && !result.oauthProvider.isEmpty()) {
        settings.setValue(SettingsNames::oauthProviderKey, result.oauthProvider);
        settings.setValue(SettingsNames::imapAuthMethodKey, SettingsNames::authMethodOAuth);
        if (result.smtp.isValid())
            settings.setValue(SettingsNames::smtpAuthMethodKey, SettingsNames::authMethodOAuth);
    } else {
        settings.remove(SettingsNames::imapAuthMethodKey);
        settings.remove(SettingsNames::smtpAuthMethodKey);
    }
}

Network::~Network() = default;

Discovery::Discovery(std::unique_ptr<Network> network, QObject *parent)
    : QObject(parent)
    , m_network(std::move(network))
{
}

Discovery::~Discovery()
{
    cancel();
}

bool Discovery::isRunning() const
{
    return m_running;
}

void Discovery::cancel()
{
    ++m_generation;
    m_running = false;
    m_probes.clear();
    if (m_network)
        m_network->abort();
}

void Discovery::start(const QString &address)
{
    cancel();
    m_running = true;
    m_address = parseAddress(address);
    m_srv = Result();
    m_srvAuthenticated = true;
    m_mxHost.clear();
    m_mxAuthenticated = false;
    const quint64 generation = m_generation;
    // Results are always delivered after start() returns
    QTimer::singleShot(0, this, [this, generation]() {
        if (generation != m_generation)
            return;
        if (!m_address.isValid()) {
            finish(Result());
            return;
        }
        if (const Provider *provider = providerForDomain(m_address.domain)) {
            enter(Source::BuiltIn);
            finish(fromProvider(*provider, m_address, Source::BuiltIn));
            return;
        }
        tryAutoconfig(0);
    });
}

void Discovery::enter(Source source)
{
    emit stageChanged(source);
}

void Discovery::tryAutoconfig(int step)
{
    if (step == 0)
        enter(Source::Autoconfig);
    QUrl url;
    if (step == 0) {
        url = QUrl(QStringLiteral("https://autoconfig.%1/mail/config-v1.1.xml").arg(m_address.asciiDomain));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("emailaddress"), QString::fromLatin1(QUrl::toPercentEncoding(m_address.address)));
        url.setQuery(query);
    } else {
        url = QUrl(QStringLiteral("https://%1/.well-known/autoconfig/mail/config-v1.1.xml").arg(m_address.asciiDomain));
    }
    const quint64 generation = m_generation;
    m_network->fetch(url, [this, generation, step](bool ok, const QByteArray &body) {
        if (generation != m_generation)
            return;
        Result result = ok ? parseAutoconfig(body, m_address) : Result();
        if (result.isValid()) {
            result.source = Source::Autoconfig;
            describeProvider(result);
            finish(result);
        } else if (step == 0) {
            tryAutoconfig(1);
        } else {
            tryIspdb(m_address.asciiDomain, false);
        }
    });
}

void Discovery::tryIspdb(const QString &domain, bool fromMx)
{
    if (!fromMx)
        enter(Source::Ispdb);
    const QUrl url(QStringLiteral("https://autoconfig.thunderbird.net/v1.1/%1").arg(domain));
    const quint64 generation = m_generation;
    m_network->fetch(url, [this, generation, fromMx](bool ok, const QByteArray &body) {
        if (generation != m_generation)
            return;
        Result result = ok ? parseAutoconfig(body, m_address) : Result();
        if (result.isValid()) {
            result.source = fromMx ? Source::Mx : Source::Ispdb;
            describeProvider(result);
            if (fromMx)
                markUnconfirmed(result, m_mxAuthenticated);
            finish(result);
        } else if (fromMx) {
            tryGuess();
        } else {
            trySrv(0);
        }
    });
}

void Discovery::trySrv(int step)
{
    if (step == 0)
        enter(Source::Srv);
    static const char *const services[] = {"_imaps._tcp.", "_imap._tcp.", "_submissions._tcp.", "_submission._tcp."};
    const QString name = QString::fromLatin1(services[step]) + m_address.asciiDomain;
    const quint64 generation = m_generation;
    m_network->lookupSrv(name, [this, generation, step](const QList<Network::SrvRecord> &records, bool authenticated) {
        if (generation != m_generation)
            return;
        const Network::SrvRecord *record = bestSrv(records);
        if (record) {
            Server found;
            found.host = normalizedHost(record->target);
            found.port = record->port;
            found.security = step % 2 == 0 ? Security::Tls : Security::StartTls;
            found.username = m_address.address;
            if (step < 2)
                m_srv.imap = found;
            else
                m_srv.smtp = found;
            m_srvAuthenticated = m_srvAuthenticated && authenticated;
        }
        if (step < 2 && record) {
            trySrv(2);
        } else if (step == 0) {
            trySrv(1);
        } else if (step == 1) {
            tryMx();
        } else if (step == 2 && !record) {
            trySrv(3);
        } else {
            Result result = m_srv;
            result.source = Source::Srv;
            describeProvider(result);
            markUnconfirmed(result, m_srvAuthenticated);
            finish(result);
        }
    });
}

void Discovery::tryMx()
{
    enter(Source::Mx);
    const quint64 generation = m_generation;
    m_network->lookupMx(m_address.asciiDomain, [this, generation](const QStringList &hosts, bool authenticated) {
        if (generation != m_generation)
            return;
        const QString host = hosts.isEmpty() ? QString() : normalizedHost(hosts.constFirst());
        if (host.isEmpty()) {
            tryGuess();
            return;
        }
        m_mxHost = host;
        m_mxAuthenticated = authenticated;
        if (const Provider *provider = providerForMx(host)) {
            Result result = fromProvider(*provider, m_address, Source::Mx);
            markUnconfirmed(result, authenticated);
            finish(result);
            return;
        }
        const QString base = baseDomain(host);
        if (base.isEmpty() || base == m_address.asciiDomain) {
            tryGuess();
            return;
        }
        tryIspdb(base, true);
    });
}

void Discovery::tryGuess()
{
    enter(Source::Guess);
    QStringList imapHosts{QStringLiteral("imap.") + m_address.asciiDomain, QStringLiteral("mail.") + m_address.asciiDomain,
                          m_address.asciiDomain};
    QStringList smtpHosts{QStringLiteral("smtp.") + m_address.asciiDomain, QStringLiteral("mail.") + m_address.asciiDomain,
                          m_address.asciiDomain};
    if (!m_mxHost.isEmpty() && !imapHosts.contains(m_mxHost)) {
        imapHosts << m_mxHost;
        smtpHosts << m_mxHost;
    }
    m_probes.clear();
    for (const QString &host : std::as_const(imapHosts)) {
        m_probes.append(Probe{true, host, portImaps, Security::Tls, -1});
        m_probes.append(Probe{true, host, portImap, Security::StartTls, -1});
    }
    for (const QString &host : std::as_const(smtpHosts)) {
        m_probes.append(Probe{false, host, portSubmissions, Security::Tls, -1});
        m_probes.append(Probe{false, host, portSubmission, Security::StartTls, -1});
    }
    const quint64 generation = m_generation;
    for (int i = 0; i < m_probes.size(); ++i) {
        const Probe &probe = m_probes.at(i);
        m_network->probe(probe.host, probe.port, probe.security, [this, generation, i](bool reachable) {
            if (generation != m_generation || i >= m_probes.size())
                return;
            m_probes[i].state = reachable ? 1 : 0;
            guessProbed();
        });
    }
}

void Discovery::guessProbed()
{
    const bool pending = std::any_of(m_probes.cbegin(), m_probes.cend(), [](const Probe &probe) { return probe.state < 0; });
    if (pending)
        return;
    Result result;
    for (const Probe &probe : std::as_const(m_probes)) {
        if (probe.state != 1)
            continue;
        Server &target = probe.imap ? result.imap : result.smtp;
        if (target.isValid())
            continue;
        target.host = probe.host;
        target.port = probe.port;
        target.security = probe.security;
        target.username = m_address.address;
    }
    m_probes.clear();
    if (!result.isValid()) {
        finish(Result());
        return;
    }
    result.source = Source::Guess;
    describeProvider(result);
    markUnconfirmed(result, false);
    finish(result);
}

void Discovery::describeProvider(Result &result) const
{
    const Provider *provider = providerForServer(result.imap.host);
    if (!provider)
        return;
    if (result.providerName.isEmpty())
        result.providerName = provider->displayName;
    result.oauthProvider = provider->oauthProvider;
    result.acceptsPasswords = result.acceptsPasswords && provider->acceptsPasswords;
    result.guidance = provider->guidance;
    if (result.helpUrl.isEmpty())
        result.helpUrl = provider->helpUrl;
}

void Discovery::markUnconfirmed(Result &result, bool authenticated) const
{
    if (authenticated)
        return;
    for (const Server *server : {&result.imap, &result.smtp}) {
        if (!server->isValid() || isWithin(server->host, m_address.asciiDomain) || providerForServer(server->host))
            continue;
        if (!result.unconfirmedHosts.contains(server->host))
            result.unconfirmedHosts << server->host;
    }
}

void Discovery::finish(Result result)
{
    m_running = false;
    // A provider that takes no passwords and offers no OAuth leaves nothing to sign in with
    if (!result.acceptsPasswords && result.oauthProvider.isEmpty() && result.guidance == Guidance::None)
        result.guidance = Guidance::OAuthOnly;
    emit finished(result);
}

}
}
