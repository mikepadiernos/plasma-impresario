/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/SearchIndexDialog.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MailboxMetadata.h"
#include "Imap/Model/SearchIndexer.h"

#include <KLocalizedString>
#include <memory>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

using Gui::SearchIndexDialog;
using namespace Imap::Mailbox;

namespace {

const QString inbox = QStringLiteral("INBOX");
const QString archive = QStringLiteral("Archive");
const QString drafts = QStringLiteral("Drafts");

SyncState syncState(uint uidValidity, uint exists, uint uidNext)
{
    SyncState state;
    state.setUidValidity(uidValidity);
    state.setExists(exists);
    state.setUidNext(uidNext);
    state.setRecent(0);
    state.setUnSeenCount(0);
    return state;
}

}

class SearchIndexDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void withoutIndexEverythingIsOff();
    void showsConfiguredAndConfirmedFolders();
    void pausesAndResumes();
    void rebuildAsksFirst();
    void followsTheIndexAway();

private:
    void runIndexer();
    QTreeWidgetItem *row(SearchIndexDialog &dialog, const QString &mailbox) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<CombinedCache> m_cache;
    std::unique_ptr<SearchIndexer> m_indexer;
    int m_counter = 0;
};

void SearchIndexDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
}

void SearchIndexDialogTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_cache = std::make_shared<CombinedCache>(QStringLiteral("search-index-dialog-%1").arg(++m_counter), m_dir->path());
    QVERIFY(m_cache->open());
    m_cache->setChildMailboxes(QString(), {
        MailboxMetadata(inbox, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(archive, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
        MailboxMetadata(drafts, QStringLiteral("/"), {QStringLiteral("\\HasNoChildren")}),
    });
    // Empty, so indexed completely
    m_cache->setMailboxSyncState(inbox, syncState(1, 0, 1));
    m_cache->setUidMapping(inbox, Imap::Uids{});
    // A message whose headers were never loaded, so the index cannot hold it
    m_cache->setMailboxSyncState(archive, syncState(2, 1, 2));
    m_cache->setUidMapping(archive, Imap::Uids{1});
    // Drafts was never synchronized
    std::shared_ptr<CombinedCache> cache = m_cache;
    m_indexer = std::make_unique<SearchIndexer>([cache]() { return std::static_pointer_cast<AbstractCache>(cache); });
    m_indexer->setTimings(0, 0, 50);
    QVERIFY2(m_indexer->open(), qPrintable(m_indexer->unavailableReason()));
    runIndexer();
}

void SearchIndexDialogTest::cleanup()
{
    m_indexer.reset();
    m_cache.reset();
    m_dir.reset();
}

void SearchIndexDialogTest::runIndexer()
{
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    m_indexer->update();
    QVERIFY(finished.wait(5000));
}

QTreeWidgetItem *SearchIndexDialogTest::row(SearchIndexDialog &dialog, const QString &mailbox) const
{
    auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("indexFolders"));
    const QList<QTreeWidgetItem *> items = tree->findItems(mailbox, Qt::MatchExactly, 0);
    return items.isEmpty() ? nullptr : items.first();
}

void SearchIndexDialogTest::withoutIndexEverythingIsOff()
{
    SearchIndexDialog dialog;
    dialog.setIndexer(nullptr, QStringLiteral("Work"));
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Offline Search Index: Work"));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("off")));
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("indexPause"))->isEnabled());
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("indexRebuild"))->isEnabled());
    QCOMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("indexFolders"))->topLevelItemCount(), 0);
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexWarning"))->isHidden());
}

void SearchIndexDialogTest::showsConfiguredAndConfirmedFolders()
{
    SearchIndexDialog dialog;
    dialog.setIndexer(m_indexer.get(), QStringLiteral("Work"));
    dialog.show();
    auto *tree = dialog.findChild<QTreeWidget *>(QStringLiteral("indexFolders"));
    QCOMPARE(tree->topLevelItemCount(), 3);
    QCOMPARE(tree->topLevelItem(0)->text(0), inbox);
    using State = SearchIndexer::FolderCoverage::State;
    QCOMPARE(row(dialog, inbox)->data(1, Qt::UserRole).toInt(), int(State::Indexed));
    QCOMPARE(row(dialog, inbox)->text(1), QStringLiteral("Indexed"));
    QVERIFY(!row(dialog, inbox)->text(5).isEmpty());
    QCOMPARE(row(dialog, archive)->data(1, Qt::UserRole).toInt(), int(State::Partial));
    QCOMPARE(row(dialog, archive)->text(2), QStringLiteral("1"));
    QCOMPARE(row(dialog, archive)->text(3), QStringLiteral("0"));
    QCOMPARE(row(dialog, drafts)->data(1, Qt::UserRole).toInt(), int(State::NotCached));
    QVERIFY(row(dialog, drafts)->text(2).isEmpty());
    QVERIFY(row(dialog, drafts)->text(5).isEmpty());

    const QString summary = dialog.findChild<QLabel *>(QStringLiteral("indexSummary"))->text();
    QVERIFY2(summary.contains(QStringLiteral("3 folders to index, 1 confirmed as indexed")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("Last updated")), qPrintable(summary));
    auto *warning = dialog.findChild<QLabel *>(QStringLiteral("indexWarning"));
    QVERIFY(warning->isVisible());
    QVERIFY2(warning->text().contains(QStringLiteral("2 folders are not fully indexed")), qPrintable(warning->text()));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("up to date")));
    QVERIFY(dialog.findChild<QProgressBar *>(QStringLiteral("indexProgress"))->isHidden());

    // Once the headers arrive and the index catches up, the folder is confirmed
    m_cache->setUidMapping(archive, Imap::Uids{});
    m_cache->setMailboxSyncState(archive, syncState(2, 0, 2));
    runIndexer();
    QTRY_COMPARE(row(dialog, archive)->data(1, Qt::UserRole).toInt(), int(State::Indexed));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexWarning"))->text().contains(QStringLiteral("1 folder is")));
}

void SearchIndexDialogTest::pausesAndResumes()
{
    SearchIndexDialog dialog;
    dialog.setIndexer(m_indexer.get(), QStringLiteral("Work"));
    QSignalSpy paused(&dialog, &SearchIndexDialog::pausedChanged);
    auto *pause = dialog.findChild<QPushButton *>(QStringLiteral("indexPause"));
    QVERIFY(pause->isEnabled());
    QCOMPARE(pause->text(), QStringLiteral("Pause"));
    QTest::mouseClick(pause, Qt::LeftButton);
    QVERIFY(m_indexer->isPaused());
    QCOMPARE(paused.size(), 1);
    QCOMPARE(paused.at(0).at(0).toBool(), true);
    QCOMPARE(pause->text(), QStringLiteral("Resume"));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("paused")));

    // Changes while paused are reported and indexed on resume
    m_indexer->scheduleUpdate();
    dialog.refresh();
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("resumed")));
    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    QTest::mouseClick(pause, Qt::LeftButton);
    QVERIFY(!m_indexer->isPaused());
    QCOMPARE(paused.size(), 2);
    QCOMPARE(paused.at(1).at(0).toBool(), false);
    QCOMPARE(pause->text(), QStringLiteral("Pause"));
    QVERIFY(finished.wait(5000));
}

void SearchIndexDialogTest::rebuildAsksFirst()
{
    SearchIndexDialog dialog;
    dialog.setIndexer(m_indexer.get(), QStringLiteral("Work"));
    dialog.show();
    auto *rebuild = dialog.findChild<QPushButton *>(QStringLiteral("indexRebuild"));
    const auto answer = [](QMessageBox::StandardButton button) {
        QTimer::singleShot(0, [button]() {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(box);
            box->button(button)->click();
        });
    };

    answer(QMessageBox::No);
    QTest::mouseClick(rebuild, Qt::LeftButton);
    QVERIFY(!m_indexer->isRunning());
    QCOMPARE(m_indexer->index()->folders().size(), 2);

    QSignalSpy finished(m_indexer.get(), &SearchIndexer::finished);
    answer(QMessageBox::Yes);
    QTest::mouseClick(rebuild, Qt::LeftButton);
    QVERIFY(m_indexer->isRunning());
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("indexState"))->text().contains(QStringLiteral("Indexing")));
    QVERIFY(finished.wait(5000));
    QTRY_COMPARE(row(dialog, inbox)->data(1, Qt::UserRole).toInt(), int(SearchIndexer::FolderCoverage::State::Indexed));
}

void SearchIndexDialogTest::followsTheIndexAway()
{
    SearchIndexDialog dialog;
    dialog.setIndexer(m_indexer.get(), QStringLiteral("Work"));
    QVERIFY(dialog.findChild<QTreeWidget *>(QStringLiteral("indexFolders"))->topLevelItemCount() > 0);
    // Saving the account settings replaces the account's index
    m_indexer.reset();
    QTRY_COMPARE(dialog.findChild<QTreeWidget *>(QStringLiteral("indexFolders"))->topLevelItemCount(), 0);
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("indexPause"))->isEnabled());
    QCOMPARE(dialog.indexer(), nullptr);
}

QTEST_MAIN(SearchIndexDialogTest)
#include "test_SearchIndexDialog.moc"
