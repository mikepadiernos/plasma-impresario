//
// C++ Interface: qwwsmtpclient
//
// Description:
//
//
// Author: Witold Wysota <wysota@wysota.eu.org>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef QWWSMTPCLIENT_H
#define QWWSMTPCLIENT_H

#include <QObject>
#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <QSslCertificate>
#include <QSslError>

class QwwSmtpClientPrivate;

/*!
        \class  QwwSmtpClient
        \author Witold Wysota <wysota@wysota.eu.org>
        \brief  Cross-platform asynchronous handling of client side SMTP connections

                Features:

                Connection mode - open, TLS, SSL
                Authentication  - PLAIN, LOGIN
                Handshake       - HELO, EHLO

                - low-level mail sending (everything you pass, goes through to the server)
                - raw command sending
                - multiple rcpt
                - option reporting

       \todo    CRAM-MD5 Authentication
                VRFY
                abort()
                SSL errors handling
                network errors
                error handling (status codes, etc.)

*/
class QwwSmtpClient : public QObject {
    Q_OBJECT
    Q_FLAGS(Options);
    Q_FLAGS(AuthModes);

public:
    explicit QwwSmtpClient(QObject *parent = nullptr);
    ~QwwSmtpClient() override;
    enum State { Disconnected, Connecting, Connected, TLSRequested, Authenticating, Sending, Disconnecting };
    Q_ENUM(State);
    enum Option { NoOptions = 0, StartTlsOption, SizeOption, PipeliningOption, EightBitMimeOption, AuthOption };
    Q_DECLARE_FLAGS ( Options, Option );
    enum AuthMode { AuthNone = 0, AuthAny = 1, AuthPlain = 2, AuthLogin = 4 };
    Q_DECLARE_FLAGS ( AuthModes, AuthMode );
    void setLocalName(const QString &ln);
    void setLocalNameEncrypted(const QString &ln);

    int connectToHost ( const QString & hostName, quint16 port = 25);
    int connectToHostEncrypted(const QString &hostName, quint16 port = 465);
//     int connectToHost ( const QHostAddress & address, quint16 port = 25);
    int authenticate(const QString &user, const QString &password, AuthMode mode = AuthAny);
    /*! Authenticate with the first candidate whose SASL mechanism the server advertises.

        Each candidate holds the mechanism name, the base64 initial response sent with the AUTH command, and the
        response which acknowledges an error challenge. This suits mechanisms with a single client response, such as
        XOAUTH2 and OAUTHBEARER. A failure is reported by saslAuthenticationFailed() before done(false).
    */
    struct SaslCandidate {
        QByteArray mechanism;
        QByteArray initialResponse;
        QByteArray cancelResponse;
    };
    int authenticateSasl(const QList<SaslCandidate> &candidates);
    /*! SASL mechanisms advertised by the server, in upper case */
    QStringList supportedSaslMechanisms() const;
    int sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &content);
    int sendMailBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &url);
    int rawCommand(const QString &cmd);
    AuthModes supportedAuthModes() const;
    Options options() const;
    QString errorString() const;
    /*! The certificate chain the server presented during the TLS handshake */
    QList<QSslCertificate> peerCertificateChain() const;
public slots:
    int disconnectFromHost();
    int startTls();
    /*! Accept the errors reported by sslErrors(); only effective when called from a slot connected to it */
    void ignoreSslErrors();
    /*! Finish a TLS handshake that sslErrors() paused because its slots did not accept the errors

        The handshake continues when \a ignore is true, otherwise it fails with a socket error. */
    void resumeAfterSslErrors(bool ignore);

signals:
    void done(bool);
    void connected();
    void disconnected();
    void stateChanged(QwwSmtpClient::State);
    void commandFinished(int, bool error);
    void commandStarted(int);
    void tlsStarted();
    void authenticated();
    /*! SASL authentication failed; the mechanism is empty when the server offers none of the candidates, and the
        challenge holds the server's base64 error challenge, if any */
    void saslAuthenticationFailed(const QByteArray &mechanism, const QByteArray &challenge);
    void rawCommandReply(int code, const QString &details);
    void sslErrors(const QList<QSslError> &);
    void socketError(QAbstractSocket::SocketError err, const QString& message);
    void logReceived(const QByteArray& data);
    void logSent(const QByteArray& data);

private:
    QwwSmtpClientPrivate *d;
    Q_PRIVATE_SLOT(d, void onConnected());
    Q_PRIVATE_SLOT(d, void onDisconnected());
    Q_PRIVATE_SLOT(d, void _q_readFromSocket());
    Q_PRIVATE_SLOT(d, void _q_encrypted());
    friend class QwwSmtpClientPrivate;

    QwwSmtpClient(const QwwSmtpClient&); // don't implement
    QwwSmtpClient& operator=(const QwwSmtpClient&); // don't implement
};

#endif
