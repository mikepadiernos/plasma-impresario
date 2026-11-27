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
    void acceptsRemote_data();
    void acceptsRemote();
    void mailtoRemoteRules_data();
    void mailtoRemoteRules();
    void notFiles();
    void displayUrlHidesCredentials();
    void refusesRelative_data();
    void refusesRelative();
    void refusesMissingFoldersAndLinks();
    void hiddenFilesOnlyFromCommandLine();
    void linkIntoHiddenFolder();
    void systemFiles();
    void unreadableFile();
    void duplicatesDropped();
    void remoteAttachmentCopies();
    void remoteAttachmentRemoved();

private:
    QString makeFile(const QString &relativePath, const QByteArray &content = "data");
    QTemporaryDir m_dir;
};

#endif
