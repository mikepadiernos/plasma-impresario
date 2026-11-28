/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>

class QLineEdit;

namespace Gui {

/** @short Clean line breaks and control characters out of a header field as
soon as they are entered

The message is cleaned again when it is built. Cleaning the field right away
shows what will be sent, and cleaned() lets the window say why the text changed.
Text set by the program is left alone.
*/
class HeaderInputGuard : public QObject
{
    Q_OBJECT
public:
    explicit HeaderInputGuard(QObject *parent = nullptr);

    /** @short Clean what the user enters in @arg edit, which @arg fieldName names
    in the notice

    Without a field name, the accessible name of @arg edit at the time of cleaning
    is used.
    */
    void watch(QLineEdit *edit, const QString &fieldName = QString());

    /** @short The notice shown after cleaning the field named @arg fieldName */
    static QString noticeText(const QString &fieldName);

signals:
    void cleaned(QLineEdit *edit, const QString &notice);

private:
    void clean(QLineEdit *edit, const QString &fieldName);
};

} // namespace Gui
