/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

class KConfigBase;

namespace Imap {
namespace Mailbox {
class SearchIndex;
}
}

namespace UiUtils {

/** @short Answering Plasma Search (KRunner) from the offline search indexes

Off by default. Only accounts with an offline search index are searched, only by the subject and the sender of their
messages, and only the subject, the sender and the date are shown. The D-Bus side lives in IPC and the text shown in
the GUI; this namespace holds the setting, the search and the IDs which name the matches.
*/
namespace PlasmaSearch {

/** Whether Plasma Search may show mail, as stored in @arg config */
bool isEnabled(const KConfigBase &config);
void setEnabled(KConfigBase &config, bool enabled);

/** Words shorter than this are left out of a query, as they would match nearly everything */
constexpr int minimumWordLength = 3;
/** The most matches given for one query */
constexpr int maximumMatches = 10;

/** @short The words of @arg query which are searched for: at least minimumWordLength characters, each once */
QStringList words(const QString &query);

/** @short Where a match is */
struct Target {
    QString accountId;
    QString mailbox;
    uint uidValidity = 0;
    uint uid = 0;
};

/** @short The ID which Plasma Search hands back when a match is chosen */
QString matchId(const Target &target);
/** @short The message named by @arg id, or false when @arg id is not one of matchId() */
bool parseMatchId(const QString &id, Target *target);

/** @short An account to search, whose index is open */
struct Source {
    QString accountId;
    const Imap::Mailbox::SearchIndex *index = nullptr;
};

struct Match {
    QString id;
    QString subject;
    QString sender;
    QDate sent;
    /** Between 0 and 1, the newest highest */
    qreal relevance = 0;
    /** The text shown: the subject, and a line with the sender and the date; filled in by the GUI */
    QString title;
    QString subtext;
};

/** @short The newest messages of @arg sources whose subject or sender contains every word of @arg query */
QList<Match> matches(const QString &query, const QList<Source> &sources, int limit = maximumMatches);

}

}
