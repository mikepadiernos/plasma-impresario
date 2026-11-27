/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_FILESEARCH_H
#define TEST_FILESEARCH_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify how Baloo's coverage of a folder is read and how file manager searches are built */
class FileSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void notInstalled();
    void disabled();
    void defaultsToTheHomeFolder();
    void closestFolderDecides();
    void hiddenFolders();
    void namesOnly();
    void indexedSubfolders();
    void expandsHomeInPaths();
    void indexedSearchUrl();
    void liveSearchUrl();
    void savedFilesFolder();

private:
    QString configPath(const QString &name, const QByteArray &contents);
    QTemporaryDir m_dir;
};

#endif
