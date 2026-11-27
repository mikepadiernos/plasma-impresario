/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/LinkSafety.h"

#include <QRegularExpression>
#include <QSet>

namespace UiUtils {

namespace {

QString asciiHost(const QString &host) {
  const QString ace = QString::fromLatin1(QUrl::toAce(host));
  return (ace.isEmpty() ? host : ace).toLower();
}

QString withoutWww(const QString &host) {
  return host.startsWith(QLatin1String("www.")) ? host.mid(4) : host;
}

bool isIpAddress(const QString &host) {
  if (host.contains(QLatin1Char(':')))
    return true;
  // Browsers also accept hexadecimal, octal and shortened IPv4 forms
  static const QRegularExpression ipv4(
      QStringLiteral(
          "^(0x[0-9a-f]*|[0-9]+)(\\.(0x[0-9a-f]*|[0-9]+)){0,3}\\.?$"),
      QRegularExpression::CaseInsensitiveOption);
  return ipv4.match(host).hasMatch();
}

} // namespace

QString linkTextHost(const QString &text) {
  static const QRegularExpression address(
      QStringLiteral(
          "^(?:(https?)://)?([^\\s/?#:@]+)(?::[0-9]+)?(?:[/?#]\\S*)?$"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::UseUnicodePropertiesOption);
  const auto match = address.match(text.trimmed());
  if (!match.hasMatch())
    return QString();
  QString host = match.captured(2);
  while (host.endsWith(QLatin1Char('.')))
    host.chop(1);
  const QStringList labels = host.split(QLatin1Char('.'));
  if (labels.size() < 2 || labels.contains(QString()))
    return QString();
  const QString lowered = host.toLower();
  if (match.capturedLength(1) == 0 &&
      !lowered.startsWith(QLatin1String("www."))) {
    // A bare "name.tld" counts only with a top-level domain made of letters,
    // and not when it reads like a file name
    static const QRegularExpression letters(
        QStringLiteral("^\\p{L}{2,63}$"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QSet<QString> fileExtensions{
        QStringLiteral("pdf"),  QStringLiteral("doc"),  QStringLiteral("docx"),
        QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("ppt"),
        QStringLiteral("pptx"), QStringLiteral("odt"),  QStringLiteral("ods"),
        QStringLiteral("odp"),  QStringLiteral("txt"),  QStringLiteral("csv"),
        QStringLiteral("zip"),  QStringLiteral("gz"),   QStringLiteral("png"),
        QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("gif"),
        QStringLiteral("svg"),  QStringLiteral("htm"),  QStringLiteral("html"),
        QStringLiteral("ics"),  QStringLiteral("vcf"),  QStringLiteral("mp3"),
        QStringLiteral("mp4"),  QStringLiteral("mov"),  QStringLiteral("exe"),
    };
    const QString tld = lowered.section(QLatin1Char('.'), -1);
    if (!letters.match(tld).hasMatch() || fileExtensions.contains(tld))
      return QString();
  }
  return asciiHost(host);
}

bool isSameSite(const QString &host, const QString &site) {
  const QString a = withoutWww(host.toLower());
  const QString b = withoutWww(site.toLower());
  if (a.isEmpty() || b.isEmpty())
    return false;
  return a == b || a.endsWith(QLatin1Char('.') + b) ||
         b.endsWith(QLatin1Char('.') + a);
}

LinkAssessment assessLink(const QUrl &target, const QString &text) {
  LinkAssessment result;
  const QString scheme = target.scheme().toLower();
  if (scheme == QLatin1String("mailto")) {
    static const QRegularExpression address(
        QStringLiteral("^(?:mailto:)?([^\\s@<>(),;:\"]+@[^\\s@<>(),;:\"]+)$"),
        QRegularExpression::CaseInsensitiveOption);
    result.address = target.path().section(QLatin1Char(','), 0, 0).trimmed();
    const auto match = address.match(text.trimmed());
    if (match.hasMatch())
      result.textAddress = match.captured(1);
    if (!result.textAddress.isEmpty() &&
        result.textAddress.compare(result.address, Qt::CaseInsensitive) != 0)
      result.warnings << LinkWarning::TextShowsOtherAddress;
    return result;
  }
  if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
    return result;
  result.host = asciiHost(target.host());
  result.textHost = linkTextHost(text);
  if (!result.textHost.isEmpty() && !isSameSite(result.host, result.textHost))
    result.warnings << LinkWarning::TextShowsOtherSite;
  if (!target.userInfo().isEmpty())
    result.warnings << LinkWarning::UserInfo;
  if (isIpAddress(result.host)) {
    result.warnings << LinkWarning::IpAddress;
  } else {
    const QStringList labels = result.host.split(QLatin1Char('.'));
    for (const QString &label : labels) {
      if (label.startsWith(QLatin1String("xn--"))) {
        result.warnings << LinkWarning::InternationalHost;
        break;
      }
    }
  }
  return result;
}

QStringList remoteHosts(const QList<QUrl> &urls) {
  QStringList hosts;
  for (const QUrl &url : urls) {
    const QString host = url.host().toLower();
    if (!host.isEmpty() && !hosts.contains(host))
      hosts << host;
  }
  return hosts;
}

} // namespace UiUtils
