/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "EmbeddedPanel.h"

class QFormLayout;
class QLabel;
class QScrollArea;

namespace Gui {

/** @short Embedded panel with the metadata of the message shown in the reader

Shows the subject, correspondents, date, size, folder, status flags and whether the
message uses MIME-level signing or encryption. Rows without a value are hidden.
*/
class MessageDetailsPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit MessageDetailsPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    bool hasContent() const override;
    QWidget *focusTarget() const override;
    QSize sizeHint() const override;

    /** The value shown for a field such as "Subject" or "Folder", or an empty string when the row is hidden */
    QString fieldText(const QString &field) const;

protected:
    void refresh() override;

private:
    QLabel *addField(const QString &field, const QString &label);
    void setField(QLabel *value, const QString &text);

    QScrollArea *m_scroll;
    QFormLayout *m_form;
    QLabel *m_subject;
    QLabel *m_from;
    QLabel *m_to;
    QLabel *m_cc;
    QLabel *m_date;
    QLabel *m_size;
    QLabel *m_folder;
    QLabel *m_status;
    QLabel *m_security;
    bool m_hasContent = false;
};

}
