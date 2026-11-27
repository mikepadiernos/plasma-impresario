/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <QStringList>

class QUrl;

namespace Composer {

/** @short Files another application asks to attach to a new message

File managers hand files to the mail client either on the command line or, through KDE's e-mail launcher and the XDG
Email portal, as `attach=` items of a mailto: URL. A mailto: link can also come from a web page, which must not be
able to pick files the user did not choose, so requests from mailto: URLs are held to stricter rules than files named
on the command line.
*/
namespace AttachmentRequests {

/** Where a request came from */
enum class Source {
    /** An `attach` or `attachment` item of a mailto: URL */
    MailtoUrl,
    /** A file named on the command line, which only the user can give */
    CommandLine,
};

struct Request {
    /** As it was given, for reporting */
    QString requested;
    /** The absolute path of the local file to attach; empty when refused */
    QString path;
    /** Why it was refused, translated; empty when accepted */
    QString problem;

    bool accepted() const { return problem.isEmpty(); }
};

/** The values of the `attach` and `attachment` items of a mailto: @arg url, decoded, in their order */
QStringList fromMailto(const QUrl &url);

/** Whether @arg url carries any attachment request */
bool hasRequests(const QUrl &url);

/** Check one @arg requested file, given as a `file:` URL or an absolute path

Only readable regular files are accepted. Requests from a mailto: URL are also refused for hidden files, files in hidden
folders and system folders such as `/proc` and `/etc`, also when reached through a link.
*/
Request resolve(const QString &requested, Source source);

/** Resolve each of @arg requested, dropping repeated requests for the same file */
QList<Request> resolveAll(const QStringList &requested, Source source);

}
}
