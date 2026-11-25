/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/Relevance.h"

#include "Imap/Model/ItemRoles.h"

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

} // namespace

QString searchHighlightTerm(const QStringList &conditions) {
  const QStringList tokens =
      conditions.size() == 1 ? tokenize(conditions.constFirst()) : conditions;
  static const QStringList bodyKeys{
      QStringLiteral("SUBJECT"),
      QStringLiteral("BODY"),
      QStringLiteral("TEXT"),
  };
  static const QStringList headerKeys{
      QStringLiteral("FROM"),
      QStringLiteral("TO"),
      QStringLiteral("CC"),
      QStringLiteral("BCC"),
  };
  for (int i = 0; i < tokens.size(); ++i) {
    const QString key = tokens.at(i).toUpper();
    if (bodyKeys.contains(key)) {
      if (i + 1 < tokens.size()) {
        const QString term = tokens.at(++i).simplified();
        if (!term.isEmpty())
          return term;
      }
    } else if (headerKeys.contains(key)) {
      ++i;
    } else if (key == QLatin1String("HEADER")) {
      i += 2;
    }
  }
  return QString();
}

QStringList relevanceTerms(const QStringList &conditions) {
  const QStringList tokens =
      conditions.size() == 1 ? tokenize(conditions.constFirst()) : conditions;
  QStringList terms;
  QSet<QString> seen;
  auto add = [&](const QString &value) {
    const QString term = value.simplified().toCaseFolded();
    if (term.isEmpty() || seen.contains(term))
      return;
    seen.insert(term);
    terms << term;
  };
  static const QStringList valueKeys{
      QStringLiteral("SUBJECT"), QStringLiteral("FROM"), QStringLiteral("TO"),
      QStringLiteral("CC"),      QStringLiteral("BCC"),  QStringLiteral("BODY"),
      QStringLiteral("TEXT"),
  };
  for (int i = 0; i < tokens.size(); ++i) {
    const QString key = tokens.at(i).toUpper();
    if (valueKeys.contains(key)) {
      if (i + 1 < tokens.size())
        add(tokens.at(++i));
    } else if (key == QLatin1String("HEADER")) {
      // HEADER field-name value
      if (i + 2 < tokens.size())
        add(tokens.at(i + 2));
      i += 2;
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
