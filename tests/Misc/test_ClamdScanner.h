/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_CLAMDSCANNER_H
#define TEST_CLAMDSCANNER_H

#include <QObject>
#include <QTemporaryDir>

/** @short Verify the INSTREAM framing, the reading of clamd's replies and the failures of a scan against a fake clamd */
class ClamdScannerTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase();
  void chunkHasItsLengthFirst();
  void parsesReplies_data();
  void parsesReplies();
  void findsTheFirstExistingSocket();
  void streamsTheFileInChunks();
  void emptyFileIsStillScanned();
  void reportsTheThreat();
  void errorReplyIsAFailure();
  void closedConnectionIsAFailure();
  void missingSocketIsUnavailable();
  void unreadableFileIsAFailure();
  void silentScannerTimesOut();
  void liveClamd();

private:
  QString writeFile(const QString &name, const QByteArray &content);
  QString socketPath(const QString &name) const;

  QTemporaryDir m_dir;
};

#endif
