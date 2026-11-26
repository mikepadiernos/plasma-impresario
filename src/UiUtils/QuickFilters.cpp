/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "QuickFilters.h"

#include <QAbstractItemModel>
#include <QRegularExpression>
#include <QUrl>
#include <QVariant>

#include "Imap/Model/ItemRoles.h"
#include "Conversations.h"
#include "RelatedMessages.h"

namespace UiUtils {

namespace {

const QLatin1Char quote('"');

QString keyword(FieldFilter field) {
  switch (field) {
  case FieldFilter::From:
    return QStringLiteral("from");
  case FieldFilter::To:
    return QStringLiteral("to");
  case FieldFilter::Subject:
    return QStringLiteral("subject");
  case FieldFilter::MailingList:
    return QStringLiteral("list");
  case FieldFilter::None:
    break;
  }
  return QString();
}

bool isFetched(const QModelIndex &index) {
  return index.data(Imap::Mailbox::RoleIsFetched).toBool();
}

bool isMessage(const QModelIndex &index) {
  return index.data(Imap::Mailbox::RoleMessageUid).toUInt() != 0;
}

QModelIndex lastShownDescendant(
    const QAbstractItemModel *model, QModelIndex index,
    const std::function<bool(const QModelIndex &)> &isExpanded) {
  while (index.isValid() && isExpanded(index)) {
    const int rows = model->rowCount(index);
    if (rows == 0)
      break;
    index = model->index(rows - 1, 0, index);
  }
  return index;
}

QModelIndex
shownRowAfter(const QAbstractItemModel *model, const QModelIndex &index,
              const std::function<bool(const QModelIndex &)> &isExpanded) {
  if (isExpanded(index) && model->rowCount(index) > 0)
    return model->index(0, 0, index);
  for (QModelIndex item = index; item.isValid(); item = item.parent()) {
    if (item.row() + 1 < model->rowCount(item.parent()))
      return model->index(item.row() + 1, 0, item.parent());
  }
  return QModelIndex();
}

QModelIndex
shownRowBefore(const QAbstractItemModel *model, const QModelIndex &index,
               const std::function<bool(const QModelIndex &)> &isExpanded) {
  if (index.row() > 0)
    return lastShownDescendant(
        model, model->index(index.row() - 1, 0, index.parent()), isExpanded);
  return index.parent();
}

} // namespace

QString searchableSubject(const QString &subject) {
  const QString result = strippedSubject(subject, true);
  return result.isEmpty() ? strippedSubject(subject, false) : result;
}

QString fieldFilterText(FieldFilter field, const QString &value) {
  if (field == FieldFilter::None || value.isEmpty())
    return value;
  const bool needsQuotes = value.front().isSpace() || value.back().isSpace() ||
                           value.front() == quote || value.back() == quote;
  return keyword(field) + QLatin1Char(':') +
         (needsQuotes ? quote + value + quote : value);
}

ParsedFieldFilter parseFieldFilter(const QString &text) {
  static const QRegularExpression filter(
      QStringLiteral("^\\s*(from|to|subject|list):(.*)$"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  ParsedFieldFilter result;
  const auto match = filter.match(text);
  if (!match.hasMatch())
    return result;
  QString value = match.captured(2).trimmed();
  if (value.size() >= 2 && value.front() == quote && value.back() == quote)
    value = value.mid(1, value.size() - 2);
  if (value.isEmpty())
    return result;

  const QString name = match.captured(1).toLower();
  for (const FieldFilter field :
       {FieldFilter::From, FieldFilter::To, FieldFilter::Subject,
        FieldFilter::MailingList}) {
    if (keyword(field) == name)
      result.field = field;
  }
  result.value = value;
  return result;
}

QStringList fieldFilterConditions(const ParsedFieldFilter &filter) {
  if (filter.value.isEmpty())
    return QStringList();
  switch (filter.field) {
  case FieldFilter::From:
    return {QStringLiteral("FROM"), filter.value};
  case FieldFilter::To:
    return {QStringLiteral("TO"), filter.value};
  case FieldFilter::Subject:
    return {QStringLiteral("SUBJECT"), filter.value};
  case FieldFilter::MailingList:
    // The header holds "<mailto:address>", and HEADER matches a substring of it
    return {QStringLiteral("HEADER"), QStringLiteral("List-Post"),
            filter.value};
  case FieldFilter::None:
    break;
  }
  return QStringList();
}

QString listPostAddress(const QVariant &listPost) {
  const QVariantList urls = listPost.toList();
  for (const QVariant &item : urls) {
    const QUrl url = item.toUrl();
    if (url.scheme().compare(QLatin1String("mailto"), Qt::CaseInsensitive) ==
            0 &&
        !url.path().isEmpty())
      return url.path().toCaseFolded();
  }
  return QString();
}

QString fieldFilterValue(const QModelIndex &message, FieldFilter field) {
  if (!message.isValid() || !isFetched(message))
    return QString();
  switch (field) {
  case FieldFilter::From:
    return firstAddress(message.data(Imap::Mailbox::RoleMessageFrom));
  case FieldFilter::To:
    return firstAddress(message.data(Imap::Mailbox::RoleMessageTo));
  case FieldFilter::Subject:
    return searchableSubject(
        message.data(Imap::Mailbox::RoleMessageSubject).toString());
  case FieldFilter::MailingList:
    return listPostAddress(
        message.data(Imap::Mailbox::RoleMessageHeaderListPost));
  case FieldFilter::None:
    break;
  }
  return QString();
}

QModelIndexList
findMatchingMessages(const QAbstractItemModel *model,
                     const std::function<bool(const QModelIndex &)> &matches) {
  QModelIndexList result;
  if (!model)
    return result;
  // Depth first, children right after their parent
  QModelIndexList pending;
  for (int row = model->rowCount() - 1; row >= 0; --row)
    pending.append(model->index(row, 0));
  while (!pending.isEmpty()) {
    const QModelIndex index = pending.takeLast();
    if (isMessage(index) && matches(index))
      result.append(index);
    for (int row = model->rowCount(index) - 1; row >= 0; --row)
      pending.append(model->index(row, 0, index));
  }
  return result;
}

QModelIndexList findSimilarMessages(const QAbstractItemModel *model,
                                    const QModelIndex &reference,
                                    SimilarMessages criterion) {
  switch (criterion) {
  case SimilarMessages::FromSender: {
    const QString sender = fieldFilterValue(reference, FieldFilter::From);
    if (sender.isEmpty())
      return QModelIndexList();
    return findMatchingMessages(model, [&sender](const QModelIndex &index) {
      return isFetched(index) &&
             firstAddress(index.data(Imap::Mailbox::RoleMessageFrom)) == sender;
    });
  }
  case SimilarMessages::WithSubject: {
    const QString subject =
        fieldFilterValue(reference, FieldFilter::Subject).toCaseFolded();
    if (subject.isEmpty())
      return QModelIndexList();
    return findMatchingMessages(model, [&subject](const QModelIndex &index) {
      return isFetched(index) &&
             searchableSubject(
                 index.data(Imap::Mailbox::RoleMessageSubject).toString())
                     .toCaseFolded() == subject;
    });
  }
  case SimilarMessages::Unread:
    return findMatchingMessages(model, [](const QModelIndex &index) {
      return !index.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool();
    });
  case SimilarMessages::Flagged:
    return findMatchingMessages(model, [](const QModelIndex &index) {
      return index.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool();
    });
  }
  return QModelIndexList();
}

QModelIndex
adjacentMessage(const QAbstractItemModel *model, const QModelIndex &current,
                bool forward,
                const std::function<bool(const QModelIndex &)> &isExpanded) {
  if (!model || model->rowCount() == 0)
    return QModelIndex();
  QModelIndex index;
  if (current.isValid())
    index = forward
                ? shownRowAfter(model, current.siblingAtColumn(0), isExpanded)
                : shownRowBefore(model, current.siblingAtColumn(0), isExpanded);
  else
    index =
        forward
            ? model->index(0, 0)
            : lastShownDescendant(model, model->index(model->rowCount() - 1, 0),
                                  isExpanded);
  while (index.isValid() && !isMessage(index))
    index = forward ? shownRowAfter(model, index, isExpanded)
                    : shownRowBefore(model, index, isExpanded);
  return index;
}

} // namespace UiUtils
