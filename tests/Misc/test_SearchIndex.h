/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_SEARCHINDEX_H
#define TEST_SEARCHINDEX_H

#include <memory>
#include <QObject>
#include <QTemporaryDir>

#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/SearchIndexer.h"

/** @short The local full-text index of the offline cache and its searches */
class SearchIndexTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();

    void testTranslate_data();
    void testTranslate();
    void testUnavailableWithoutPersistentCache();
    void testSearches_data();
    void testSearches();
    void testFolderFilterAndText();
    void testTextArrivingLater();
    void testFlagsAndRemovedMessages();
    void testUidValidityChange();
    void testRemovedFolder();
    void testDropAndReopen();
    void testPlainTextOfHtml();

private:
    void seed();
    void runIndexer();
    QList<uint> matches(const QStringList &conditions, const QString &mailbox = QStringLiteral("INBOX"));

    std::unique_ptr<QTemporaryDir> m_dir;
    std::shared_ptr<Imap::Mailbox::CombinedCache> m_cache;
    std::unique_ptr<Imap::Mailbox::SearchIndexer> m_indexer;
    int m_counter = 0;
};

#endif
