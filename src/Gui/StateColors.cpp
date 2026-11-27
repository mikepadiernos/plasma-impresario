/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "StateColors.h"

#include <QPalette>
#include <QWidget>

#include <KColorScheme>

namespace Gui {

namespace StateColors {

namespace {

KColorScheme::BackgroundRole backgroundRole(State state)
{
    switch (state) {
    case State::Positive:
        return KColorScheme::PositiveBackground;
    case State::Neutral:
        return KColorScheme::NeutralBackground;
    case State::Negative:
        return KColorScheme::NegativeBackground;
    case State::Normal:
        break;
    }
    return KColorScheme::NormalBackground;
}

KColorScheme::ForegroundRole foregroundRole(State state)
{
    switch (state) {
    case State::Positive:
        return KColorScheme::PositiveText;
    case State::Neutral:
        return KColorScheme::NeutralText;
    case State::Negative:
        return KColorScheme::NegativeText;
    case State::Normal:
        break;
    }
    return KColorScheme::NormalText;
}

}

void applyToField(QWidget *field, State state)
{
    if (state == State::Normal) {
        field->setPalette(QPalette());
        return;
    }
    QPalette palette = field->palette();
    KColorScheme::adjustBackground(palette, backgroundRole(state), QPalette::Base, KColorScheme::View);
    field->setPalette(palette);
}

QColor fieldBackground(State state)
{
    return KColorScheme(QPalette::Active, KColorScheme::View).background(backgroundRole(state)).color();
}

QColor text(State state)
{
    return KColorScheme(QPalette::Active, KColorScheme::Window).foreground(foregroundRole(state)).color();
}

QString warningStyleSheet()
{
    const KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    return QStringLiteral("border: 2px solid %1; background-color: %2; color: %3; padding: 5px; margin: 5px; text-align: center;")
        .arg(scheme.foreground(KColorScheme::NegativeText).color().name(),
             scheme.background(KColorScheme::NeutralBackground).color().name(),
             scheme.foreground(KColorScheme::NormalText).color().name());
}

}

}
