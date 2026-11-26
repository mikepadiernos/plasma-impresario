/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

/** @short Privacy checks for links and remote content in the reader

These decide what the reader tells the user before a link from a message is
opened, and which servers a message's blocked remote content would contact.
Nothing here loads anything.
*/
namespace UiUtils {

/** @short Why opening a link from a message deserves a second look */
enum class LinkWarning {
  /** The link text is a web address on another site than the target */
  TextShowsOtherSite,
  /** The target has a user name before the host, as in
     https://bank.example@other.example/ */
  UserInfo,
  /** The target host is a numeric IP address */
  IpAddress,
  /** The target host has internationalized labels, which can imitate
     another name */
  InternationalHost,
};

struct LinkAssessment {
  /** Host of the target as it would be contacted, in its ASCII (punycode)
   * form */
  QString host;
  /** Host which the link text shows, or empty if the text is not an address
   */
  QString textHost;
  QList<LinkWarning> warnings;
};

/** @short Host named by @arg text when the whole text is a web address

"https://www.example.org/login", "www.example.org" and "example.org/path"
give "www.example.org" or "example.org"; "Click here" or "Sign in at
example.org" give an empty string, so that ordinary link text never warns.
The result is lower-case and in its ASCII (punycode) form.
*/
QString linkTextHost(const QString &text);

/** @short Checks an http or https @arg target clicked as @arg text */
LinkAssessment assessLink(const QUrl &target, const QString &text);

/** @short Whether @arg host is @arg site or a subdomain of it, ignoring a
 * leading "www." on either */
bool isSameSite(const QString &host, const QString &site);

/** @short Distinct hosts of @arg urls, lower-case, in the order they first
 * appear; URLs without a host are skipped */
QStringList remoteHosts(const QList<QUrl> &urls);

} // namespace UiUtils
