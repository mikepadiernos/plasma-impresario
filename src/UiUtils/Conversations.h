/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVector>
#include <functional>
#include <optional>

#include "Imap/Parser/ThreadingNode.h"

class QModelIndex;

namespace UiUtils {

/** @short @arg subject without its leading reply and forward prefixes and, with
@arg alsoListTags, leading list tags

Prefixes may be localized ("AW:", "SV:", "回复:"), counted ("Re[2]:", "Re(3):")
and repeated, and list tags such as
"[dev]" may come before or between them. The result is simplified but keeps its
case.
*/
QString strippedSubject(const QString &subject, bool alsoListTags);

/** @short Whether @arg subject starts with a reply or forward prefix, possibly
 * after list tags */
bool hasReplyPrefix(const QString &subject);

/** @short What decides which conversation a message belongs to */
struct ConversationMessage {
  QString subject;
  QDateTime date;
  QByteArray messageId;
  QList<QByteArray> inReplyTo;
  QList<QByteArray> references;
};

/** @short The ConversationMessage of a message index of the IMAP model or of a
model above it

Read only fetched messages: the roles of any other message would make the IMAP
model download its envelope.
*/
ConversationMessage conversationMessage(const QModelIndex &index);

/** @short How far apart in time two messages may be to join a conversation by
 * their subject alone */
constexpr int conversationWindowDays = 30;

/** @short Group @arg messages into conversations

The result holds, for each message, the position of the first message of its
conversation in @arg messages.

Messages belong together when they share a Message-ID, whether as their own, in
In-Reply-To or in References, so a message which refers to two conversations
merges them. A message with neither In-Reply-To nor References, whose subject
starts with a reply or forward prefix, also joins the closest earlier message
with the same normalized subject (see normalizedSubject()), or else the closest
later one, when they are at most @arg windowDays apart. A message without a date
can join any message of its subject. Two messages without prefixes never join by
subject, so unrelated mail called "Hello" stays apart.
*/
QList<qsizetype> groupConversations(const QList<ConversationMessage> &messages,
                                    int windowDays = conversationWindowDays);

/** @short @arg threads, from a THREAD response, with the threads of one
conversation merged

@arg describe gives the ConversationMessage of a UID, or nothing when its
message is not loaded. Threads whose messages groupConversations() puts in one
conversation become one: the others are appended, as replies, to the top message
of the thread with the earliest message. A thread without any loaded message
stays as it is. The result is @arg threads itself when nothing is merged.
*/
QVector<Imap::Responses::ThreadingNode> mergeConversationThreads(
    const QVector<Imap::Responses::ThreadingNode> &threads,
    const std::function<std::optional<ConversationMessage>(uint uid)> &describe,
    int windowDays = conversationWindowDays);

} // namespace UiUtils
