/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "ServerDiscovery.h"

#include <algorithm>
#include <memory>
#include <QDnsLookup>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSslSocket>
#include <QTimer>

namespace UiUtils {
namespace ServerDiscovery {

namespace {

/** Autoconfig documents are small; anything larger is not one */
constexpr qint64 maximumDocumentSize = 256 * 1024;
constexpr int fetchTimeoutMs = 10000;
constexpr int probeTimeoutMs = 6000;

class QtNetwork : public Network
{
public:
    QtNetwork()
        : m_context(std::make_unique<QObject>())
        , m_alive(std::make_shared<bool>(true))
    {
        m_manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }

    ~QtNetwork() override
    {
        *m_alive = false;
        // Requests must go before the manager which made them
        for (const QPointer<QObject> &old : std::as_const(m_retired))
            delete old.data();
    }

    void fetch(const QUrl &url, FetchDone done) override
    {
        QNetworkRequest request(url);
        request.setTransferTimeout(fetchTimeoutMs);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        QNetworkReply *reply = m_manager.get(request);
        reply->setParent(m_context.get());
        QObject::connect(reply, &QNetworkReply::downloadProgress, m_context.get(), [reply](qint64 received, qint64) {
            if (received > maximumDocumentSize)
                reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, m_context.get(), [reply, done, alive = m_alive]() {
            reply->deleteLater();
            if (!*alive)
                return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = reply->error() == QNetworkReply::NoError && status == 200;
            done(ok, ok ? reply->read(maximumDocumentSize) : QByteArray());
        });
    }

    void lookupSrv(const QString &name, SrvDone done) override
    {
        auto *lookup = new QDnsLookup(QDnsLookup::SRV, name, m_context.get());
        QObject::connect(lookup, &QDnsLookup::finished, m_context.get(), [lookup, done, alive = m_alive]() {
            lookup->deleteLater();
            if (!*alive)
                return;
            QList<SrvRecord> records;
            if (lookup->error() == QDnsLookup::NoError) {
                const auto found = lookup->serviceRecords();
                for (const auto &record : found)
                    records.append(SrvRecord{record.target(), record.port(), record.priority(), record.weight()});
            }
            done(records, lookup->error() == QDnsLookup::NoError && lookup->isAuthenticData());
        });
        lookup->lookup();
    }

    void lookupMx(const QString &domain, MxDone done) override
    {
        auto *lookup = new QDnsLookup(QDnsLookup::MX, domain, m_context.get());
        QObject::connect(lookup, &QDnsLookup::finished, m_context.get(), [lookup, done, alive = m_alive]() {
            lookup->deleteLater();
            if (!*alive)
                return;
            QStringList hosts;
            if (lookup->error() == QDnsLookup::NoError) {
                auto records = lookup->mailExchangeRecords();
                std::stable_sort(records.begin(), records.end(), [](const QDnsMailExchangeRecord &a, const QDnsMailExchangeRecord &b) {
                    return a.preference() < b.preference();
                });
                for (const auto &record : std::as_const(records)) {
                    QString host = record.exchange();
                    while (host.endsWith(QLatin1Char('.')))
                        host.chop(1);
                    // A null MX (RFC 7505) says that the domain takes no mail
                    if (!host.isEmpty())
                        hosts << host;
                }
            }
            done(hosts, lookup->error() == QDnsLookup::NoError && lookup->isAuthenticData());
        });
        lookup->lookup();
    }

    void probe(const QString &host, quint16 port, Security security, ProbeDone done) override
    {
        auto *socket = new QSslSocket(m_context.get());
        auto *timer = new QTimer(socket);
        timer->setSingleShot(true);
        auto answered = std::make_shared<bool>(false);
        const auto answer = [socket, answered, done, alive = m_alive](bool reachable) {
            if (*answered)
                return;
            *answered = true;
            socket->abort();
            socket->deleteLater();
            if (*alive)
                done(reachable);
        };
        QObject::connect(timer, &QTimer::timeout, socket, [answer]() { answer(false); });
        QObject::connect(socket, &QAbstractSocket::errorOccurred, socket, [answer]() { answer(false); });
        if (security == Security::Tls) {
            QObject::connect(socket, &QSslSocket::encrypted, socket, [answer]() { answer(true); });
            // The server speaks TLS; whether its certificate is trusted is decided when the account connects
            QObject::connect(socket, &QSslSocket::sslErrors, socket, [answer]() { answer(true); });
            socket->connectToHostEncrypted(host, port);
        } else {
            QObject::connect(socket, &QAbstractSocket::connected, socket, [answer]() { answer(true); });
            socket->connectToHost(host, port);
        }
        timer->start(probeTimeoutMs);
    }

    void abort() override
    {
        // abort() may run inside a callback of the request being dropped, so the requests are only silenced here
        // and deleted from the event loop
        *m_alive = false;
        m_alive = std::make_shared<bool>(true);
        QObject *old = m_context.release();
        m_context = std::make_unique<QObject>();
        m_retired.removeAll(QPointer<QObject>());
        m_retired.append(QPointer<QObject>(old));
        old->deleteLater();
    }

private:
    QNetworkAccessManager m_manager;
    std::unique_ptr<QObject> m_context;
    /** False once the requests made so far were aborted */
    std::shared_ptr<bool> m_alive;
    QList<QPointer<QObject>> m_retired;
};

}

std::unique_ptr<Network> createNetwork()
{
    return std::make_unique<QtNetwork>();
}

}
}
