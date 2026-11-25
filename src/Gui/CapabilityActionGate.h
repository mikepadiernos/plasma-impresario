/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_CAPABILITYACTIONGATE_H
#define GUI_CAPABILITYACTIONGATE_H

#include <QList>
#include <QObject>
#include <QPointer>

#include "Imap/Model/AccountCapabilities.h"

class QAction;

namespace Gui {

/** @short Hides the actions that the active account's protocol cannot perform

Each action is registered with the operation it needs. Applying an account's
capabilities hides the actions whose operation it does not support and shows
them again for an account that does. Only actions hidden here are shown again,
so actions hidden for other reasons stay hidden.
*/
class CapabilityActionGate : public QObject {
  Q_OBJECT
public:
  explicit CapabilityActionGate(QObject *parent = nullptr);

  /** Show @p action only for accounts that support @p operation */
  void addAction(QAction *action, Imap::AccountOperation operation);

  /** Show or hide the registered actions for an account with @p capabilities */
  void apply(const Imap::AccountCapabilities &capabilities);

  /** Whether @p action is currently hidden because the account does not support
   * its operation */
  bool isHiddenByGate(const QAction *action) const;

private:
  struct Entry {
    QPointer<QAction> action;
    Imap::AccountOperation operation;
    bool hidden = false;
  };
  QList<Entry> m_entries;
};

} // namespace Gui

#endif
