/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_COMPOSER_ATTACHMENTREQUESTS_H
#define TEST_COMPOSER_ATTACHMENTREQUESTS_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify which files a mailto: URL or the command line may ask to attach */
class ComposerAttachmentRequestsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void launcherUrlsRoundTrip_data();
    void launcherUrlsRoundTrip();
    void attachmentKeys();
    void otherSchemesIgnored();
    void absolutePathsAndFileUrls();
    void refusesRemoteAndRelative_data();
    void refusesRemoteAndRelative();
    void refusesMissingFoldersAndLinks();
    void hiddenFilesOnlyFromCommandLine();
    void linkIntoHiddenFolder();
    void systemFiles();
    void unreadableFile();
    void duplicatesDropped();

private:
    QString makeFile(const QString &relativePath, const QByteArray &content = "data");
    QTemporaryDir m_dir;
};

#endif
