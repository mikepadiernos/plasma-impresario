/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "EmbeddedPanelsMenu.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QDebug>

#include "AttachmentPanel.h"

namespace Gui {

EmbeddedPanelsMenu::EmbeddedPanelsMenu(AttachmentPanel *attachments, KSharedConfigPtr config, QWidget *parent)
    : QMenu(parent)
    , m_attachments(attachments)
    , m_config(config)
{
    setObjectName(QStringLiteral("embeddedPanelsMenu"));
    setTitle(i18nc("@title:menu", "Embedded Panels"));

    m_attachmentsAction = addAction(i18nc("@option:check embedded panel", "Attachments"));
    m_attachmentsAction->setObjectName(QStringLiteral("embeddedPanelAttachments"));
    m_attachmentsAction->setCheckable(true);
    m_attachmentsAutoHideAction = addAction(i18nc("@option:check", "Hide Attachments Panel When Empty"));
    m_attachmentsAutoHideAction->setObjectName(QStringLiteral("embeddedPanelAttachmentsAutoHide"));
    m_attachmentsAutoHideAction->setCheckable(true);
    m_attachmentsAutoHideAction->setToolTip(
        i18nc("@info:tooltip", "Show the attachments panel only while the current message has attachments"));

    const KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    const bool visible = appearance.readEntry("AttachmentPanelVisible", true);
    const bool autoHide = appearance.readEntry("AttachmentPanelAutoHide", false);
    m_attachmentsAction->setChecked(visible);
    m_attachmentsAutoHideAction->setChecked(autoHide);
    m_attachmentsAutoHideAction->setEnabled(visible);
    m_attachments->setAutoHide(autoHide);
    m_attachments->setPanelEnabled(visible);

    connect(m_attachmentsAction, &QAction::toggled, this, [this](bool checked) {
        m_attachments->setPanelEnabled(checked);
        m_attachmentsAutoHideAction->setEnabled(checked);
        save("AttachmentPanelVisible", checked);
    });
    connect(m_attachmentsAutoHideAction, &QAction::toggled, this, [this](bool checked) {
        m_attachments->setAutoHide(checked);
        save("AttachmentPanelAutoHide", checked);
    });
}

QAction *EmbeddedPanelsMenu::attachmentsAction() const
{
    return m_attachmentsAction;
}

QAction *EmbeddedPanelsMenu::attachmentsAutoHideAction() const
{
    return m_attachmentsAutoHideAction;
}

void EmbeddedPanelsMenu::save(const char *key, bool value)
{
    KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    appearance.writeEntry(key, value);
    if (!m_config->sync())
        qWarning() << "Unable to save embedded panel preference" << key;
}

}
