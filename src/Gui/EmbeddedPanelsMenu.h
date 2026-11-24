/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>
#include <QMenu>

namespace Gui {

class AttachmentPanel;

/** @short View menu for showing and hiding embedded panels

Preferences are stored in the Appearance group: AttachmentPanelVisible
(default true) and AttachmentPanelAutoHide (default false).
*/
class EmbeddedPanelsMenu : public QMenu
{
    Q_OBJECT
public:
    EmbeddedPanelsMenu(AttachmentPanel *attachments, KSharedConfigPtr config, QWidget *parent = nullptr);

    QAction *attachmentsAction() const;
    QAction *attachmentsAutoHideAction() const;

private:
    void save(const char *key, bool value);

    AttachmentPanel *m_attachments;
    KSharedConfigPtr m_config;
    QAction *m_attachmentsAction;
    QAction *m_attachmentsAutoHideAction;
};

}
