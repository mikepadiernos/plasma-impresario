/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <optional>

namespace UiUtils {

/** @short Checks for the optional app-local Qt Style Sheet

Qt Style Sheets are a CSS-like subset which styles the application's widgets,
never HTML message content. The checks bound their size and refuse what could
load files or change how widgets behave rather than how they look, without
asking Qt to parse the sheet.
*/
namespace StyleSheets {

/** @short Larger stylesheets are refused, in characters and in file bytes */
inline constexpr qsizetype MaximumSize = 64 * 1024;
inline constexpr int MaximumRules = 2000;

/** @short An error naming the line, or an empty string when @arg sheet can be
applied

Refused are sheets over MaximumSize or MaximumRules, unbalanced braces,
parentheses, comments and strings, nested rules, control characters, at-rules
such as @import, qproperty- declarations and any url() other than Qt resources
(":/" or "qrc:/").
*/
QString check(const QString &sheet);

/** @short The UTF-8 text of the stylesheet file at @arg path, of at most
 * MaximumSize bytes */
std::optional<QString> readFile(const QString &path, QString *error);
/** @short Write @arg sheet as UTF-8; an error, or an empty string */
QString writeFile(const QString &path, const QString &sheet);

} // namespace StyleSheets

} // namespace UiUtils
