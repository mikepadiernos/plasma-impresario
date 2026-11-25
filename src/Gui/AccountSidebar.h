/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_ACCOUNTSIDEBAR_H
#define GUI_ACCOUNTSIDEBAR_H

#include <QHash>
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

A header also shows how many unread messages the account's INBOX holds, once
the count is known; see setUnreadCount().

The headers take keyboard focus: Up and Down move between them, and Return,
Enter or Space activates one. A header's context menu, by mouse or the Menu
key, is requested through accountMenuRequested().
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

  /** Show @arg count unread messages in the INBOX of @arg accountId; a
   * negative count means that the count is not known */
  void setUnreadCount(const QString &accountId, int count);

  /** The unread count shown for @arg accountId, or -1 if none is known */
  int unreadCount(const QString &accountId) const {
    return m_unread.value(accountId, -1);
  }

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
  /** The user asked for the context menu of an account's header, at
   * @arg globalPos */
  void accountMenuRequested(const QString &accountId, const QPoint &globalPos);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void activateHeader(QToolButton *header);
  void setFoldersExpanded(bool expanded);
  void updateHeaderStates();
  void updateHeaderText(QToolButton *header);

  QWidget *m_folders;
  QVBoxLayout *m_layout;
  QList<QToolButton *> m_headers;
  QString m_activeAccount;
  QHash<QString, int> m_unread;
  bool m_expanded = true;
};

} // namespace Gui

#endif
