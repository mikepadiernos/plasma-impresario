/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "KRunnerBridge.h"

#include <QDBusArgument>
#include <QDBusMetaType>

namespace IPC {

QDBusArgument &operator<<(QDBusArgument &argument, const RunnerMatch &match)
{
    argument.beginStructure();
    argument << match.id << match.text << match.iconName << match.categoryRelevance << double(match.relevance)
             << match.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RunnerMatch &match)
{
    double relevance = 0;
    argument.beginStructure();
    argument >> match.id >> match.text >> match.iconName >> match.categoryRelevance >> relevance >> match.properties;
    argument.endStructure();
    match.relevance = relevance;
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const RunnerAction &action)
{
    argument.beginStructure();
    argument << action.id << action.text << action.iconName;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RunnerAction &action)
{
    argument.beginStructure();
    argument >> action.id >> action.text >> action.iconName;
    argument.endStructure();
    return argument;
}

KRunnerBridge::KRunnerBridge(Search search, Open open, QObject *parent)
    : QObject(parent)
    , m_search(std::move(search))
    , m_open(std::move(open))
{
    registerTypes();
}

QString KRunnerBridge::objectPath()
{
    return QStringLiteral("/runner");
}

void KRunnerBridge::registerTypes()
{
    qDBusRegisterMetaType<RunnerMatch>();
    qDBusRegisterMetaType<RunnerMatches>();
    qDBusRegisterMetaType<RunnerAction>();
    qDBusRegisterMetaType<RunnerActions>();
}

RunnerActions KRunnerBridge::Actions()
{
    // Choosing a match opens it; there is nothing else to offer
    return {};
}

RunnerMatches KRunnerBridge::Match(const QString &query)
{
    return m_search ? m_search(query) : RunnerMatches();
}

void KRunnerBridge::Run(const QString &matchId, const QString &actionId)
{
    // The default action has an empty ID, and there are no others
    if (!actionId.isEmpty() || matchId.isEmpty() || !m_open)
        return;
    m_open(matchId);
}

void KRunnerBridge::Teardown()
{
}

}
