/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QModelIndex>
#include <functional>

namespace UiUtils {

/** @short Choosing the message which becomes current when the current one leaves the list

The list is walked in its visual order through two step functions, so the same rules apply to flat lists,
threads and sorted or filtered views.
*/
namespace SelectionAfterRemoval {

using Step = std::function<QModelIndex(const QModelIndex &)>;
using Test = std::function<bool(const QModelIndex &)>;

/** @short The row which should become current once @arg current is gone

@arg below and @arg above return the next row in the respective direction, or an invalid index at either end.
@arg remains tells whether a row is still a message after the removal. Rows for which @arg pending returns
true, typically the other selected messages which are on their way out as well, are only used when nothing
else remains.

The first remaining row after @arg current wins; at the end of the list it is the nearest remaining row above.
An invalid index is returned when nothing remains.
*/
QModelIndex successor(const QModelIndex &current, const Step &below, const Step &above, const Test &remains,
                      const Test &pending = Test());

/** @short Whether @arg index, or a row which contains it, is one of the rows @arg first to @arg last of @arg parent */
bool isWithinRemovedRows(const QModelIndex &index, const QModelIndex &parent, int first, int last);

}

}
