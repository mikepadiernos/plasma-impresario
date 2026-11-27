/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QtTest>
#include "test_SpecialFolder.h"
#include "Imap/Model/SpecialFolder.h"

using Imap::Mailbox::SpecialUse;
namespace SF = Imap::Mailbox::SpecialFolder;

namespace {

SF::Candidate candidate(const QString &mailbox, const QStringList &flags = {}, bool selectable = true)
{
    SF::Candidate c;
    c.mailbox = mailbox;
    c.separator = QStringLiteral(".");
    c.flags = flags;
    c.selectable = selectable;
    return c;
}

}

void SpecialFolderTest::attributes_data()
{
    QTest::addColumn<QStringList>("flags");
    QTest::addColumn<int>("kind");

    auto row = [](const char *name, const QStringList &flags, SpecialUse kind) {
        QTest::newRow(name) << flags << static_cast<int>(kind);
    };
    row("none", {}, SpecialUse::None);
    row("only structural", {QStringLiteral("\\HASNOCHILDREN"), QStringLiteral("\\SUBSCRIBED")}, SpecialUse::None);
    row("drafts", {QStringLiteral("\\DRAFTS")}, SpecialUse::Drafts);
    row("sent", {QStringLiteral("\\HASNOCHILDREN"), QStringLiteral("\\SENT")}, SpecialUse::Sent);
    row("trash", {QStringLiteral("\\TRASH")}, SpecialUse::Trash);
    row("junk", {QStringLiteral("\\JUNK")}, SpecialUse::Junk);
    row("archive", {QStringLiteral("\\ARCHIVE")}, SpecialUse::Archive);
    row("all", {QStringLiteral("\\ALL")}, SpecialUse::All);
    row("flagged", {QStringLiteral("\\FLAGGED")}, SpecialUse::Flagged);
    row("xlist spam", {QStringLiteral("\\SPAM")}, SpecialUse::Junk);
    row("xlist allmail", {QStringLiteral("\\AllMail")}, SpecialUse::All);
    row("xlist starred", {QStringLiteral("\\Starred")}, SpecialUse::Flagged);
    row("xlist inbox", {QStringLiteral("\\Inbox")}, SpecialUse::Inbox);
    row("mixed case", {QStringLiteral("\\Sent")}, SpecialUse::Sent);
    row("canonical order wins", {QStringLiteral("\\Trash"), QStringLiteral("\\Drafts")}, SpecialUse::Drafts);
    row("no backslash", {QStringLiteral("Sent")}, SpecialUse::None);
}

void SpecialFolderTest::attributes()
{
    QFETCH(QStringList, flags);
    QFETCH(int, kind);
    QCOMPARE(static_cast<int>(SF::fromAttributes(flags)), kind);
}

void SpecialFolderTest::names_data()
{
    QTest::addColumn<QString>("mailbox");
    QTest::addColumn<QString>("separator");
    QTest::addColumn<int>("kind");

    auto row = [](const char *name, const QString &mailbox, const QString &separator, SpecialUse kind) {
        QTest::newRow(name) << mailbox << separator << static_cast<int>(kind);
    };
    const QString dot = QStringLiteral(".");
    const QString slash = QStringLiteral("/");
    row("inbox", QStringLiteral("INBOX"), dot, SpecialUse::Inbox);
    row("inbox lowercase", QStringLiteral("inbox"), dot, SpecialUse::Inbox);
    row("sent", QStringLiteral("Sent"), dot, SpecialUse::Sent);
    row("sent items", QStringLiteral("Sent Items"), dot, SpecialUse::Sent);
    row("sent messages", QStringLiteral("Sent Messages"), dot, SpecialUse::Sent);
    row("sent mail", QStringLiteral("sent mail"), dot, SpecialUse::Sent);
    row("drafts", QStringLiteral("DRAFTS"), dot, SpecialUse::Drafts);
    row("trash", QStringLiteral("Trash"), dot, SpecialUse::Trash);
    row("deleted items", QStringLiteral("Deleted Items"), dot, SpecialUse::Trash);
    row("deleted messages", QStringLiteral("Deleted Messages"), dot, SpecialUse::Trash);
    row("junk", QStringLiteral("Junk"), dot, SpecialUse::Junk);
    row("junk e-mail", QStringLiteral("Junk E-mail"), dot, SpecialUse::Junk);
    row("spam", QStringLiteral("Spam"), dot, SpecialUse::Junk);
    row("archive", QStringLiteral("Archive"), dot, SpecialUse::Archive);
    row("archives", QStringLiteral("Archives"), dot, SpecialUse::Archive);
    row("inbox child", QStringLiteral("INBOX.Sent"), dot, SpecialUse::Sent);
    row("inbox child slash", QStringLiteral("INBOX/Trash"), slash, SpecialUse::Trash);
    row("inbox child lowercase", QStringLiteral("inbox.drafts"), dot, SpecialUse::Drafts);
    row("nested elsewhere", QStringLiteral("Projects.Sent"), dot, SpecialUse::None);
    row("too deep", QStringLiteral("INBOX.Old.Sent"), dot, SpecialUse::None);
    row("dot is not a separator here", QStringLiteral("INBOX.Sent"), slash, SpecialUse::None);
    row("no separator", QStringLiteral("Trash"), QString(), SpecialUse::Trash);
    row("localized", QStringLiteral("Gesendet"), dot, SpecialUse::None);
    row("prefix only", QStringLiteral("Sent Old"), dot, SpecialUse::None);
}

void SpecialFolderTest::names()
{
    QFETCH(QString, mailbox);
    QFETCH(QString, separator);
    QFETCH(int, kind);
    QCOMPARE(static_cast<int>(SF::fromName(mailbox, separator)), kind);
}

void SpecialFolderTest::inboxIsAlwaysInbox()
{
    // Even a misbehaving server that marks INBOX as something else cannot move the Inbox
    const auto res = SF::resolve({candidate(QStringLiteral("INBOX"), {QStringLiteral("\\TRASH")})}, QString());
    QCOMPARE(res.kindOf(QStringLiteral("INBOX")), SpecialUse::Inbox);
    QCOMPARE(res.mailboxFor(SpecialUse::Inbox), QStringLiteral("INBOX"));
    QVERIFY(res.mailboxFor(SpecialUse::Trash).isEmpty());
}

void SpecialFolderTest::attributeBeatsName()
{
    // A folder called "Trash" that the server says is the Junk folder is the Junk folder
    const auto res = SF::resolve({candidate(QStringLiteral("Trash"), {QStringLiteral("\\JUNK")})}, QString());
    QCOMPARE(res.kindOf(QStringLiteral("Trash")), SpecialUse::Junk);
    QCOMPARE(res.mailboxFor(SpecialUse::Junk), QStringLiteral("Trash"));
    QVERIFY(res.mailboxFor(SpecialUse::Trash).isEmpty());
}

void SpecialFolderTest::attributeSuppressesNameGuess()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("Sent")),
        candidate(QStringLiteral("Gesendet"), {QStringLiteral("\\SENT")}),
        candidate(QStringLiteral("Drafts")),
    }, QString());
    QCOMPARE(res.kindOf(QStringLiteral("Sent")), SpecialUse::None);
    QCOMPARE(res.kindOf(QStringLiteral("Gesendet")), SpecialUse::Sent);
    QCOMPARE(res.mailboxFor(SpecialUse::Sent), QStringLiteral("Gesendet"));
    // Kinds that nobody announces still fall back to names
    QCOMPARE(res.kindOf(QStringLiteral("Drafts")), SpecialUse::Drafts);
    QCOMPARE(res.mailboxFor(SpecialUse::Drafts), QStringLiteral("Drafts"));
}

void SpecialFolderTest::firstNameMatchWins()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("INBOX.Trash")),
        candidate(QStringLiteral("Deleted Items")),
        candidate(QStringLiteral("Trash")),
    }, QString());
    QCOMPARE(res.kindOf(QStringLiteral("INBOX.Trash")), SpecialUse::Trash);
    QCOMPARE(res.kindOf(QStringLiteral("Deleted Items")), SpecialUse::None);
    QCOMPARE(res.kindOf(QStringLiteral("Trash")), SpecialUse::None);
    QCOMPARE(res.mailboxFor(SpecialUse::Trash), QStringLiteral("INBOX.Trash"));
}

void SpecialFolderTest::configuredArchive()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("Old Mail")),
        candidate(QStringLiteral("work")),
    }, QStringLiteral("Old Mail"));
    QCOMPARE(res.kindOf(QStringLiteral("Old Mail")), SpecialUse::Archive);
    QCOMPARE(res.mailboxFor(SpecialUse::Archive), QStringLiteral("Old Mail"));
    QCOMPARE(res.kindOf(QStringLiteral("work")), SpecialUse::None);

    // A configured folder which does not exist does not invent anything
    const auto missing = SF::resolve({candidate(QStringLiteral("INBOX"))}, QStringLiteral("Old Mail"));
    QVERIFY(missing.mailboxFor(SpecialUse::Archive).isEmpty());
}

void SpecialFolderTest::configuredArchiveSuppressesArchiveGuess()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("Archive")),
        candidate(QStringLiteral("Old Mail")),
    }, QStringLiteral("Old Mail"));
    QCOMPARE(res.kindOf(QStringLiteral("Archive")), SpecialUse::None);
    QCOMPARE(res.mailboxFor(SpecialUse::Archive), QStringLiteral("Old Mail"));

    // Without the configured folder on the server, the name fallback applies again
    const auto fallback = SF::resolve({candidate(QStringLiteral("Archive"))}, QStringLiteral("Old Mail"));
    QCOMPARE(fallback.mailboxFor(SpecialUse::Archive), QStringLiteral("Archive"));
}

void SpecialFolderTest::archiveAttributeAndConfiguredArchive()
{
    // Both are archives; the account's own setting is where navigation goes
    const auto res = SF::resolve({
        candidate(QStringLiteral("Server Archive"), {QStringLiteral("\\ARCHIVE")}),
        candidate(QStringLiteral("Old Mail")),
    }, QStringLiteral("Old Mail"));
    QCOMPARE(res.kindOf(QStringLiteral("Server Archive")), SpecialUse::Archive);
    QCOMPARE(res.kindOf(QStringLiteral("Old Mail")), SpecialUse::Archive);
    QCOMPARE(res.mailboxFor(SpecialUse::Archive), QStringLiteral("Old Mail"));

    // An attribute on another folder beats the configured name for that folder's own kind
    const auto junk = SF::resolve({candidate(QStringLiteral("Old Mail"), {QStringLiteral("\\JUNK")})},
                                  QStringLiteral("Old Mail"));
    QCOMPARE(junk.kindOf(QStringLiteral("Old Mail")), SpecialUse::Junk);
    QVERIFY(junk.mailboxFor(SpecialUse::Archive).isEmpty());
}

void SpecialFolderTest::nonSelectableIsNotATarget()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("Trash"), {}, false),
        candidate(QStringLiteral("Junk"), {QStringLiteral("\\JUNK")}, false),
        candidate(QStringLiteral("Spam"), {QStringLiteral("\\SPAM")}),
        candidate(QStringLiteral("Deleted Items")),
    }, QString());
    // Name guesses ignore \Noselect containers entirely
    QCOMPARE(res.kindOf(QStringLiteral("Trash")), SpecialUse::None);
    QCOMPARE(res.kindOf(QStringLiteral("Deleted Items")), SpecialUse::Trash);
    QCOMPARE(res.mailboxFor(SpecialUse::Trash), QStringLiteral("Deleted Items"));
    // An attributed container keeps its kind but navigation skips it
    QCOMPARE(res.kindOf(QStringLiteral("Junk")), SpecialUse::Junk);
    QCOMPARE(res.mailboxFor(SpecialUse::Junk), QStringLiteral("Spam"));
}

void SpecialFolderTest::gmailAllMailArchives()
{
    const QList<SF::Candidate> tree{
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("Archive")),
        candidate(QStringLiteral("[Gmail]"), {}, false),
        candidate(QStringLiteral("[Gmail]/All Mail"), {QStringLiteral("\\All")}),
        candidate(QStringLiteral("[Gmail]/Sent Mail"), {QStringLiteral("\\Sent")}),
    };
    // On Gmail a folder called "Archive" is only a label; archiving means leaving the Inbox for All Mail
    const auto res = SF::resolve(tree, QString(), true);
    QCOMPARE(res.kindOf(QStringLiteral("Archive")), SpecialUse::None);
    QCOMPARE(res.kindOf(QStringLiteral("[Gmail]/All Mail")), SpecialUse::All);
    QCOMPARE(res.mailboxFor(SpecialUse::Archive), QStringLiteral("[Gmail]/All Mail"));
    QCOMPARE(res.mailboxFor(SpecialUse::All), QStringLiteral("[Gmail]/All Mail"));
    QCOMPARE(res.mailboxFor(SpecialUse::Sent), QStringLiteral("[Gmail]/Sent Mail"));

    // Elsewhere the same tree keeps the name guess
    const auto plain = SF::resolve(tree, QString());
    QCOMPARE(plain.kindOf(QStringLiteral("Archive")), SpecialUse::Archive);
    QCOMPARE(plain.mailboxFor(SpecialUse::Archive), QStringLiteral("Archive"));
}

void SpecialFolderTest::gmailConfiguredArchiveWins()
{
    const auto res = SF::resolve({
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("Old Mail")),
        candidate(QStringLiteral("[Gmail]/All Mail"), {QStringLiteral("\\All")}),
    }, QStringLiteral("Old Mail"), true);
    QCOMPARE(res.kindOf(QStringLiteral("Old Mail")), SpecialUse::Archive);
    QCOMPARE(res.mailboxFor(SpecialUse::Archive), QStringLiteral("Old Mail"));
    QCOMPARE(res.mailboxFor(SpecialUse::All), QStringLiteral("[Gmail]/All Mail"));

    // So does a server's own \Archive attribute
    const auto attributed = SF::resolve({
        candidate(QStringLiteral("Kept"), {QStringLiteral("\\Archive")}),
        candidate(QStringLiteral("[Gmail]/All Mail"), {QStringLiteral("\\All")}),
    }, QString(), true);
    QCOMPARE(attributed.mailboxFor(SpecialUse::Archive), QStringLiteral("Kept"));
}

void SpecialFolderTest::gmailWithoutAllMail()
{
    // All Mail hidden from IMAP in Gmail's settings: there is nothing to archive into
    const auto res = SF::resolve({
        candidate(QStringLiteral("INBOX")),
        candidate(QStringLiteral("Archive")),
    }, QString(), true);
    QCOMPARE(res.kindOf(QStringLiteral("Archive")), SpecialUse::None);
    QVERIFY(res.mailboxFor(SpecialUse::Archive).isEmpty());
    QVERIFY(res.mailboxFor(SpecialUse::All).isEmpty());
}

void SpecialFolderTest::rankAndNavigableKinds()
{
    QVERIFY(SF::sortRank(SpecialUse::Inbox) < SF::sortRank(SpecialUse::Drafts));
    QVERIFY(SF::sortRank(SpecialUse::Drafts) < SF::sortRank(SpecialUse::Sent));
    QVERIFY(SF::sortRank(SpecialUse::Sent) < SF::sortRank(SpecialUse::Archive));
    QVERIFY(SF::sortRank(SpecialUse::Archive) < SF::sortRank(SpecialUse::Junk));
    QVERIFY(SF::sortRank(SpecialUse::Junk) < SF::sortRank(SpecialUse::Trash));
    QVERIFY(SF::sortRank(SpecialUse::Trash) < SF::sortRank(SpecialUse::All));
    QVERIFY(SF::sortRank(SpecialUse::All) < SF::sortRank(SpecialUse::Flagged));
    QVERIFY(SF::sortRank(SpecialUse::Flagged) < SF::sortRank(SpecialUse::None));

    const QList<SpecialUse> expected{SpecialUse::Inbox, SpecialUse::Drafts, SpecialUse::Sent,
                                     SpecialUse::Archive, SpecialUse::Junk, SpecialUse::Trash};
    QCOMPARE(SF::navigableKinds(), expected);
}

QTEST_GUILESS_MAIN(SpecialFolderTest)
