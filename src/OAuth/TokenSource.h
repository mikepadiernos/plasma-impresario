/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OAUTH_TOKENSOURCE_H
#define OAUTH_TOKENSOURCE_H

#include <QObject>

#include "OAuth/OAuth.h"

namespace OAuth {

/** @short Provides access tokens of one account to the IMAP and SMTP sessions

The source is attached to the object which owns the connections (a direct child of the IMAP model, for example), so
code that only knows the owner can find it through find(). Without a source, a connection uses password
authentication.
*/
class TokenSource : public QObject
{
    Q_OBJECT
public:
    explicit TokenSource(QObject *parent = nullptr);
    ~TokenSource() override;

    /** @short SASL mechanisms the provider supports, most preferred first */
    virtual QList<Mechanism> mechanisms() const = 0;

    /** @short Ask for a valid access token; answered by accessTokenAvailable() or failed(), never synchronously */
    virtual void requestAccessToken() = 0;

    /** @short The server refused this token; the next request must not return it again */
    virtual void rejectAccessToken(const QString &accessToken) = 0;

    /** @short Server which receives the tokens, for the OAUTHBEARER host and port fields */
    void setServer(const QString &host, quint16 port);
    QString host() const;
    quint16 port() const;

    /** @short Reason of the last failure, or NoError */
    Error lastError() const;

    /** @short The token source attached directly to the owner, or nullptr */
    static TokenSource *find(const QObject *owner);

signals:
    void accessTokenAvailable(const QString &accessToken);
    /** @short No token is available; the message is localized and tells the user how to recover */
    void failed(const QString &message);

protected:
    void setLastError(Error error);

private:
    QString m_host;
    quint16 m_port = 0;
    Error m_lastError = Error::NoError;
};

}

#endif
