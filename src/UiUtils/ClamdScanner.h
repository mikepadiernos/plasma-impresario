/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "UiUtils/MailSafety.h"

class QLocalSocket;

namespace UiUtils {

/** @short Scan a file with the local ClamAV daemon, clamd

The file is streamed to clamd's local socket with the INSTREAM command, so clamd needs no access to the file itself and
nothing leaves the computer. The socket is a Unix domain socket; a TCP clamd is not supported.
*/
class ClamdScanner : public QObject
{
    Q_OBJECT

public:
    explicit ClamdScanner(QObject *parent = nullptr);
    ~ClamdScanner() override;

    /** @short The sockets where distributions put clamd's local socket, in the order they are tried */
    static QStringList defaultSockets();
    /** @short The first of @arg candidates which exists, or an empty string */
    static QString findSocket(const QStringList &candidates);

    /** @short One INSTREAM chunk: @arg data after its length as four bytes in network byte order */
    static QByteArray chunk(const QByteArray &data);
    /** @short The result of clamd's @arg reply to INSTREAM; @arg threat receives the name of a found threat */
    static MailSafety::ScanResult parseReply(const QByteArray &reply, QString *threat = nullptr);

    /** How long a scan may take before it counts as failed, in milliseconds */
    void setTimeout(int milliseconds);
    /** The size of the chunks the file is sent in */
    void setChunkSize(int bytes);

    /** @short Scan the file at @arg path through clamd's socket at @arg socketPath

    finished() is emitted exactly once, from the event loop and never before this returns; it is not emitted when the
    scanner is destroyed first. An empty or unreachable socket gives
    MailSafety::ScanResult::Unavailable; an unreadable file, an error reply, a closed connection or a timeout give
    MailSafety::ScanResult::Failed.
    */
    void scanFile(const QString &path, const QString &socketPath);

signals:
    void finished(UiUtils::MailSafety::ScanResult result, const QString &threat);

private:
    void onConnected();
    void sendMore();
    void onReadyRead();
    void onDisconnected();
    void finish(MailSafety::ScanResult result, const QString &threat = QString());

    QPointer<QLocalSocket> m_socket;
    QFile m_file;
    QTimer m_timer;
    QByteArray m_reply;
    int m_chunkSize = 64 * 1024;
    bool m_connected = false;
    bool m_sentEnd = false;
    bool m_running = false;
};

}
