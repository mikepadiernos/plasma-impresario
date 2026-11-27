/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>
#include <memory>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "UiUtils/ServerDiscovery.h"

namespace Tests {

/** @short A scripted UiUtils::ServerDiscovery::Network which answers from tables, from the event loop

Pages are keyed by host and path, without the query. Unknown pages, names and hosts fail. Every request is logged as
"GET host/path", "SRV name", "MX domain" or "PROBE host:port". With hold set, answers wait for release().
*/
class FakeDiscoveryNetwork : public UiUtils::ServerDiscovery::Network
{
public:
    struct State {
        QHash<QString, QByteArray> pages;
        QHash<QString, QList<SrvRecord>> srv;
        QSet<QString> authenticatedNames;
        QHash<QString, QStringList> mx;
        QSet<QString> reachable;
        QStringList log;
        QList<QUrl> urls;
        int aborts = 0;
        /** Whether abort() drops the answers of earlier requests, as the real network does */
        bool dropOnAbort = true;
        bool hold = false;
        QList<std::function<void()>> held;
        quint64 generation = 0;

        void release()
        {
            hold = false;
            const auto answers = held;
            held.clear();
            for (const auto &answer : answers)
                answer();
        }
    };

    explicit FakeDiscoveryNetwork(std::shared_ptr<State> state)
        : m_state(std::move(state))
    {
    }

    static QString pageKey(const QUrl &url) { return url.host() + url.path(); }

    void fetch(const QUrl &url, FetchDone done) override
    {
        const QString key = pageKey(url);
        m_state->log << QStringLiteral("GET ") + key;
        m_state->urls << url;
        const bool found = m_state->pages.contains(key);
        const QByteArray body = m_state->pages.value(key);
        answer([done, found, body]() { done(found, found ? body : QByteArray()); });
    }

    void lookupSrv(const QString &name, SrvDone done) override
    {
        m_state->log << QStringLiteral("SRV ") + name;
        const auto records = m_state->srv.value(name);
        const bool authenticated = m_state->authenticatedNames.contains(name);
        answer([done, records, authenticated]() { done(records, authenticated); });
    }

    void lookupMx(const QString &domain, MxDone done) override
    {
        m_state->log << QStringLiteral("MX ") + domain;
        const QStringList hosts = m_state->mx.value(domain);
        const bool authenticated = m_state->authenticatedNames.contains(domain);
        answer([done, hosts, authenticated]() { done(hosts, authenticated); });
    }

    void probe(const QString &host, quint16 port, UiUtils::ServerDiscovery::Security, ProbeDone done) override
    {
        const QString key = host + QLatin1Char(':') + QString::number(port);
        m_state->log << QStringLiteral("PROBE ") + key;
        const bool reachable = m_state->reachable.contains(key);
        answer([done, reachable]() { done(reachable); });
    }

    void abort() override
    {
        ++m_state->aborts;
        if (m_state->dropOnAbort)
            ++m_state->generation;
    }

private:
    void answer(std::function<void()> callback)
    {
        const std::shared_ptr<State> state = m_state;
        const quint64 generation = state->generation;
        auto guarded = [state, generation, callback]() {
            if (generation == state->generation)
                callback();
        };
        if (state->hold)
            state->held << guarded;
        else
            QTimer::singleShot(0, guarded);
    }

    std::shared_ptr<State> m_state;
};

}
