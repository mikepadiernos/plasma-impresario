/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_PLASMASEARCH_H
#define TEST_PLASMASEARCH_H

#include <memory>
#include <QObject>
#include <QTemporaryDir>

#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/SearchIndexer.h"

/** @short Answering Plasma Search from the offline search indexes of several accounts */
class PlasmaSearchTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();

    void offByDefault();
    void words_data();
    void words();
    void matchIdRoundTrip_data();
    void matchIdRoundTrip();
    void rejectedMatchIds_data();
    void rejectedMatchIds();
    void matchesAcrossAccounts();

private:
    struct Account {
        std::shared_ptr<Imap::Mailbox::CombinedCache> cache;
        std::unique_ptr<Imap::Mailbox::SearchIndexer> indexer;
    };
    void openAccount(Account &account, const QString &name);

    QTemporaryDir m_dir;
    Account m_home;
    Account m_work;
};

#endif
