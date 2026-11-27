/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QString>

class QProcessEnvironment;

namespace UiUtils {

/** @short The token which lets a window of the running application come to the front for a request

Plasma does not let a window take the focus by itself. A launcher, a notification or Plasma Search which the user acted
upon hands over an XDG activation token (Wayland) or a startup ID (X11), and only a window shown with that token comes
to the front. When a request reaches the running application from another process, its token is handed over first,
and the next window shown for a request takes it.

A token can be taken once, and only within lifetimeMs of being handed over, so that a token whose request showed no
window is not spent on an unrelated one later.
*/
class ActivationToken
{
public:
    /** How long a handed-over token waits for the window it is meant for */
    static constexpr qint64 lifetimeMs = 20000;
    /** Tokens are short; anything longer is not one */
    static constexpr int maximumLength = 512;

    /** @short The token this process was started with

    XDG_ACTIVATION_TOKEN, or else DESKTOP_STARTUP_ID; empty when neither holds an acceptable token. Qt clears
    DESKTOP_STARTUP_ID when the application object is created on X11, so this must be read before that.
    */
    static QString fromEnvironment(const QProcessEnvironment &environment);
    /** Whether @arg token can be a token: printable ASCII without spaces, and not too long */
    static bool isAcceptable(const QString &token);

    /** Keep @arg token, replacing one kept earlier; unacceptable tokens leave the kept one alone */
    void handOver(const QString &token, const QDateTime &now);
    /** The kept token, unless it expired; either way, nothing is kept afterwards */
    QString take(const QDateTime &now);
    /** Whether a token is kept, expired or not */
    bool isPending() const;

private:
    QString m_token;
    QDateTime m_handedOver;
};

}
