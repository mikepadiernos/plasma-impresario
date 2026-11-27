/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QtTest>

#include "test_SearchSuggestions.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/SearchSuggestions.h"

using UiUtils::SearchHistory;
using namespace UiUtils::SearchSuggestions;

namespace {

QStringList texts(const QList<Suggestion> &suggestions, Kind kind)
{
    QStringList result;
    for (const Suggestion &suggestion : suggestions) {
        if (suggestion.kind == kind)
            result << suggestion.text;
    }
    return result;
}

const QList<Contact> people = {
    {QStringLiteral("Ann Smith"), QStringLiteral("ann@example.org")},
    {QStringLiteral("Annette"), QStringLiteral("annette@example.org")},
    {QStringLiteral("Bob"), QStringLiteral("bob@example.org")},
};

}

void SearchSuggestionsTest::historyNormalizes()
{
    const SearchHistory history({QStringLiteral(" invoice "), QString(), QStringLiteral("invoice"), QStringLiteral("from:ann"),
                                 QStringLiteral("tag:work")},
                                2);
    QCOMPARE(history.entries(), (QStringList{QStringLiteral("invoice"), QStringLiteral("from:ann")}));
    QCOMPARE(history.limit(), 2);
    QCOMPARE(SearchHistory(QStringList(), 0).limit(), 1);
}

void SearchSuggestionsTest::historyAddMovesToFrontAndCaps()
{
    SearchHistory history({}, 3);
    QVERIFY(history.add(QStringLiteral("a")));
    QVERIFY(history.add(QStringLiteral("b")));
    QVERIFY(history.add(QStringLiteral("c")));
    QVERIFY(!history.add(QStringLiteral(" c ")));
    QVERIFY(!history.add(QStringLiteral("   ")));
    QVERIFY(history.add(QStringLiteral("a")));
    QCOMPARE(history.entries(), (QStringList{QStringLiteral("a"), QStringLiteral("c"), QStringLiteral("b")}));
    QVERIFY(history.add(QStringLiteral("d")));
    QCOMPARE(history.entries(), (QStringList{QStringLiteral("d"), QStringLiteral("a"), QStringLiteral("c")}));
    // Queries differing in case are different searches for raw IMAP queries, so both stay
    QVERIFY(history.add(QStringLiteral("D")));
    QCOMPARE(history.entries().first(), QStringLiteral("D"));
    history.clear();
    QVERIFY(history.entries().isEmpty());
}

void SearchSuggestionsTest::profileKey()
{
    QCOMPARE(SearchHistory::profileKey(QString()), QStringLiteral("default"));
    QCOMPARE(SearchHistory::profileKey(QStringLiteral("work")), QStringLiteral("work"));
}

void SearchSuggestionsTest::currentToken_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("start");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("value");

    QTest::newRow("empty") << QString() << -1 << QString() << QString() << QString();
    QTest::newRow("plain") << QStringLiteral("an") << 0 << QString() << QString() << QStringLiteral("an");
    QTest::newRow("last word") << QStringLiteral("invoice ann") << 8 << QString() << QString() << QStringLiteral("ann");
    QTest::newRow("after space") << QStringLiteral("invoice ") << -1 << QString() << QString() << QString();
    QTest::newRow("field") << QStringLiteral("From:an") << 0 << QString() << QStringLiteral("from") << QStringLiteral("an");
    QTest::newRow("empty tag") << QStringLiteral("x tag:") << 2 << QString() << QStringLiteral("tag") << QString();
    QTest::newRow("negated") << QStringLiteral("-to:bo") << 0 << QStringLiteral("-") << QStringLiteral("to") << QStringLiteral("bo");
    QTest::newRow("group") << QStringLiteral("(ann OR (-bo") << 8 << QStringLiteral("(-") << QString() << QStringLiteral("bo");
    QTest::newRow("other field") << QStringLiteral("subject:an") << -1 << QString() << QString() << QString();
    QTest::newRow("comparison") << QStringLiteral("size>2M") << 0 << QString() << QString() << QStringLiteral("size>2M");
    QTest::newRow("open quote") << QStringLiteral("subject:\"release pl") << -1 << QString() << QString() << QString();
    QTest::newRow("closed quote") << QStringLiteral("\"a b\" ann") << 6 << QString() << QString() << QStringLiteral("ann");
    QTest::newRow("escaped quote") << QStringLiteral("\"a \\\" b\" ann") << 9 << QString() << QString() << QStringLiteral("ann");
    QTest::newRow("quoted word") << QStringLiteral("\"ann\"") << -1 << QString() << QString() << QString();
    QTest::newRow("closing paren") << QStringLiteral("(ann)") << -1 << QString() << QString() << QString();
    QTest::newRow("raw") << QStringLiteral(":=FROM ann") << -1 << QString() << QString() << QString();
    QTest::newRow("only prefix") << QStringLiteral("a -") << -1 << QString() << QString() << QString();
}

void SearchSuggestionsTest::currentToken()
{
    QFETCH(QString, text);
    QFETCH(int, start);
    QFETCH(QString, prefix);
    QFETCH(QString, field);
    QFETCH(QString, value);
    const Token token = UiUtils::SearchSuggestions::currentToken(text);
    QCOMPARE(token.start, start);
    QCOMPARE(token.isValid(), start >= 0);
    if (token.isValid()) {
        QCOMPARE(token.prefix, prefix);
        QCOMPARE(token.field, field);
        QCOMPARE(token.value, value);
    }
}

void SearchSuggestionsTest::contactQuery_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("query");

    QTest::newRow("one letter") << QStringLiteral("a") << QString();
    QTest::newRow("two letters") << QStringLiteral("an") << QStringLiteral("an");
    QTest::newRow("field with one letter") << QStringLiteral("cc:a") << QStringLiteral("a");
    QTest::newRow("empty field") << QStringLiteral("to:") << QString();
    QTest::newRow("tag") << QStringLiteral("tag:wo") << QString();
    QTest::newRow("after space") << QStringLiteral("ann ") << QString();
}

void SearchSuggestionsTest::contactQuery()
{
    QFETCH(QString, text);
    QFETCH(QString, query);
    QCOMPARE(UiUtils::SearchSuggestions::contactQuery(text), query);
}

void SearchSuggestionsTest::recentSearchesMatchTheWholeText()
{
    const QStringList history = {QStringLiteral("from:ann invoice"), QStringLiteral("Invoice 2025"), QStringLiteral("tag:work")};
    // Without text, the whole history is offered
    QCOMPARE(texts(suggest(QString(), history, {}, {}, {}), Kind::RecentSearch), history);
    QCOMPARE(texts(suggest(QStringLiteral("invoice"), history, {}, {}, {}), Kind::RecentSearch),
             (QStringList{QStringLiteral("from:ann invoice"), QStringLiteral("Invoice 2025")}));
    // The query in the field is not offered again
    QCOMPARE(texts(suggest(QStringLiteral("tag:work "), history, {}, {}, {}), Kind::RecentSearch), QStringList());
}

void SearchSuggestionsTest::contactsCompleteTheLastWord()
{
    const QList<Suggestion> plain = suggest(QStringLiteral("invoice an"), {}, people, {}, {});
    QCOMPARE(texts(plain, Kind::Contact),
             (QStringList{QStringLiteral("invoice from:ann@example.org"), QStringLiteral("invoice from:annette@example.org")}));
    QCOMPARE(plain.first().value, QStringLiteral("ann@example.org"));
    QCOMPARE(plain.first().detail, QStringLiteral("Ann Smith"));

    // The typed field and a minus sign are kept; names match as well as addresses
    QCOMPARE(texts(suggest(QStringLiteral("-cc:smi"), {}, people, {}, {}), Kind::Contact),
             (QStringList{QStringLiteral("-cc:ann@example.org")}));
    // Answers about an earlier word do not match the current one
    QCOMPARE(texts(suggest(QStringLiteral("bo"), {}, people, {}, {}), Kind::Contact),
             (QStringList{QStringLiteral("from:bob@example.org")}));
    // A term typed in full is not offered again, and repeated addresses only once
    QCOMPARE(texts(suggest(QStringLiteral("from:bob@example.org"), {}, people + people, {}, {}), Kind::Contact), QStringList());
    QCOMPARE(texts(suggest(QStringLiteral("from:ann"), {}, people + people, {}, {}), Kind::Contact).size(), 2);
    // A single letter is too little for a plain word
    QVERIFY(suggest(QStringLiteral("a"), {}, people, {QStringLiteral("archive")}, {}).isEmpty());
}

void SearchSuggestionsTest::tagsAndFolders()
{
    const QStringList tags = {QStringLiteral("work"), QStringLiteral("to do"), QStringLiteral("$label1")};
    const QList<FolderCandidate> folders = {
        {QStringLiteral("acct/INBOX"), QStringLiteral("INBOX")},
        {QStringLiteral("acct/Work.2024"), QStringLiteral("Work / 2024")},
    };
    QCOMPARE(texts(suggest(QStringLiteral("is:unread wo"), {}, {}, tags, folders), Kind::Tag),
             (QStringList{QStringLiteral("is:unread tag:work")}));
    // A folder takes the place of the word rather than becoming a term
    const QList<Suggestion> folder = suggest(QStringLiteral("is:unread  wo"), {}, {}, tags, folders);
    QCOMPARE(texts(folder, Kind::Folder), (QStringList{QStringLiteral("is:unread")}));
    QCOMPARE(folder.last().folderKey, QStringLiteral("acct/Work.2024"));
    QCOMPARE(folder.last().value, QStringLiteral("Work / 2024"));
    // Tags with spaces are quoted, and tag: on its own lists every tag
    QCOMPARE(texts(suggest(QStringLiteral("tag:"), {}, people, tags, folders), Kind::Tag),
             (QStringList{QStringLiteral("tag:work"), QStringLiteral("tag:\"to do\""), QStringLiteral("tag:$label1")}));
    // tag: offers neither contacts nor folders; from: offers neither tags nor folders
    QCOMPARE(suggest(QStringLiteral("tag:wo"), {}, people, tags, folders).size(), 1);
    QVERIFY(texts(suggest(QStringLiteral("from:wo"), {}, {}, tags, folders), Kind::Tag).isEmpty());
    QVERIFY(texts(suggest(QStringLiteral("from:wo"), {}, {}, tags, folders), Kind::Folder).isEmpty());
    // A negated word is no folder
    QVERIFY(texts(suggest(QStringLiteral("-wo"), {}, {}, tags, folders), Kind::Folder).isEmpty());
    QCOMPARE(texts(suggest(QStringLiteral("-wo"), {}, {}, tags, folders), Kind::Tag), (QStringList{QStringLiteral("-tag:work")}));
    // A tag typed in full is not offered again
    QVERIFY(texts(suggest(QStringLiteral("tag:work"), {}, {}, tags, folders), Kind::Tag).isEmpty());
}

void SearchSuggestionsTest::nothingForRawOrQuotedText()
{
    const QStringList history = {QStringLiteral(":=FROM ann")};
    // Recent raw queries are offered, but raw words are not completed
    const QList<Suggestion> raw = suggest(QStringLiteral(":=FROM an"), history, people, {QStringLiteral("annual")}, {});
    QCOMPARE(raw.size(), 1);
    QCOMPARE(raw.first().kind, Kind::RecentSearch);
    QVERIFY(suggest(QStringLiteral("subject:\"ann"), {}, people, {QStringLiteral("annual")}, {}).isEmpty());
}

void SearchSuggestionsTest::limitsEachKind()
{
    QStringList history;
    QList<Contact> contacts;
    QStringList tags;
    for (int i = 0; i < 8; ++i) {
        history << QStringLiteral("query%1").arg(i);
        contacts.append({QString(), QStringLiteral("query%1@example.org").arg(i)});
        tags << QStringLiteral("query%1").arg(i);
    }
    const QList<Suggestion> result = suggest(QStringLiteral("query"), history, contacts, tags, {}, 3);
    QCOMPARE(texts(result, Kind::RecentSearch).size(), 3);
    QCOMPARE(texts(result, Kind::Contact).size(), 3);
    QCOMPARE(texts(result, Kind::Tag).size(), 3);
    QCOMPARE(result.first().text, QStringLiteral("query0"));
}

void SearchSuggestionsTest::knownFoldersSkipsUnlistedBranches()
{
    using namespace Imap::Mailbox;
    const auto folder = [](const QString &name, const QString &shortName, bool selectable, bool fetched = true) {
        auto *item = new QStandardItem(shortName);
        item->setData(name, RoleMailboxName);
        item->setData(shortName, RoleShortMailboxName);
        item->setData(selectable, RoleMailboxIsSelectable);
        item->setData(fetched, RoleIsFetched);
        return item;
    };
    QStandardItemModel model;
    model.appendRow(folder(QStringLiteral("INBOX"), QStringLiteral("INBOX"), true));
    QStandardItem *archive = folder(QStringLiteral("Archive"), QStringLiteral("Archive"), false);
    archive->appendRow(folder(QStringLiteral("Archive.2024"), QStringLiteral("2024"), true));
    model.appendRow(archive);
    QStandardItem *lists = folder(QStringLiteral("Lists"), QStringLiteral("Lists"), true, false);
    lists->appendRow(folder(QStringLiteral("Lists.kde"), QStringLiteral("kde"), true));
    model.appendRow(lists);

    const QList<FolderCandidate> expected = {
        {QStringLiteral("acct/INBOX"), QStringLiteral("INBOX")},
        {QStringLiteral("acct/Archive.2024"), QStringLiteral("Archive / 2024")},
        {QStringLiteral("acct/Lists"), QStringLiteral("Lists")},
    };
    QCOMPARE(knownFolders(&model, QStringLiteral("acct")), expected);
    QVERIFY(knownFolders(nullptr, QStringLiteral("acct")).isEmpty());
    QVERIFY(knownFolders(&model, QString()).isEmpty());
}

QTEST_GUILESS_MAIN(SearchSuggestionsTest)
