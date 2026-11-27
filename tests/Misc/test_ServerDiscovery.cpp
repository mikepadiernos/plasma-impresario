/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <memory>

#include "Common/SettingsNames.h"
#include "UiUtils/ServerDiscovery.h"
#include "Utils/FakeDiscoveryNetwork.h"

using namespace UiUtils::ServerDiscovery;
using Common::SettingsNames;
using Tests::FakeDiscoveryNetwork;

namespace {

QByteArray autoconfig(const QByteArray &servers, const QByteArray &extra = QByteArray())
{
    return "<?xml version=\"1.0\"?>\n<clientConfig version=\"1.1\">\n"
           "  <emailProvider id=\"example\">\n    <domain>example.org</domain>\n"
           "    <displayName>Example Mail</displayName>\n"
        + servers + extra + "  </emailProvider>\n</clientConfig>\n";
}

QByteArray imapServer(const char *host, int port, const char *socketType, const char *username = "%EMAILADDRESS%",
                      const char *authentication = "password-cleartext")
{
    return QByteArray("    <incomingServer type=\"imap\">\n      <hostname>") + host + "</hostname>\n      <port>"
        + QByteArray::number(port) + "</port>\n      <socketType>" + socketType + "</socketType>\n      <username>" + username
        + "</username>\n      <authentication>" + authentication + "</authentication>\n    </incomingServer>\n";
}

QByteArray smtpServer(const char *host, int port, const char *socketType, const char *authentication = "password-cleartext")
{
    return QByteArray("    <outgoingServer type=\"smtp\">\n      <hostname>") + host + "</hostname>\n      <port>"
        + QByteArray::number(port) + "</port>\n      <socketType>" + socketType
        + "</socketType>\n      <username>%EMAILADDRESS%</username>\n      <authentication>" + authentication
        + "</authentication>\n    </outgoingServer>\n";
}

Network::SrvRecord srv(const char *target, quint16 port, quint16 priority = 0, quint16 weight = 0)
{
    Network::SrvRecord record;
    record.target = QString::fromLatin1(target);
    record.port = port;
    record.priority = priority;
    record.weight = weight;
    return record;
}

struct Run {
    std::shared_ptr<FakeDiscoveryNetwork::State> state = std::make_shared<FakeDiscoveryNetwork::State>();
    std::unique_ptr<Discovery> discovery;
    std::unique_ptr<QSignalSpy> finished;
    std::unique_ptr<QSignalSpy> stages;

    Run()
        : discovery(std::make_unique<Discovery>(std::make_unique<FakeDiscoveryNetwork>(state)))
        , finished(std::make_unique<QSignalSpy>(discovery.get(), &Discovery::finished))
        , stages(std::make_unique<QSignalSpy>(discovery.get(), &Discovery::stageChanged))
    {
    }

    Result result() const { return finished->constLast().constFirst().value<Result>(); }

    QList<Source> stageList() const
    {
        QList<Source> out;
        for (const auto &arguments : *stages)
            out << arguments.constFirst().value<Source>();
        return out;
    }

    bool run(const QString &address)
    {
        discovery->start(address);
        return finished->wait(2000) || !finished->isEmpty();
    }
};

}

class ServerDiscoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<Result>();
        qRegisterMetaType<Source>();
    }

    void parsesAddresses()
    {
        Address address = parseAddress(QStringLiteral("  Jo.Doe@Example.ORG. "));
        QVERIFY(address.isValid());
        QCOMPARE(address.localPart, QStringLiteral("Jo.Doe"));
        QCOMPARE(address.domain, QStringLiteral("example.org"));
        QCOMPARE(address.asciiDomain, QStringLiteral("example.org"));
        QCOMPARE(address.address, QStringLiteral("Jo.Doe@example.org"));

        address = parseAddress(QStringLiteral("anna@b\u00fccher.example"));
        QVERIFY(address.isValid());
        QCOMPARE(address.domain, QStringLiteral("b\u00fccher.example"));
        QCOMPARE(address.asciiDomain, QStringLiteral("xn--bcher-kva.example"));

        for (const char *invalid : {"", "jo", "@example.org", "jo@", "jo@localhost", "jo doe@example.org",
                                    "<jo@example.org>", "jo@.example.org", "jo@example..org", "jo@exa mple.org",
                                    "jo@exa_mple.org"}) {
            QVERIFY2(!parseAddress(QString::fromLatin1(invalid)).isValid(), invalid);
        }
    }

    void expandsPatterns()
    {
        const Address address = parseAddress(QStringLiteral("jo@example.org"));
        QCOMPARE(expand(QStringLiteral("%EMAILADDRESS%"), address), QStringLiteral("jo@example.org"));
        QCOMPARE(expand(QStringLiteral("%EMAILLOCALPART%"), address), QStringLiteral("jo"));
        QCOMPARE(expand(QStringLiteral("%EMAILDOMAIN%\\%EMAILLOCALPART%"), address), QStringLiteral("example.org\\jo"));
    }

    void matchesDomains()
    {
        QVERIFY(isWithin(QStringLiteral("imap.example.org"), QStringLiteral("example.org")));
        QVERIFY(isWithin(QStringLiteral("Example.ORG."), QStringLiteral("example.org")));
        QVERIFY(!isWithin(QStringLiteral("badexample.org"), QStringLiteral("example.org")));
        QVERIFY(!isWithin(QStringLiteral("example.org.evil.test"), QStringLiteral("example.org")));
        QVERIFY(!isWithin(QString(), QStringLiteral("example.org")));
    }

    void knowsProviders()
    {
        QCOMPARE(providerForDomain(QStringLiteral("GMAIL.com"))->id, QStringLiteral("google"));
        QCOMPARE(providerForDomain(QStringLiteral("hotmail.co.uk"))->id, QStringLiteral("outlook"));
        QVERIFY(!providerForDomain(QStringLiteral("example.org")));
        QCOMPARE(providerForMx(QStringLiteral("eur.olc.protection.outlook.com"))->id, QStringLiteral("outlook"));
        QCOMPARE(providerForMx(QStringLiteral("example-org.mail.protection.outlook.com"))->id, QStringLiteral("microsoft365"));
        QCOMPARE(providerForMx(QStringLiteral("aspmx.l.google.com."))->id, QStringLiteral("google"));
        QVERIFY(!providerForMx(QStringLiteral("mx.example.org")));
        QCOMPARE(providerForServer(QStringLiteral("imap.mail.me.com"))->id, QStringLiteral("icloud"));
        for (const Provider &provider : providers()) {
            QVERIFY2(provider.imap.isValid(), qPrintable(provider.id));
            QVERIFY2(provider.smtp.isValid(), qPrintable(provider.id));
            QVERIFY2(provider.imap.security != Security::None, qPrintable(provider.id));
            QVERIFY2(provider.helpUrl.isEmpty() || provider.helpUrl.scheme() == QLatin1String("https"), qPrintable(provider.id));
            // Every server is recognised as the provider's own, so that it never needs confirmation
            QVERIFY2(providerForServer(provider.imap.host), qPrintable(provider.id));
            QVERIFY2(providerForServer(provider.smtp.host), qPrintable(provider.id));
        }
    }

    void parsesAutoconfig()
    {
        const Address address = parseAddress(QStringLiteral("jo@example.org"));
        const Result result = parseAutoconfig(
            autoconfig(imapServer("IMAP.example.org.", 993, "SSL") + smtpServer("smtp.example.org", 465, "SSL")
                           + smtpServer("smtp.example.org", 587, "STARTTLS"),
                       "    <documentation url=\"http://example.org/plain\"/>\n"
                       "    <documentation url=\"https://example.org/help\"><descr>Help</descr></documentation>\n"),
            address);
        QVERIFY(result.isValid());
        QCOMPARE(result.providerName, QStringLiteral("Example Mail"));
        QCOMPARE(result.imap.host, QStringLiteral("imap.example.org"));
        QCOMPARE(result.imap.port, quint16(993));
        QCOMPARE(result.imap.security, Security::Tls);
        QCOMPARE(result.imap.username, QStringLiteral("jo@example.org"));
        QCOMPARE(result.smtp.port, quint16(465));
        QCOMPARE(result.smtp.security, Security::Tls);
        QVERIFY(result.acceptsPasswords);
        QCOMPARE(result.helpUrl, QUrl(QStringLiteral("https://example.org/help")));
    }

    void autoconfigSkipsServersWithoutEncryption()
    {
        const Address address = parseAddress(QStringLiteral("jo@example.org"));
        QVERIFY(!parseAutoconfig(autoconfig(imapServer("imap.example.org", 143, "plain")), address).isValid());
        const Result result = parseAutoconfig(
            autoconfig(imapServer("imap.example.org", 143, "plain") + imapServer("imap.example.org", 143, "STARTTLS", "%EMAILLOCALPART%")
                       + smtpServer("smtp.example.org", 25, "plain")),
            address);
        QVERIFY(result.isValid());
        QCOMPARE(result.imap.security, Security::StartTls);
        QCOMPARE(result.imap.username, QStringLiteral("jo"));
        QVERIFY(!result.smtp.isValid());
    }

    void autoconfigNotesOAuthOnlyServers()
    {
        const Address address = parseAddress(QStringLiteral("jo@example.org"));
        const Result result = parseAutoconfig(
            autoconfig(imapServer("outlook.office365.com", 993, "SSL", "%EMAILADDRESS%", "OAuth2")
                       + smtpServer("smtp-mail.outlook.com", 587, "STARTTLS", "OAuth2")),
            address);
        QVERIFY(result.isValid());
        QVERIFY(!result.acceptsPasswords);
    }

    void autoconfigRejectsOtherDocuments()
    {
        const Address address = parseAddress(QStringLiteral("jo@example.org"));
        QVERIFY(!parseAutoconfig("<!DOCTYPE HTML><html><body>404 Not Found</body></html>", address).isValid());
        QVERIFY(!parseAutoconfig(QByteArray(), address).isValid());
        QVERIFY(!parseAutoconfig("<clientConfig><emailProvider><incomingServer type=\"pop3\"><hostname>pop.example.org"
                                 "</hostname><port>995</port><socketType>SSL</socketType></incomingServer>"
                                 "</emailProvider></clientConfig>",
                                 address)
                     .isValid());
        // A missing user name means the address
        const Result result = parseAutoconfig(
            "<clientConfig><emailProvider><incomingServer type=\"imap\"><hostname>imap.example.org</hostname>"
            "<port>993</port><socketType>SSL</socketType></incomingServer></emailProvider></clientConfig>",
            address);
        QVERIFY(result.isValid());
        QCOMPARE(result.imap.username, QStringLiteral("jo@example.org"));
    }

    void builtInProvidersNeedNoNetwork()
    {
        Run run;
        run.discovery->start(QStringLiteral("jo@gmail.com"));
        // The result arrives from the event loop, never from within start()
        QVERIFY(run.finished->isEmpty());
        QVERIFY(run.finished->wait(1000));
        QVERIFY(run.state->log.isEmpty());
        const Result result = run.result();
        QCOMPARE(result.source, Source::BuiltIn);
        QCOMPARE(result.imap.host, QStringLiteral("imap.gmail.com"));
        QCOMPARE(result.smtp.host, QStringLiteral("smtp.gmail.com"));
        QCOMPARE(result.oauthProvider, QStringLiteral("google"));
        QCOMPARE(result.guidance, Guidance::AppPassword);
        QVERIFY(!result.needsConfirmation());
        QCOMPARE(run.stageList(), QList<Source>{Source::BuiltIn});

        QVERIFY(run.run(QStringLiteral("jo@icloud.com")));
        QCOMPARE(run.result().imap.username, QStringLiteral("jo"));
        QCOMPARE(run.result().smtp.username, QStringLiteral("jo@icloud.com"));
    }

    void triesEachSourceInOrder()
    {
        Run run;
        run.state->mx.insert(QStringLiteral("example.org"), {QStringLiteral("mx.hosting.test")});
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        QVERIFY(!run.result().isValid());
        const QStringList lookups = run.state->log.filter(QRegularExpression(QStringLiteral("^(GET|SRV|MX) ")));
        QCOMPARE(lookups,
                 (QStringList{
                     QStringLiteral("GET autoconfig.example.org/mail/config-v1.1.xml"),
                     QStringLiteral("GET example.org/.well-known/autoconfig/mail/config-v1.1.xml"),
                     QStringLiteral("GET autoconfig.thunderbird.net/v1.1/example.org"),
                     QStringLiteral("SRV _imaps._tcp.example.org"),
                     QStringLiteral("SRV _imap._tcp.example.org"),
                     QStringLiteral("MX example.org"),
                     QStringLiteral("GET autoconfig.thunderbird.net/v1.1/hosting.test"),
                 }));
        QCOMPARE(QUrlQuery(run.state->urls.constFirst()).queryItemValue(QStringLiteral("emailaddress"), QUrl::FullyDecoded),
                 QStringLiteral("jo@example.org"));
        QCOMPARE(run.stageList(),
                 (QList<Source>{Source::Autoconfig, Source::Ispdb, Source::Srv, Source::Mx, Source::Guess}));
        // The guesses are probed in this order of preference, the MX host last
        const QStringList probes = run.state->log.filter(QStringLiteral("PROBE "));
        QCOMPARE(probes,
                 (QStringList{
                     QStringLiteral("PROBE imap.example.org:993"), QStringLiteral("PROBE imap.example.org:143"),
                     QStringLiteral("PROBE mail.example.org:993"), QStringLiteral("PROBE mail.example.org:143"),
                     QStringLiteral("PROBE example.org:993"), QStringLiteral("PROBE example.org:143"),
                     QStringLiteral("PROBE mx.hosting.test:993"), QStringLiteral("PROBE mx.hosting.test:143"),
                     QStringLiteral("PROBE smtp.example.org:465"), QStringLiteral("PROBE smtp.example.org:587"),
                     QStringLiteral("PROBE mail.example.org:465"), QStringLiteral("PROBE mail.example.org:587"),
                     QStringLiteral("PROBE example.org:465"), QStringLiteral("PROBE example.org:587"),
                     QStringLiteral("PROBE mx.hosting.test:465"), QStringLiteral("PROBE mx.hosting.test:587"),
                 }));
        QCOMPARE(run.finished->size(), 1);
    }

    void usesTheDomainsAutoconfig()
    {
        Run run;
        run.state->pages.insert(QStringLiteral("example.org/.well-known/autoconfig/mail/config-v1.1.xml"),
                                autoconfig(imapServer("imap.example.org", 993, "SSL") + smtpServer("smtp.example.org", 587, "STARTTLS")));
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        const Result result = run.result();
        QCOMPARE(result.source, Source::Autoconfig);
        QCOMPARE(result.smtp.security, Security::StartTls);
        QVERIFY(!result.needsConfirmation());
        QCOMPARE(run.state->log.size(), 2);
    }

    void autoconfigOfAKnownProviderGetsItsGuidance()
    {
        // A custom domain hosted by Google Workspace
        Run run;
        run.state->pages.insert(QStringLiteral("autoconfig.thunderbird.net/v1.1/example.org"),
                                autoconfig(imapServer("imap.gmail.com", 993, "SSL") + smtpServer("smtp.gmail.com", 465, "SSL")));
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        const Result result = run.result();
        QCOMPARE(result.source, Source::Ispdb);
        QCOMPARE(result.providerName, QStringLiteral("Example Mail"));
        QCOMPARE(result.oauthProvider, QStringLiteral("google"));
        QCOMPARE(result.guidance, Guidance::AppPassword);
        QVERIFY(!result.helpUrl.isEmpty());
    }

    void unknownOAuthOnlyProviderIsExplained()
    {
        Run run;
        run.state->pages.insert(QStringLiteral("autoconfig.example.org/mail/config-v1.1.xml"),
                                autoconfig(imapServer("imap.example.org", 993, "SSL", "%EMAILADDRESS%", "OAuth2")));
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        QCOMPARE(run.result().guidance, Guidance::OAuthOnly);
        QVERIFY(run.result().oauthProvider.isEmpty());
    }

    void usesSrvRecords()
    {
        Run run;
        run.state->srv.insert(QStringLiteral("_imaps._tcp.example.org"),
                              {srv("imap2.example.org.", 993, 20), srv("imap.example.org.", 993, 10, 5), srv("imap3.example.org", 993, 10, 1)});
        run.state->srv.insert(QStringLiteral("_submissions._tcp.example.org"), {srv(".", 0)});
        run.state->srv.insert(QStringLiteral("_submission._tcp.example.org"), {srv("smtp.example.org", 587)});
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        const Result result = run.result();
        QCOMPARE(result.source, Source::Srv);
        QCOMPARE(result.imap.host, QStringLiteral("imap.example.org"));
        QCOMPARE(result.imap.security, Security::Tls);
        QCOMPARE(result.smtp.host, QStringLiteral("smtp.example.org"));
        QCOMPARE(result.smtp.security, Security::StartTls);
        QVERIFY(!run.state->log.contains(QStringLiteral("SRV _imap._tcp.example.org")));
        // Hosts inside the mail domain need no confirmation
        QVERIFY(!result.needsConfirmation());
    }

    void srvServiceDeclaredUnavailableFallsThrough()
    {
        Run run;
        run.state->srv.insert(QStringLiteral("_imaps._tcp.example.org"), {srv(".", 0)});
        run.state->srv.insert(QStringLiteral("_imap._tcp.example.org"), {srv("mail.example.org", 143)});
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        QCOMPARE(run.result().imap.host, QStringLiteral("mail.example.org"));
        QCOMPARE(run.result().imap.security, Security::StartTls);
        QVERIFY(!run.result().smtp.isValid());
    }

    void unsignedSrvElsewhereNeedsConfirmation()
    {
        Run run;
        run.state->srv.insert(QStringLiteral("_imaps._tcp.example.org"), {srv("imap.hosting.test", 993)});
        run.state->srv.insert(QStringLiteral("_submissions._tcp.example.org"), {srv("smtp.gmail.com", 465)});
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        // A known provider's server is not listed
        QCOMPARE(run.result().unconfirmedHosts, QStringList{QStringLiteral("imap.hosting.test")});

        Run signedRun;
        signedRun.state->srv.insert(QStringLiteral("_imaps._tcp.example.org"), {srv("imap.hosting.test", 993)});
        signedRun.state->authenticatedNames.insert(QStringLiteral("_imaps._tcp.example.org"));
        signedRun.state->authenticatedNames.insert(QStringLiteral("_submissions._tcp.example.org"));
        signedRun.state->authenticatedNames.insert(QStringLiteral("_submission._tcp.example.org"));
        QVERIFY(signedRun.run(QStringLiteral("jo@example.org")));
        QVERIFY(!signedRun.result().needsConfirmation());
    }

    void recognisesProvidersByMx()
    {
        Run run;
        run.state->mx.insert(QStringLiteral("example.org"),
                             {QStringLiteral("example-org.mail.protection.outlook.com."), QStringLiteral("backup.example.org")});
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        const Result result = run.result();
        QCOMPARE(result.source, Source::Mx);
        QCOMPARE(result.providerName, QStringLiteral("Microsoft 365"));
        QCOMPARE(result.imap.host, QStringLiteral("outlook.office365.com"));
        QCOMPARE(result.smtp.host, QStringLiteral("smtp.office365.com"));
        QCOMPARE(result.oauthProvider, QStringLiteral("microsoft"));
        QVERIFY(!result.acceptsPasswords);
        QCOMPARE(result.guidance, Guidance::OAuthOnly);
        QVERIFY(!result.needsConfirmation());
    }

    void looksUpTheMxDomainInTheIspdb()
    {
        Run run;
        run.state->mx.insert(QStringLiteral("example.org"), {QStringLiteral("mx1.mail.hosting.test")});
        run.state->pages.insert(QStringLiteral("autoconfig.thunderbird.net/v1.1/hosting.test"),
                                autoconfig(imapServer("imap.hosting.test", 993, "SSL") + smtpServer("smtp.hosting.test", 465, "SSL")));
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        Result result = run.result();
        QCOMPARE(result.source, Source::Mx);
        QCOMPARE(result.imap.host, QStringLiteral("imap.hosting.test"));
        QCOMPARE(result.unconfirmedHosts, (QStringList{QStringLiteral("imap.hosting.test"), QStringLiteral("smtp.hosting.test")}));

        Run signedRun;
        signedRun.state->mx = run.state->mx;
        signedRun.state->pages = run.state->pages;
        signedRun.state->authenticatedNames.insert(QStringLiteral("example.org"));
        QVERIFY(signedRun.run(QStringLiteral("jo@example.org")));
        QVERIFY(!signedRun.result().needsConfirmation());
    }

    void guessesPreferTheFirstReachableName()
    {
        Run run;
        run.state->reachable = {QStringLiteral("mail.example.org:993"), QStringLiteral("imap.example.org:143"),
                                QStringLiteral("smtp.example.org:587"), QStringLiteral("mail.example.org:465")};
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        const Result result = run.result();
        QCOMPARE(result.source, Source::Guess);
        QCOMPARE(result.imap.host, QStringLiteral("imap.example.org"));
        QCOMPARE(result.imap.port, quint16(143));
        QCOMPARE(result.imap.security, Security::StartTls);
        QCOMPARE(result.smtp.host, QStringLiteral("smtp.example.org"));
        QCOMPARE(result.smtp.port, quint16(587));
        QCOMPARE(result.imap.username, QStringLiteral("jo@example.org"));
        QVERIFY(!result.needsConfirmation());
    }

    void guessedMxHostNeedsConfirmation()
    {
        Run run;
        run.state->mx.insert(QStringLiteral("example.org"), {QStringLiteral("mx.hosting.test")});
        run.state->reachable = {QStringLiteral("mx.hosting.test:993")};
        QVERIFY(run.run(QStringLiteral("jo@example.org")));
        QCOMPARE(run.result().imap.host, QStringLiteral("mx.hosting.test"));
        QVERIFY(!run.result().smtp.isValid());
        QCOMPARE(run.result().unconfirmedHosts, QStringList{QStringLiteral("mx.hosting.test")});
    }

    void invalidAddressFindsNothing()
    {
        Run run;
        QVERIFY(run.run(QStringLiteral("not an address")));
        QVERIFY(!run.result().isValid());
        QVERIFY(run.state->log.isEmpty());
    }

    void cancelStopsWithoutAResult()
    {
        Run run;
        run.state->hold = true;
        run.discovery->start(QStringLiteral("jo@example.org"));
        QTRY_VERIFY(!run.state->held.isEmpty());
        QVERIFY(run.discovery->isRunning());
        run.discovery->cancel();
        QVERIFY(!run.discovery->isRunning());
        run.state->release();
        QTest::qWait(50);
        QVERIFY(run.finished->isEmpty());
        QVERIFY(run.state->aborts >= 1);
    }

    void restartIgnoresTheEarlierLookup()
    {
        Run run;
        // Even a network that delivers stale answers cannot mix two lookups
        run.state->dropOnAbort = false;
        run.state->hold = true;
        run.state->pages.insert(QStringLiteral("autoconfig.example.org/mail/config-v1.1.xml"),
                                autoconfig(imapServer("imap.example.org", 993, "SSL")));
        run.discovery->start(QStringLiteral("jo@example.org"));
        QTRY_VERIFY(!run.state->held.isEmpty());
        run.discovery->start(QStringLiteral("jo@gmail.com"));
        run.state->release();
        QVERIFY(run.finished->wait(1000));
        QTest::qWait(50);
        QCOMPARE(run.finished->size(), 1);
        QCOMPARE(run.result().imap.host, QStringLiteral("imap.gmail.com"));
    }

    void appliesSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings settings(directory.filePath(QStringLiteral("account.conf")), QSettings::IniFormat);
        settings.setValue(SettingsNames::imapHostKey, QStringLiteral("old.example.org"));
        settings.setValue(SettingsNames::imapSslPemPubKey, QByteArray("old-key"));
        settings.setValue(SettingsNames::smtpHostKey, QStringLiteral("smtp.example.org"));
        settings.setValue(SettingsNames::smtpSslPemPubKey, QByteArray("smtp-key"));

        Result result;
        result.source = Source::Autoconfig;
        result.imap.host = QStringLiteral("imap.example.org");
        result.imap.port = 143;
        result.imap.security = Security::StartTls;
        result.imap.username = QStringLiteral("jo");
        result.smtp.host = QStringLiteral("smtp.example.org");
        result.smtp.port = 587;
        result.smtp.security = Security::StartTls;
        result.smtp.username = QStringLiteral("jo");
        applyTo(settings, result, true);
        QCOMPARE(settings.value(SettingsNames::imapMethodKey).toString(), SettingsNames::methodTCP);
        QVERIFY(settings.value(SettingsNames::imapStartTlsKey).toBool());
        QCOMPARE(settings.value(SettingsNames::imapHostKey).toString(), QStringLiteral("imap.example.org"));
        QCOMPARE(settings.value(SettingsNames::imapPortKey).toInt(), 143);
        QCOMPARE(settings.value(SettingsNames::imapUserKey).toString(), QStringLiteral("jo"));
        // A certificate accepted for another server is forgotten; the same server keeps its own
        QVERIFY(!settings.contains(SettingsNames::imapSslPemPubKey));
        QCOMPARE(settings.value(SettingsNames::smtpSslPemPubKey).toByteArray(), QByteArray("smtp-key"));
        QCOMPARE(settings.value(SettingsNames::msaMethodKey).toString(), SettingsNames::methodSMTP);
        QVERIFY(settings.value(SettingsNames::smtpStartTlsKey).toBool());
        QCOMPARE(settings.value(SettingsNames::smtpPortKey).toInt(), 587);
        QVERIFY(settings.value(SettingsNames::smtpAuthKey).toBool());
        QVERIFY(settings.value(SettingsNames::smtpAuthReuseImapCredsKey).toBool());
        // Without an OAuth provider, OAuth cannot be chosen
        QVERIFY(!settings.contains(SettingsNames::imapAuthMethodKey));

        result.imap.security = Security::Tls;
        result.imap.port = 993;
        result.smtp.security = Security::Tls;
        result.smtp.port = 465;
        result.smtp.username = QStringLiteral("jo@example.org");
        result.oauthProvider = QStringLiteral("google");
        applyTo(settings, result, true);
        QCOMPARE(settings.value(SettingsNames::imapMethodKey).toString(), SettingsNames::methodSSL);
        QCOMPARE(settings.value(SettingsNames::msaMethodKey).toString(), SettingsNames::methodSSMTP);
        QVERIFY(!settings.value(SettingsNames::smtpAuthReuseImapCredsKey).toBool());
        QCOMPARE(settings.value(SettingsNames::oauthProviderKey).toString(), QStringLiteral("google"));
        QCOMPARE(settings.value(SettingsNames::imapAuthMethodKey).toString(), SettingsNames::authMethodOAuth);
        QCOMPARE(settings.value(SettingsNames::smtpAuthMethodKey).toString(), SettingsNames::authMethodOAuth);

        applyTo(settings, result, false);
        QVERIFY(!settings.contains(SettingsNames::imapAuthMethodKey));
        QVERIFY(!settings.contains(SettingsNames::smtpAuthMethodKey));

        // Nothing found changes nothing
        applyTo(settings, Result(), false);
        QCOMPARE(settings.value(SettingsNames::imapHostKey).toString(), QStringLiteral("imap.example.org"));
    }

    void networkProbesWithoutSigningIn()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 open = server.serverPort();
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost));
        const quint16 closedPort = closed.serverPort();
        closed.close();

        const std::unique_ptr<Network> network = createNetwork();
        int reachable = -1;
        network->probe(QStringLiteral("127.0.0.1"), open, Security::StartTls, [&](bool ok) { reachable = ok; });
        QTRY_COMPARE(reachable, 1);
        reachable = -1;
        network->probe(QStringLiteral("127.0.0.1"), closedPort, Security::StartTls, [&](bool ok) { reachable = ok; });
        QTRY_COMPARE_WITH_TIMEOUT(reachable, 0, 7000);

        // An abort from within a callback, as when a dialog restarts the lookup, is safe
        int calls = 0;
        network->probe(QStringLiteral("127.0.0.1"), open, Security::StartTls, [&](bool) {
            ++calls;
            network->abort();
        });
        QTRY_COMPARE(calls, 1);
        // An aborted request never answers
        bool answered = false;
        network->probe(QStringLiteral("127.0.0.1"), open, Security::StartTls, [&](bool) { answered = true; });
        network->abort();
        QTest::qWait(200);
        QVERIFY(!answered);
    }

    void networkFetchesDocuments()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&server]() {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                const QByteArray request = socket->readAll();
                const bool found = request.startsWith("GET /found ");
                const QByteArray body = found ? QByteArray("<clientConfig/>") : QByteArray("missing");
                socket->write((found ? QByteArray("HTTP/1.1 200 OK\r\n") : QByteArray("HTTP/1.1 404 Not Found\r\n"))
                              + "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        const QString base = QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort());
        const std::unique_ptr<Network> network = createNetwork();
        int ok = -1;
        QByteArray body;
        network->fetch(QUrl(base + QStringLiteral("found")), [&](bool success, const QByteArray &data) {
            ok = success;
            body = data;
        });
        QTRY_COMPARE(ok, 1);
        QCOMPARE(body, QByteArray("<clientConfig/>"));
        ok = -1;
        network->fetch(QUrl(base + QStringLiteral("other")), [&](bool success, const QByteArray &data) {
            ok = success;
            body = data;
        });
        QTRY_COMPARE(ok, 0);
        QVERIFY(body.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);
    ServerDiscoveryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ServerDiscovery.moc"
