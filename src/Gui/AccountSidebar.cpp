/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "AccountSidebar.h"

#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Common/AccountRegistry.h"
#include "Common/AccountStatus.h"

namespace {
const char accountIdProperty[] = "accountId";
const char attentionProperty[] = "accountNeedsAttention";

QString headerAccount(const QToolButton *header) {
  return header->property(accountIdProperty).toString();
}
} // namespace

namespace Gui {

AccountSidebar::AccountSidebar(QWidget *folders, QWidget *parent)
    : QWidget(parent), m_folders(folders), m_layout(new QVBoxLayout(this)) {
  setObjectName(QStringLiteral("accountSidebar"));
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);
  m_layout->addWidget(m_folders, 1);
  // Focus entering the sidebar goes to the folders, as it did before the
  // sidebar wrapped them
  setFocusProxy(m_folders);
}

QList<QToolButton *> AccountSidebar::headers() const {
  QList<QToolButton *> shown;
  for (QToolButton *header : m_headers) {
    if (!header->isHidden())
      shown.append(header);
  }
  return shown;
}

void AccountSidebar::refresh(const Common::AccountRegistry &accounts,
                             const Common::AccountStatusTracker &status) {
  const QStringList ids = accounts.accountIds();
  const QString active = accounts.activeAccount();
  if (active != m_activeAccount)
    m_expanded = true;
  m_activeAccount = active;

  // Keep existing headers, so that a status change does not take focus away
  // from them
  QList<QToolButton *> headers;
  QList<QToolButton *> unused = m_headers;
  for (const QString &id : ids) {
    QToolButton *header = nullptr;
    for (QToolButton *candidate : std::as_const(unused)) {
      if (headerAccount(candidate) == id) {
        header = candidate;
        break;
      }
    }
    if (header) {
      unused.removeOne(header);
    } else {
      header = new QToolButton(this);
      header->setObjectName(QStringLiteral("accountHeader"));
      header->setProperty(accountIdProperty, id);
      header->setAutoRaise(true);
      header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
      header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      header->setFocusPolicy(Qt::StrongFocus);
      header->installEventFilter(this);
      header->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(header, &QToolButton::clicked, this,
              [this, header]() { activateHeader(header); });
      connect(header, &QWidget::customContextMenuRequested, this,
              [this, header](const QPoint &pos) {
                emit accountMenuRequested(headerAccount(header),
                                          header->mapToGlobal(pos));
              });
    }
    const Common::AccountStatus accountStatus = status.status(id);
    const bool attention =
        Common::AccountStatusTracker::needsAttention(accountStatus.kind);
    const QString name = accounts.displayName(id);
    header->setText(name);
    header->setAccessibleName(name);
    header->setProperty(attentionProperty, attention);
    QString tip = name;
    if (attention) {
      tip = i18nc("@info:tooltip account name and its status", "%1: %2", name,
                  Common::AccountStatusTracker::label(accountStatus.kind));
      if (!accountStatus.detail.isEmpty())
        tip += QLatin1String(": ") + accountStatus.detail;
    }
    header->setToolTip(tip);
    headers.append(header);
  }
  for (QToolButton *header : std::as_const(unused))
    header->deleteLater();
  m_headers = headers;

  while (QLayoutItem *item = m_layout->takeAt(0))
    delete item;
  const bool grouped = m_headers.size() > 1;
  bool afterActive = false;
  for (QToolButton *header : std::as_const(m_headers)) {
    header->setVisible(grouped);
    if (!grouped)
      continue;
    m_layout->addWidget(header);
    if (headerAccount(header) == m_activeAccount) {
      m_layout->addWidget(m_folders, 1);
      afterActive = true;
    }
  }
  if (!afterActive)
    m_layout->addWidget(m_folders, 1);
  // Keeps the headers together at the top while the folders are collapsed
  m_layout->addStretch(0);
  setFoldersExpanded(grouped ? m_expanded : true);
}

void AccountSidebar::activateHeader(QToolButton *header) {
  const QString id = headerAccount(header);
  if (id == m_activeAccount) {
    setFoldersExpanded(!m_expanded);
    return;
  }
  emit accountActivated(id);
}

void AccountSidebar::setFoldersExpanded(bool expanded) {
  m_expanded = expanded;
  m_folders->setVisible(expanded);
  m_layout->setStretch(m_layout->count() - 1, expanded ? 0 : 1);
  updateHeaderStates();
}

void AccountSidebar::updateHeaderStates() {
  for (QToolButton *header : std::as_const(m_headers)) {
    const bool open = headerAccount(header) == m_activeAccount && m_expanded;
    const bool attention = header->property(attentionProperty).toBool();
    QIcon icon;
    if (attention)
      icon =
          QIcon::fromTheme(QStringLiteral("dialog-warning-symbolic"),
                           QIcon::fromTheme(QStringLiteral("dialog-warning")));
    else if (open)
      icon = QIcon::fromTheme(QStringLiteral("go-down-symbolic"),
                              QIcon::fromTheme(QStringLiteral("go-down")));
    else
      icon = QIcon::fromTheme(QStringLiteral("go-next-symbolic"),
                              QIcon::fromTheme(QStringLiteral("go-next")));
    header->setIcon(icon);
    header->setAccessibleDescription(
        open ? i18nc("@info:accessibility account folders", "Folders shown")
             : i18nc("@info:accessibility account folders", "Folders hidden"));
  }
}

bool AccountSidebar::eventFilter(QObject *watched, QEvent *event) {
  auto *header = qobject_cast<QToolButton *>(watched);
  if (header && m_headers.contains(header) &&
      event->type() == QEvent::KeyPress) {
    const int key = static_cast<QKeyEvent *>(event)->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
      activateHeader(header);
      return true;
    }
    if (key == Qt::Key_Up || key == Qt::Key_Down) {
      const QList<QToolButton *> shown = headers();
      const int index = shown.indexOf(header) + (key == Qt::Key_Up ? -1 : 1);
      if (index >= 0 && index < shown.size())
        shown.at(index)->setFocus(Qt::OtherFocusReason);
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

} // namespace Gui
