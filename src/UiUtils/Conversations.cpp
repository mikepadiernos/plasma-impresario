/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Conversations.h"

#include <QHash>
#include <QModelIndex>
#include <QRegularExpression>
#include <QVariant>
#include <algorithm>
#include <numeric>

#include "Imap/Model/ItemRoles.h"
#include "RelatedMessages.h"

namespace UiUtils {

namespace {

/** One reply or forward prefix, optionally counted, in English and other common
 * mail client languages */
const QRegularExpression &replyPrefix() {
  static const QRegularExpression prefix(
      QStringLiteral(
          "^\\s*(re|fw|fwd|aw|wg|sv|vs|vb|antw|doorst|tr|rif|r|res|enc|rv|odp|"
          "pd|ynt|ilt|vl|vá|"
          "回复|回覆|答复|转发|轉寄|회신|전달|отв|пересл|απ|σχετ|πρθ)"
          "\\s*(\\[\\d+\\]|\\(\\d+\\))?\\s*[:：]\\s*"),
      QRegularExpression::CaseInsensitiveOption);
  return prefix;
}

/** A mailing list tag such as "[dev]" */
const QRegularExpression &listTag() {
  static const QRegularExpression tag(
      QStringLiteral("^\\s*\\[[^\\[\\]]*\\]\\s*"));
  return tag;
}

/** Minimal union-find whose representative is chosen by @arg better */
class Groups {
public:
  Groups(qsizetype count, std::function<bool(qsizetype, qsizetype)> better)
      : m_parent(count), m_better(std::move(better)) {
    std::iota(m_parent.begin(), m_parent.end(), qsizetype(0));
  }

  qsizetype find(qsizetype at) {
    while (m_parent.at(at) != at) {
      m_parent[at] = m_parent.at(m_parent.at(at));
      at = m_parent.at(at);
    }
    return at;
  }

  void unite(qsizetype a, qsizetype b) {
    a = find(a);
    b = find(b);
    if (a == b)
      return;
    if (m_better(a, b))
      m_parent[b] = a;
    else
      m_parent[a] = b;
  }

private:
  QList<qsizetype> m_parent;
  std::function<bool(qsizetype, qsizetype)> m_better;
};

/** Strict weak order which puts messages without a date first */
bool earlier(const QDateTime &a, const QDateTime &b) {
  if (!a.isValid() || !b.isValid())
    return !a.isValid() && b.isValid();
  return a < b;
}

} // namespace

QString strippedSubject(const QString &subject, bool alsoListTags) {
  QString result = subject;
  for (;;) {
    const qsizetype before = result.size();
    result.remove(replyPrefix());
    if (alsoListTags)
      result.remove(listTag());
    if (result.size() == before)
      break;
  }
  return result.simplified();
}

bool hasReplyPrefix(const QString &subject) {
  QString rest = subject;
  for (;;) {
    const qsizetype before = rest.size();
    rest.remove(listTag());
    if (rest.size() == before)
      break;
  }
  return replyPrefix().match(rest).hasMatch();
}

ConversationMessage conversationMessage(const QModelIndex &index) {
  return ConversationMessage{
      index.data(Imap::Mailbox::RoleMessageSubject).toString(),
      index.data(Imap::Mailbox::RoleMessageDate).toDateTime(),
      index.data(Imap::Mailbox::RoleMessageMessageId).toByteArray(),
      index.data(Imap::Mailbox::RoleMessageInReplyTo)
          .value<QList<QByteArray>>(),
      index.data(Imap::Mailbox::RoleMessageHeaderReferences)
          .value<QList<QByteArray>>(),
  };
}

QList<qsizetype> groupConversations(const QList<ConversationMessage> &messages,
                                    int windowDays) {
  const qsizetype count = messages.size();
  Groups groups(count, [](qsizetype a, qsizetype b) { return a < b; });

  // Messages which mention one Message-ID, as their own, as their parent or
  // among their references, belong together
  QHash<QByteArray, qsizetype> byId;
  const auto link = [&](const QByteArray &id, qsizetype at) {
    if (id.isEmpty())
      return;
    const auto known = byId.constFind(id);
    if (known == byId.constEnd())
      byId.insert(id, at);
    else
      groups.unite(*known, at);
  };
  QHash<QString, QList<qsizetype>> bySubject;
  for (qsizetype at = 0; at < count; ++at) {
    const ConversationMessage &message = messages.at(at);
    link(message.messageId, at);
    for (const QByteArray &id : message.inReplyTo)
      link(id, at);
    for (const QByteArray &id : message.references)
      link(id, at);
    const QString subject = normalizedSubject(message.subject);
    if (!subject.isEmpty())
      bySubject[subject].append(at);
  }

  // A reply that carries no link joins the closest message of its subject,
  // preferring an earlier one
  const qint64 window = qint64(windowDays) * 24 * 60 * 60;
  const auto near = [&](qsizetype a, qsizetype b) {
    const QDateTime &first = messages.at(a).date;
    const QDateTime &second = messages.at(b).date;
    return !first.isValid() || !second.isValid() ||
           qAbs(first.secsTo(second)) <= window;
  };
  for (auto it = bySubject.begin(); it != bySubject.end(); ++it) {
    QList<qsizetype> &members = it.value();
    if (members.size() < 2)
      continue;
    std::stable_sort(members.begin(), members.end(),
                     [&messages](qsizetype a, qsizetype b) {
                       return earlier(messages.at(a).date, messages.at(b).date);
                     });
    for (qsizetype i = 0; i < members.size(); ++i) {
      const qsizetype at = members.at(i);
      const ConversationMessage &message = messages.at(at);
      if (!message.inReplyTo.isEmpty() || !message.references.isEmpty() ||
          !hasReplyPrefix(message.subject))
        continue;
      if (i > 0 && near(at, members.at(i - 1)))
        groups.unite(at, members.at(i - 1));
      else if (i + 1 < members.size() && near(at, members.at(i + 1)))
        groups.unite(at, members.at(i + 1));
    }
  }

  QList<qsizetype> result(count);
  for (qsizetype at = 0; at < count; ++at)
    result[at] = groups.find(at);
  return result;
}

QVector<Imap::Responses::ThreadingNode> mergeConversationThreads(
    const QVector<Imap::Responses::ThreadingNode> &threads,
    const std::function<std::optional<ConversationMessage>(uint uid)> &describe,
    int windowDays) {
  using Imap::Responses::ThreadingNode;
  QList<ConversationMessage> messages;
  QList<qsizetype> threadOf;
  QList<QDateTime> firstDate(threads.size());
  const std::function<void(const ThreadingNode &, qsizetype)> gather =
      [&](const ThreadingNode &node, qsizetype thread) {
        if (node.num) {
          if (const std::optional<ConversationMessage> message =
                  describe(node.num)) {
            if (message->date.isValid() &&
                (!firstDate.at(thread).isValid() ||
                 message->date < firstDate.at(thread)))
              firstDate[thread] = message->date;
            messages.append(*message);
            threadOf.append(thread);
          }
        }
        for (const ThreadingNode &child : node.children)
          gather(child, thread);
      };
  for (qsizetype thread = 0; thread < threads.size(); ++thread)
    gather(threads.at(thread), thread);
  if (messages.size() < 2)
    return threads;

  // The thread which starts first takes in the others; threads without a known
  // date count as starting last
  const auto startsFirst = [&firstDate](qsizetype a, qsizetype b) {
    const QDateTime &first = firstDate.at(a);
    const QDateTime &second = firstDate.at(b);
    if (first.isValid() != second.isValid())
      return first.isValid();
    if (first.isValid() && first != second)
      return first < second;
    return a < b;
  };
  Groups merged(threads.size(), startsFirst);
  const QList<qsizetype> conversations =
      groupConversations(messages, windowDays);
  QHash<qsizetype, qsizetype> threadOfConversation;
  for (qsizetype at = 0; at < messages.size(); ++at) {
    const auto known = threadOfConversation.constFind(conversations.at(at));
    if (known == threadOfConversation.constEnd())
      threadOfConversation.insert(conversations.at(at), threadOf.at(at));
    else
      merged.unite(*known, threadOf.at(at));
  }

  QHash<qsizetype, QList<qsizetype>> joining;
  for (qsizetype thread = 0; thread < threads.size(); ++thread) {
    const qsizetype into = merged.find(thread);
    if (into != thread)
      joining[into].append(thread);
  }
  if (joining.isEmpty())
    return threads;

  QVector<ThreadingNode> result;
  result.reserve(threads.size());
  for (qsizetype thread = 0; thread < threads.size(); ++thread) {
    if (merged.find(thread) != thread)
      continue;
    ThreadingNode node = threads.at(thread);
    QList<qsizetype> others = joining.value(thread);
    std::sort(others.begin(), others.end(), startsFirst);
    for (const qsizetype other : std::as_const(others))
      node.children.append(threads.at(other));
    result.append(node);
  }
  return result;
}

} // namespace UiUtils
