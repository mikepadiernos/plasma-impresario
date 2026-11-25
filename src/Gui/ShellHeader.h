/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef GUI_SHELLHEADER_H
#define GUI_SHELLHEADER_H

#include <KSharedConfig>
#include <QWidget>

class QAction;
class QLabel;

namespace Common {
class AccountRegistry;
class AccountStatusTracker;
} // namespace Common

namespace Gui {

/** @short The header region at the top of the shell

The header completes the five shell regions of the design: header, left column,
main column, right column and footer. It shows where the user is, the active
account and folder, and the account's status while it needs attention. It is off
by default and is turned on with View › Show Header; the choice is saved as
`ShowHeader` in the `Appearance` group of @p config. The designed header with
search and compose comes later.
*/
class ShellHeader : public QWidget {
  Q_OBJECT
public:
  explicit ShellHeader(KSharedConfig::Ptr config, QWidget *parent = nullptr);

  /** The checkable View › Show Header action */
  QAction *toggleAction() const { return m_toggle; }

  /** Show the active account and its status */
  void setAccount(const Common::AccountRegistry &accounts,
                  const Common::AccountStatusTracker &status);

  /** Show the selected folder; an empty name means that no folder is selected
   */
  void setFolder(const QString &folder);

  /** The widget which takes focus when the header region is entered */
  QWidget *focusTarget() const;

  /** The text describing the account and folder */
  QString contextText() const;

  /** The account's status text, empty while the account needs no attention */
  QString statusText() const;

private:
  void updateContext();

  KSharedConfig::Ptr m_config;
  QAction *m_toggle;
  QLabel *m_context;
  QLabel *m_statusIcon;
  QLabel *m_status;
  QString m_account;
  QString m_folder;
};

} // namespace Gui

#endif
