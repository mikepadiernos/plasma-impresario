/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/Relevance.h"

#include "Imap/Model/ItemRoles.h"

#include <QHash>
#include <QModelIndex>
#include <QSet>
#include <QVariant>

namespace UiUtils {

namespace {

/** @short Split a raw IMAP search into keys, quoted strings and parentheses */
QStringList tokenize(const QString &text) {
  QStringList tokens;
  QString current;
  bool quoted = false;
  bool inToken = false;
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (quoted) {
      if (c == QLatin1Char('\\') && i + 1 < text.size()) {
        current += text.at(++i);
      } else if (c == QLatin1Char('"')) {
        quoted = false;
      } else {
        current += c;
      }
      continue;
    }
    if (c == QLatin1Char('"')) {
      quoted = true;
      inToken = true;
    } else if (c.isSpace() || c == QLatin1Char('(') || c == QLatin1Char(')')) {
      if (inToken)
        tokens << current;
      current.clear();
      inToken = false;
      if (!c.isSpace())
        tokens << QString(c);
    } else {
      current += c;
      inToken = true;
    }
  }
  if (inToken)
    tokens << current;
  return tokens;
}

/** @short "name mailbox@host" of each address in a RoleMessageFrom-style list
 */
QString addressText(const QVariant &addresses) {
  QStringList parts;
  const QVariantList list = addresses.toList();
  for (const QVariant &address : list) {
    const QStringList fields = address.toStringList();
    if (fields.size() < 4)
      continue;
    parts << fields.at(0) << fields.at(2) + QLatin1Char('@') + fields.at(3);
  }
  return parts.join(QLatin1Char(' ')).toCaseFolded();
}

int fieldScore(const QString &field, const QString &term,
               const QStringList &words, int weight) {
  if (field.isEmpty())
    return 0;
  if (field.contains(term))
    return weight * (static_cast<int>(words.size()) + 1);
  if (words.size() < 2)
    return 0;
  int score = 0;
  for (const QString &word : words) {
    if (field.contains(word))
      score += weight;
  }
  return score;
}

/** @short A search key with text, such as SUBJECT, and its arguments */
struct TextKey {
  QString key;
  QStringList arguments;
};

/** @short How many arguments follow @arg key, for keys without text */
int argumentCount(const QString &key) {
  static const QHash<QString, int> counts = {
      {QStringLiteral("BEFORE"), 1},      {QStringLiteral("ON"), 1},
      {QStringLiteral("SINCE"), 1},       {QStringLiteral("SENTBEFORE"), 1},
      {QStringLiteral("SENTON"), 1},      {QStringLiteral("SENTSINCE"), 1},
      {QStringLiteral("LARGER"), 1},      {QStringLiteral("SMALLER"), 1},
      {QStringLiteral("KEYWORD"), 1},     {QStringLiteral("UNKEYWORD"), 1},
      {QStringLiteral("UID"), 1},         {QStringLiteral("OLDER"), 1},
      {QStringLiteral("YOUNGER"), 1},     {QStringLiteral("SAVEDBEFORE"), 1},
      {QStringLiteral("SAVEDON"), 1},     {QStringLiteral("SAVEDSINCE"), 1},
      {QStringLiteral("EMAILID"), 1},     {QStringLiteral("THREADID"), 1},
      {QStringLiteral("FILTER"), 1},      {QStringLiteral("X-GM-RAW"), 1},
      {QStringLiteral("X-GM-MSGID"), 1},  {QStringLiteral("X-GM-THRID"), 1},
      {QStringLiteral("X-GM-LABELS"), 1}, {QStringLiteral("ANNOTATION"), 3},
  };
  return counts.value(key);
}

/** @short Collect the text keys of the search key at @arg i which a match has
to contain, moving @arg i past it

A key under an odd number of NOTs excludes its text, so it is skipped. */
void collectTextKeys(const QStringList &tokens, qsizetype &i, bool negated,
                     QList<TextKey> &keys) {
  static const QStringList oneArgument{
      QStringLiteral("SUBJECT"), QStringLiteral("FROM"), QStringLiteral("TO"),
      QStringLiteral("CC"),      QStringLiteral("BCC"),  QStringLiteral("BODY"),
      QStringLiteral("TEXT"),
  };
  if (i >= tokens.size())
    return;
  const QString key = tokens.at(i++).toUpper();
  if (key == QLatin1String("NOT")) {
    collectTextKeys(tokens, i, !negated, keys);
  } else if (key == QLatin1String("OR")) {
    collectTextKeys(tokens, i, negated, keys);
    collectTextKeys(tokens, i, negated, keys);
  } else if (key == QLatin1String("FUZZY")) {
    collectTextKeys(tokens, i, negated, keys);
  } else if (key == QLatin1String("(")) {
    while (i < tokens.size() && tokens.at(i) != QLatin1String(")"))
      collectTextKeys(tokens, i, negated, keys);
    ++i;
  } else if (oneArgument.contains(key) || key == QLatin1String("HEADER")) {
    const qsizetype count = key == QLatin1String("HEADER") ? 2 : 1;
    if (!negated && i + count <= tokens.size())
      keys.append({key, tokens.mid(i, count)});
    i += count;
  } else {
    i += argumentCount(key);
  }
}

/** @short The text keys which messages found by @arg conditions contain */
QList<TextKey> textKeys(const QStringList &conditions) {
  const QStringList tokens =
      conditions.size() == 1 ? tokenize(conditions.constFirst()) : conditions;
  QList<TextKey> keys;
  qsizetype i = 0;
  while (i < tokens.size())
    collectTextKeys(tokens, i, false, keys);
  return keys;
}

} // namespace

QString searchHighlightTerm(const QStringList &conditions) {
  static const QStringList bodyKeys{
      QStringLiteral("SUBJECT"),
      QStringLiteral("BODY"),
      QStringLiteral("TEXT"),
  };
  const QList<TextKey> keys = textKeys(conditions);
  for (const TextKey &key : keys) {
    if (!bodyKeys.contains(key.key))
      continue;
    const QString term = key.arguments.constFirst().simplified();
    if (!term.isEmpty())
      return term;
  }
  return QString();
}

QStringList relevanceTerms(const QStringList &conditions) {
  QStringList terms;
  QSet<QString> seen;
  auto add = [&](const QString &value) {
    const QString term = value.simplified().toCaseFolded();
    if (term.isEmpty() || seen.contains(term))
      return;
    seen.insert(term);
    terms << term;
  };
  const QList<TextKey> keys = textKeys(conditions);
  for (const TextKey &key : keys) {
    if (key.key != QLatin1String("HEADER")) {
      add(key.arguments.constFirst());
    } else if (key.arguments.constFirst().compare(
                   QLatin1String("Content-Type"), Qt::CaseInsensitive) != 0) {
      // HEADER field-name value; has:attachment's Content-Type is no text
      add(key.arguments.at(1));
    }
  }
  return terms;
}

int relevanceScore(const QModelIndex &message, const QStringList &terms) {
  using namespace Imap::Mailbox;
  if (!message.isValid())
    return -2;
  const QVariant fetched = message.data(RoleIsFetched);
  if (!fetched.isValid() || message.data(RoleMessageUid).toUInt() == 0)
    return -2;
  // Asking for the envelope of a message which is not loaded would download it
  if (!fetched.toBool())
    return -1;

  const QString subject =
      message.data(RoleMessageSubject).toString().toCaseFolded();
  const QString senders = addressText(message.data(RoleMessageFrom));
  const QString recipients =
      addressText(message.data(RoleMessageTo)) + QLatin1Char(' ') +
      addressText(message.data(RoleMessageCc));
  int score = 0;
  for (const QString &term : terms) {
    const QStringList words = term.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    score += fieldScore(subject, term, words, 4);
    score += fieldScore(senders, term, words, 2);
    score += fieldScore(recipients.trimmed(), term, words, 1);
  }
  return score;
}

} // namespace UiUtils
