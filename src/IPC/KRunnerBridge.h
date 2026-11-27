/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IPC_KRUNNERBRIDGE_H
#define IPC_KRUNNERBRIDGE_H

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

class QDBusArgument;

namespace IPC {

/** @short A match as Plasma Search (KRunner) reads it over D-Bus, signature (sssida{sv}) */
struct RunnerMatch {
    QString id;
    QString text;
    QString iconName;
    /** KRunner's category relevance, see KRunnerBridge::categoryRelevanceLow */
    int categoryRelevance = 0;
    qreal relevance = 0;
    /** Such as "subtext", the second line */
    QVariantMap properties;
};

/** @short An action offered on the matches, signature (sss) */
struct RunnerAction {
    QString id;
    QString text;
    QString iconName;
};

using RunnerMatches = QList<RunnerMatch>;
using RunnerActions = QList<RunnerAction>;

QDBusArgument &operator<<(QDBusArgument &argument, const RunnerMatch &match);
const QDBusArgument &operator>>(const QDBusArgument &argument, RunnerMatch &match);
QDBusArgument &operator<<(QDBusArgument &argument, const RunnerAction &action);
const QDBusArgument &operator>>(const QDBusArgument &argument, RunnerAction &action);

/** @short The org.kde.krunner1 interface, through which Plasma Search asks the application for matches

KRunner finds the object through the plugin metadata installed in share/krunner/dbusplugins, calls Match() as the user
types and Run() with the ID of the chosen match. On Wayland, KRunner calls SetActivationToken() just before Run() so
that the window showing the match can come to the front. Finding and opening the matches and keeping the token are left
to the functions given, so that this class knows nothing of the windows.
*/
class KRunnerBridge : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")

public:
    using Search = std::function<RunnerMatches(const QString &query)>;
    using Open = std::function<void(const QString &matchId)>;
    using HandOverToken = std::function<void(const QString &token)>;

    /** KRunner's CategoryRelevance::Low, formerly PossibleMatch, for matches which are suggestions */
    static constexpr int categoryRelevanceLow = 30;

    KRunnerBridge(Search search, Open open, HandOverToken handOverToken = {}, QObject *parent = nullptr);

    /** The object path which the plugin metadata names */
    static QString objectPath();
    /** Register the D-Bus types; needed before the object is exported */
    static void registerTypes();

public slots:
    IPC::RunnerActions Actions();
    IPC::RunnerMatches Match(const QString &query);
    /** The XDG activation token for the Run() call which follows */
    void SetActivationToken(const QString &token);
    void Run(const QString &matchId, const QString &actionId);
    /** Called when a search session ends; nothing is kept between queries */
    void Teardown();

private:
    Search m_search;
    Open m_open;
    HandOverToken m_handOverToken;
};

}

Q_DECLARE_METATYPE(IPC::RunnerMatch)
Q_DECLARE_METATYPE(IPC::RunnerAction)

#endif
