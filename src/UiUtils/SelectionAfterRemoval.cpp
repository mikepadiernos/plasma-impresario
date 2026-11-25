/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SelectionAfterRemoval.h"

namespace UiUtils {
namespace SelectionAfterRemoval {

QModelIndex successor(const QModelIndex &current, const Step &below, const Step &above, const Test &remains,
                      const Test &pending)
{
    if (!current.isValid() || !below || !above || !remains)
        return QModelIndex();

    for (const bool skipPending : {true, false}) {
        if (!skipPending && !pending)
            break;
        for (const bool forward : {true, false}) {
            const Step &step = forward ? below : above;
            for (QModelIndex walker = step(current); walker.isValid(); walker = step(walker)) {
                if (!remains(walker))
                    continue;
                if (skipPending && pending && pending(walker))
                    continue;
                return walker;
            }
        }
    }
    return QModelIndex();
}

bool isWithinRemovedRows(const QModelIndex &index, const QModelIndex &parent, const int first, const int last)
{
    for (QModelIndex walker = index; walker.isValid(); walker = walker.parent()) {
        if (walker.parent() == parent && walker.row() >= first && walker.row() <= last)
            return true;
    }
    return false;
}

}
}
