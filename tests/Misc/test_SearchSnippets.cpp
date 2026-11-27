/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QtTest>

#include "test_SearchSnippets.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/SearchSnippets.h"

using namespace Imap::Mailbox;

namespace {

const QChar ellipsis(0x2026);

QString marked(const SearchSnippet &snippet)
{
    QStringList parts;
    for (const auto &match : snippet.matches)
        parts << snippet.text.mid(match.first, match.second);
    return parts.join(QLatin1Char('|'));
}

}

void SearchSnippetsTest::cutsAroundTheFirstMatch()
{
    QString text;
    for (int i = 0; i < 40; ++i)
        text += QStringLiteral("filler%1 ").arg(i);
    text += QStringLiteral("the kestrel landed ");
    for (int i = 40; i < 80; ++i)
        text += QStringLiteral("filler%1 ").arg(i);

    const SearchSnippet snippet = SearchSnippets::make(text, {QStringLiteral("kestrel")}, 60);
    QVERIFY(snippet.text.startsWith(ellipsis));
    QVERIFY(snippet.text.endsWith(ellipsis));
    // Whole words, and the match about a third in
    QVERIFY(snippet.text.at(1) == QLatin1Char('f'));
    QVERIFY(!snippet.text.contains(QLatin1String("  ")));
    QCOMPARE(snippet.matches.size(), 1);
    QCOMPARE(marked(snippet), QStringLiteral("kestrel"));
    QVERIFY(snippet.matches.constFirst().first > 5);
    QVERIFY(snippet.matches.constFirst().first < 30);
    QVERIFY(snippet.text.size() <= 62);
}

void SearchSnippetsTest::keepsShortTextWhole()
{
    const SearchSnippet snippet =
        SearchSnippets::make(QStringLiteral("  Noon at the\n caf\u00E9 "), {QStringLiteral("caf\u00E9")});
    QCOMPARE(snippet.text, QStringLiteral("Noon at the caf\u00E9"));
    QCOMPARE(snippet.matches, (QList<std::pair<int, int>>{{12, 4}}));
}

void SearchSnippetsTest::marksEveryMatchCaseInsensitively()
{
    const SearchSnippet snippet = SearchSnippets::make(QStringLiteral("Budget talk: the BUDGET and the budget."),
                                                       {QStringLiteral("budget"), QStringLiteral("talk")});
    QCOMPARE(marked(snippet), QStringLiteral("Budget|talk|BUDGET|budget"));
}

void SearchSnippetsTest::fallsBackToTheWordsOfAPhrase()
{
    // The phrase as a whole where it is found
    SearchSnippet snippet =
        SearchSnippets::make(QStringLiteral("we meet at noon today"), {QStringLiteral("at noon")});
    QCOMPARE(marked(snippet), QStringLiteral("at noon"));
    // Otherwise its words
    snippet = SearchSnippets::make(QStringLiteral("noon is when we meet at the cafe"), {QStringLiteral("at noon")});
    QCOMPARE(marked(snippet), QStringLiteral("noon|at"));
}

void SearchSnippetsTest::mergesOverlappingMatches()
{
    const SearchSnippet snippet = SearchSnippets::make(QStringLiteral("a reconciliation report"),
                                                       {QStringLiteral("reconcil"), QStringLiteral("concilia")});
    QCOMPARE(snippet.matches, (QList<std::pair<int, int>>{{2, 10}}));
}

void SearchSnippetsTest::startsAtTheTextWithoutAMatch()
{
    const QString text = QStringLiteral("Found by its subject. ").repeated(20);
    const SearchSnippet snippet = SearchSnippets::make(text, {QStringLiteral("absent")}, 50);
    QVERIFY(snippet.text.startsWith(QLatin1String("Found by")));
    QVERIFY(snippet.text.endsWith(ellipsis));
    QVERIFY(snippet.matches.isEmpty());
}

void SearchSnippetsTest::emptyText()
{
    const SearchSnippet snippet = SearchSnippets::make(QStringLiteral(" \n "), {QStringLiteral("x")});
    QVERIFY(snippet.text.isEmpty());
    QVERIFY(snippet.matches.isEmpty());
}

void SearchSnippetsTest::snippetsOfMessages()
{
    // An account-like tree: a folder, its message list and its messages
    QStandardItemModel model;
    auto *folder = new QStandardItem(QStringLiteral("INBOX"));
    folder->setData(QStringLiteral("INBOX"), RoleMailboxName);
    auto *list = new QStandardItem(QStringLiteral("[messages]"));
    folder->appendRow(list);
    model.appendRow(folder);
    for (uint uid : {1u, 2u}) {
        auto *message = new QStandardItem(QStringLiteral("message %1").arg(uid));
        message->setData(uid, RoleMessageUid);
        list->appendRow(message);
    }
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    const QModelIndex first = proxy.index(0, 0, proxy.index(0, 0, proxy.index(0, 0)));
    const QModelIndex second = first.siblingAtRow(1);

    SearchSnippets snippets;
    QVERIFY(!snippets.isActive());
    QVERIFY(!snippets.snippet(first).isValid());
    int asked = 0;
    snippets.setTextSource([&asked](const QString &mailbox, uint uid) {
        ++asked;
        return mailbox == QLatin1String("INBOX") && uid == 1 ? QStringLiteral("Lunch at noon") : QString();
    });
    QVERIFY(!snippets.isActive());
    QSignalSpy changed(&snippets, &SearchSnippets::changed);
    snippets.setTerms({QStringLiteral("noon")});
    QCOMPARE(changed.count(), 1);
    QVERIFY(snippets.isActive());

    const auto snippet = snippets.snippet(first).value<SearchSnippet>();
    QCOMPARE(snippet.text, QStringLiteral("Lunch at noon"));
    QCOMPARE(marked(snippet), QStringLiteral("noon"));
    // Kept until the terms change
    snippets.snippet(first);
    QCOMPARE(asked, 1);
    // A message whose text was not downloaded has an empty snippet; the folder is not a message
    QVERIFY(snippets.snippet(second).value<SearchSnippet>().text.isEmpty());
    QVERIFY(!snippets.snippet(proxy.index(0, 0)).isValid());

    snippets.setTerms({QStringLiteral("lunch")});
    QCOMPARE(marked(snippets.snippet(first).value<SearchSnippet>()), QStringLiteral("Lunch"));
    QCOMPARE(asked, 3);
    snippets.setTerms({});
    QVERIFY(!snippets.snippet(first).isValid());
}

QTEST_GUILESS_MAIN(SearchSnippetsTest)
