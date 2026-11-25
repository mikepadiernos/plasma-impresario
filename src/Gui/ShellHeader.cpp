/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShellHeader.h"

#include <QAction>
#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QStyle>

#include <KConfigGroup>
#include <KLocalizedString>

#include "Common/AccountRegistry.h"
#include "Common/AccountStatus.h"

namespace {
const char showHeaderKey[] = "ShowHeader";
}

namespace Gui {

ShellHeader::ShellHeader(KSharedConfig::Ptr config, QWidget *parent)
    : QWidget(parent), m_config(config),
      m_toggle(new QAction(i18nc("@action:inmenu", "Show Header"), this)),
      m_context(new QLabel(this)), m_statusIcon(new QLabel(this)),
      m_status(new QLabel(this)) {
  setObjectName(QStringLiteral("shellHeader"));
  m_toggle->setObjectName(QStringLiteral("showShellHeader"));
  m_toggle->setCheckable(true);

  m_context->setObjectName(QStringLiteral("shellHeaderContext"));
  QFont font = m_context->font();
  font.setBold(true);
  m_context->setFont(font);
  m_context->setTextFormat(Qt::PlainText);
  // Focusable, so that F6 can bring a screen reader here; the text can be
  // selected and copied from the keyboard
  m_context->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                     Qt::TextSelectableByKeyboard);
  m_context->setFocusPolicy(Qt::StrongFocus);
  m_context->setAccessibleName(
      i18nc("@label:accessibility", "Current account and folder"));
  m_status->setObjectName(QStringLiteral("shellHeaderStatus"));
  m_status->setTextFormat(Qt::PlainText);

  auto *layout = new QHBoxLayout(this);
  const int margin = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
  layout->setContentsMargins(margin, margin / 2, margin, margin / 2);
  layout->addWidget(m_context, 1);
  layout->addWidget(m_statusIcon);
  layout->addWidget(m_status);
  m_statusIcon->hide();
  m_status->hide();

  const bool shown = KConfigGroup(m_config, QStringLiteral("Appearance"))
                         .readEntry(showHeaderKey, false);
  m_toggle->setChecked(shown);
  setVisible(shown);
  connect(m_toggle, &QAction::toggled, this, [this](bool checked) {
    setVisible(checked);
    KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    appearance.writeEntry(showHeaderKey, checked);
    if (!m_config->sync())
      qWarning() << "Unable to save the header visibility";
  });
  updateContext();
}

void ShellHeader::setAccount(const Common::AccountRegistry &accounts,
                             const Common::AccountStatusTracker &status) {
  const QString active = accounts.activeAccount();
  m_account = accounts.displayName(active);
  const Common::AccountStatus accountStatus = status.status(active);
  const bool attention =
      Common::AccountStatusTracker::needsAttention(accountStatus.kind);
  if (attention) {
    m_status->setText(Common::AccountStatusTracker::label(accountStatus.kind));
    m_status->setToolTip(accountStatus.detail);
    const int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_statusIcon->setPixmap(
        QIcon::fromTheme(QStringLiteral("dialog-warning-symbolic"),
                         QIcon::fromTheme(QStringLiteral("dialog-warning")))
            .pixmap(size, size));
  } else {
    m_status->clear();
    m_status->setToolTip(QString());
  }
  m_status->setVisible(attention);
  m_statusIcon->setVisible(attention);
  updateContext();
}

void ShellHeader::setFolder(const QString &folder) {
  m_folder = folder;
  updateContext();
}

QWidget *ShellHeader::focusTarget() const { return m_context; }

QString ShellHeader::contextText() const { return m_context->text(); }

QString ShellHeader::statusText() const { return m_status->text(); }

void ShellHeader::updateContext() {
  QString text;
  if (m_folder.isEmpty())
    text = m_account;
  else if (m_account.isEmpty())
    text = m_folder;
  else
    text = i18nc("@label account name, then the folder shown from it",
                 "%1 › %2", m_account, m_folder);
  m_context->setText(text);
  m_context->setAccessibleDescription(
      statusText().isEmpty() ? text
                             : text + QLatin1String(", ") + statusText());
}

} // namespace Gui
