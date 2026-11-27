/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_PRIVATEFILES_H
#define TEST_PRIVATEFILES_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify that the cache and opened attachments stay user-only and that old copies are removed */
class PrivateFilesTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void makeFolderCreatesUserOnlyParents();
    void makeFolderTightensAnExistingFolder();
    void makeFolderRefusesAFile();
    void tightenFoldersAndFiles();
    void tightenLeavesLinkedFilesAlone();
    void safeFileName_data();
    void safeFileName();
    void safeFileNameFitsTheFileSystem();
    void openedAttachmentIsUserOnly();
    void openedAttachmentStaysInsideItsFolder();
    void openedAttachmentsDoNotCollide();
    void openedAttachmentFolderInTheWay();
    void removingACopyRemovesItsFolder();
    void removingAFileOutsideKeepsItsFolder();
    void oldCopiesAreRemovedAfterADay();
    void cacheFolderIsInsideTheCache();
    void copiesChangedBeforeAreRemoved();

private:
    QTemporaryDir m_dir;
};

#endif
