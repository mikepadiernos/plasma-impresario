/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef JMAP_SETTINGS_H
#define JMAP_SETTINGS_H

#include <QByteArray>
#include <QString>
#include <QUrl>

class QSettings;

namespace Jmap {

/** @short How requests prove who is asking */
enum class Authentication {
    /** User name and password, as HTTP Basic authentication */
    Basic,
    /** An access token issued by the provider, such as an app token, sent as a Bearer token */
    Bearer,
};

/** @short How mail written in this account is sent */
enum class Sending {
    /** Through the JMAP server's own submission, which also files the message in Sent */
    Jmap,
    /** Through the SMTP server of the account's sending settings */
    Smtp,
};

/** @short The server and sign-in of a JMAP account (RFC 8620, RFC 8621) */
struct Settings {
    /** The address as entered: a session URL, or only a host whose well-known address is asked */
    QString url;
    QString user;
    /** The password or the token, depending on @ref authentication; never stored by save() */
    QString password;
    Authentication authentication = Authentication::Basic;
    /** Minutes between automatic checks; 0 turns them off */
    int interval = 10;
    Sending sending = Sending::Jmap;
    /** Whether the server offered sending when it was last asked */
    bool canSubmit = true;

    /** The session resource to ask: https://host/.well-known/jmap for a bare host, else the address entered */
    QUrl sessionUrl() const;
    /** The host of @ref sessionUrl(), for messages and trusted certificates */
    QString host() const;
    /** The value of the Authorization header */
    QByteArray authorization() const;

    static Settings fromSettings(const QSettings &account);
    /** Store everything except the password */
    void save(QSettings &account) const;

    static QString authenticationName(Authentication authentication);
    static Authentication authenticationFromName(const QString &name);
    static QString sendingName(Sending sending);
    static Sending sendingFromName(const QString &name);

    /** The session URL for an address as a user would type it, or an invalid URL */
    static QUrl sessionUrlFor(const QString &entered);
    /** Whether @arg url may be used: https, or plain http to this computer only */
    static bool isAcceptable(const QUrl &url);
};

}

#endif
