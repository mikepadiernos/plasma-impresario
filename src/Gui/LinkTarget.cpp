/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/LinkTarget.h"

#include "UiUtils/LinkSafety.h"

#include <KLocalizedString>

namespace Gui {

namespace {

/** Longest target shown; the rest is left out from the middle */
constexpr qsizetype maxShownLength = 1000;

QString bold(const QString &text)
{
    return QLatin1String("<b>") + text.toHtmlEscaped() + QLatin1String("</b>");
}

/** Escapes @arg text and lets it wrap after punctuation or every few characters, since an address has no spaces */
QString breakable(const QString &text)
{
    static const QString breakAfter = QStringLiteral("/?&=#.-_~:%@+,;");
    const QChar zeroWidthSpace(0x200B);
    QString result;
    qsizetype run = 0;
    for (const QChar c : text) {
        result += QString(c).toHtmlEscaped();
        if (breakAfter.contains(c) || ++run >= 24) {
            result += zeroWidthSpace;
            run = 0;
        }
    }
    return result;
}

}

QStringList linkWarningReasons(const QUrl &url, const QString &text)
{
    const UiUtils::LinkAssessment assessment = UiUtils::assessLink(url, text);
    QStringList reasons;
    for (const auto warning : assessment.warnings) {
        switch (warning) {
        case UiUtils::LinkWarning::TextShowsOtherSite:
            reasons << i18nc("@info link warning; %1 and %2 are site names",
                             "The link text shows %1, but the link goes to %2.", bold(assessment.textHost),
                             bold(assessment.host));
            break;
        case UiUtils::LinkWarning::UserInfo:
            reasons << i18nc("@info link warning; %1 is a user name, %2 a site name",
                             "The address puts the user name %1 before the site %2, which can make another site "
                             "look like the destination.",
                             bold(url.userName(QUrl::FullyEncoded)), bold(assessment.host));
            if (!url.password().isEmpty())
                reasons << i18nc("@info link warning", "The address also contains a password.");
            break;
        case UiUtils::LinkWarning::IpAddress:
            reasons << i18nc("@info link warning", "The link goes to a numeric address instead of a named site.");
            break;
        case UiUtils::LinkWarning::InternationalHost:
            reasons << i18nc("@info link warning; %1 is a site name in international characters, %2 its ASCII form",
                             "The site name uses international characters, which can imitate another name: %1 is "
                             "written %2.",
                             bold(QUrl::fromAce(assessment.host.toLatin1(), QUrl::IgnoreIDNWhitelist)),
                             bold(assessment.host));
            break;
        case UiUtils::LinkWarning::TextShowsOtherAddress:
            reasons << i18nc("@info link warning; %1 and %2 are e-mail addresses",
                             "The link text shows %1, but the message is written to %2.", bold(assessment.textAddress),
                             bold(assessment.address));
            break;
        }
    }
    return reasons;
}

QString linkTargetDescription(const QUrl &url, const QString &text)
{
    QString target = url.toString(QUrl::FullyEncoded);
    if (target.size() > maxShownLength) {
        const qsizetype half = maxShownLength / 2;
        target = target.left(half) + QChar(0x2026) + target.right(half);
    }
    QString result = QStringLiteral("<p><b>%1</b><br/>%2</p>")
                         .arg(i18nc("@info heading above a link's address", "Link target").toHtmlEscaped(),
                              breakable(target));
    const QStringList reasons = linkWarningReasons(url, text);
    if (!reasons.isEmpty()) {
        result += QStringLiteral("<p>%1</p><ul><li>%2</li></ul>")
                      .arg(i18nc("@info", "This link may not go where it appears to:").toHtmlEscaped(),
                           reasons.join(QLatin1String("</li><li>")));
    }
    return result;
}

}
