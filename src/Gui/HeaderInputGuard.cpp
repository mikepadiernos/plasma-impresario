/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "HeaderInputGuard.h"

#include <QLineEdit>

#include <algorithm>

#include <KLocalizedString>

#include "Composer/HeaderSafety.h"

namespace Gui {

HeaderInputGuard::HeaderInputGuard(QObject *parent)
    : QObject(parent)
{
}

void HeaderInputGuard::watch(QLineEdit *edit, const QString &fieldName)
{
    connect(edit, &QLineEdit::textEdited, this, [this, edit, fieldName]() { clean(edit, fieldName); });
}

QString HeaderInputGuard::noticeText(const QString &fieldName)
{
    return i18nc("@info %1 is the name of a field, such as Subject",
        "Line breaks and other control characters entered in %1 were "
        "replaced with spaces.",
        fieldName);
}

void HeaderInputGuard::clean(QLineEdit *edit, const QString &fieldName)
{
    const QString text = edit->text();
    if (std::none_of(text.cbegin(), text.cend(), Composer::HeaderSafety::isUnsafe))
        return;
    const int cursor = Composer::HeaderSafety::text(text.left(edit->cursorPosition())).size();
    edit->setText(Composer::HeaderSafety::text(text));
    edit->setCursorPosition(cursor);
    emit cleaned(edit, noticeText(fieldName.isEmpty() ? edit->accessibleName() : fieldName));
}

} // namespace Gui
