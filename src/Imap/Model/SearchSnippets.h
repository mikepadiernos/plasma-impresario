/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_SEARCHSNIPPETS_H
#define IMAP_MODEL_SEARCHSNIPPETS_H

#include <functional>
#include <utility>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QModelIndex;

namespace Imap
{
namespace Mailbox
{

class Model;

/** @short A short piece of the text of a message, with the matches of a search in it */
struct SearchSnippet {
    QString text;
    /** Start and length of each match in text, in order and without overlaps */
    QList<std::pair<int, int>> matches;
};

/** @short Snippets of the downloaded text of messages for the terms of a search

A snippet is cut from the text around the first match, or from the start of the text when the text has no match, as
for a message found by its subject. Only text which was downloaded is used: nothing is fetched for a snippet, so a
message whose text was never downloaded has none.

The snippets are kept until the terms change. With setModel(), a snippet is made again once a part of its message is
downloaded, and changed() is emitted.
*/
class SearchSnippets : public QObject
{
    Q_OBJECT
public:
    /** @short The downloaded text of the message @p uid in @p mailbox, or an empty string */
    using TextSource = std::function<QString(const QString &mailbox, uint uid)>;

    /** @short Length of a snippet in characters, not counting the marks of left-out text */
    static constexpr int defaultLength = 160;

    explicit SearchSnippets(QObject *parent = nullptr);

    /** @short Mark @p terms, case-folded words and phrases as from UiUtils::relevanceTerms(); none turn snippets off */
    void setTerms(const QStringList &terms);
    QStringList terms() const;
    /** @short Read the text of the messages through @p source */
    void setTextSource(const TextSource &source);
    /** @short Read the text which the offline cache of @p model holds, and follow the parts it downloads */
    void setModel(Model *model);
    /** @short Whether there are terms and a source of text */
    bool isActive() const;

    /** @short The snippet of the message @p message of a message list, or an invalid QVariant

    The message's UID and the name of its folder are what the snippet is made for; @p message may be an index of any
    proxy of the model. An invalid QVariant is returned while inactive and for rows which are not messages.
    */
    QVariant snippet(const QModelIndex &message) const;

    /** @short A snippet of @p text of about @p length characters around the first of @p terms which it contains */
    static SearchSnippet make(const QString &text, const QStringList &terms, int length = defaultLength);

signals:
    /** @short Snippets which were asked for may have changed */
    void changed();

private:
    void partChanged(const QModelIndex &topLeft);

    QStringList m_terms;
    TextSource m_source;
    QPointer<Model> m_model;
    QMetaObject::Connection m_watch;
    /** By folder and UID */
    mutable QHash<std::pair<QString, uint>, SearchSnippet> m_cache;
};

}
}

Q_DECLARE_METATYPE(Imap::Mailbox::SearchSnippet)

#endif
