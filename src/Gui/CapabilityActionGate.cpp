/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CapabilityActionGate.h"

#include <QAction>

namespace Gui {

CapabilityActionGate::CapabilityActionGate(QObject *parent) : QObject(parent) {}

void CapabilityActionGate::addAction(QAction *action,
                                     Imap::AccountOperation operation) {
  if (action)
    m_entries.append(Entry{action, operation, false});
}

void CapabilityActionGate::apply(
    const Imap::AccountCapabilities &capabilities) {
  for (Entry &entry : m_entries) {
    if (!entry.action)
      continue;
    const bool supported = capabilities.supports(entry.operation);
    if (!supported && !entry.hidden) {
      entry.hidden = entry.action->isVisible();
      entry.action->setVisible(false);
    } else if (supported && entry.hidden) {
      entry.hidden = false;
      entry.action->setVisible(true);
    }
  }
}

bool CapabilityActionGate::isHiddenByGate(const QAction *action) const {
  for (const Entry &entry : m_entries) {
    if (entry.action == action && entry.hidden)
      return true;
  }
  return false;
}

} // namespace Gui
