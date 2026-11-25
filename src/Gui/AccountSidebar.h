/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_ACCOUNTSIDEBAR_H
#define GUI_ACCOUNTSIDEBAR_H

#include <QList>
#include <QWidget>

class QToolButton;
class QVBoxLayout;

namespace Common {
class AccountRegistry;
class AccountStatusTracker;
} // namespace Common

namespace Gui {

/** @short The folder sidebar, grouped by account

With two or more accounts, each account has a header row with its name and
status. The active account's header is expanded and followed by its folder tree;
activating another header switches to that account, and activating the active
one collapses or expands its folders. With a single account the headers are
hidden and the sidebar shows only the folder tree, as before.

The headers take keyboard focus: Up and Down move between them, and Return,
Enter or Space activates one.
*/
class AccountSidebar : public QWidget {
  Q_OBJECT
public:
  /** @param folders the folder tree of the active account, which the sidebar
   * takes over */
  explicit AccountSidebar(QWidget *folders, QWidget *parent = nullptr);

  /** Rebuild the headers from the account list, their status and the active
   * account */
  void refresh(const Common::AccountRegistry &accounts,
               const Common::AccountStatusTracker &status);

  /** The folder tree passed to the constructor */
  QWidget *folders() const { return m_folders; }

  /** The header rows, one per account in account order; empty while they are
   * hidden */
  QList<QToolButton *> headers() const;

  /** Whether the active account's folders are shown */
  bool foldersExpanded() const { return m_expanded; }

signals:
  /** The user activated the header of another account */
  void accountActivated(const QString &accountId);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void activateHeader(QToolButton *header);
  void setFoldersExpanded(bool expanded);
  void updateHeaderStates();

  QWidget *m_folders;
  QVBoxLayout *m_layout;
  QList<QToolButton *> m_headers;
  QString m_activeAccount;
  bool m_expanded = true;
};

} // namespace Gui

#endif
