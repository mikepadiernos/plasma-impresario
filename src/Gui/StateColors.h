/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_STATECOLORS_H
#define GUI_STATECOLORS_H

#include <QColor>

class QWidget;

namespace Gui {

/** @short Colors for positive, neutral and negative states from Plasma's active color scheme

These follow the user's color scheme through KColorScheme, so that a failed search or an
invalid address looks the same as in other KDE applications, in light and dark schemes.
*/
namespace StateColors {

enum class State {
    Normal,
    Positive,
    Neutral,
    Negative,
};

/** @short Tint the base of an input field for @arg state; Normal restores the inherited palette */
void applyToField(QWidget *field, State state);

/** @short The scheme's background for @arg state in the view color set */
QColor fieldBackground(State state);

/** @short The scheme's text color for @arg state in the window color set */
QColor text(State state);

/** @short Style sheet for a warning label: a negative border on a neutral background */
QString warningStyleSheet();

}

}

#endif
