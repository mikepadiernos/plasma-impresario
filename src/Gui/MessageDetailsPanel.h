/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPointer>
#include <functional>

#include "EmbeddedPanel.h"
#include "UiUtils/MessageSecurity.h"

class QAbstractItemModel;
class QFormLayout;
class QLabel;
class QScrollArea;

namespace Gui {

/** @short Embedded panel with the metadata of the message shown in the reader

Shows the subject, correspondents, date, size, folder, account, status flags and the message's
security. The security comes from the reader's signature verification and decryption
when its part model is set through setMessageModel(), and otherwise from the MIME
structure, which is reported as not verified. Rows without a value are hidden.
*/
class MessageDetailsPanel : public MessageContextPanel
{
    Q_OBJECT
public:
    explicit MessageDetailsPanel(QWidget *parent = nullptr);

    static Descriptor panelDescriptor();

    bool hasContent() const override;
    QWidget *focusTarget() const override;

    /** The value shown for a field such as "Subject" or "Folder", or an empty string when the row is hidden */
    QString fieldText(const QString &field) const;

    /** The security state currently shown */
    UiUtils::MessageSecurity security() const;

    /** Text and theme icon name describing a security state; the text is empty for unprotected mail */
    static QString securityText(const UiUtils::MessageSecurity &security);
    static QString securityIconName(const UiUtils::MessageSecurity &security);

    /** @short Name the account of a message in the Account row; an empty name hides the row */
    void setAccountResolver(const std::function<QString(const QModelIndex &)> &resolver);

public slots:
    /** Follow the reader's part model (Cryptography::MessageModel) for verification and decryption results */
    void setMessageModel(QAbstractItemModel *model);

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
    QLabel *m_account;
    QLabel *m_status;
    QWidget *m_securityRow;
    QLabel *m_securityIcon;
    QLabel *m_security;
    UiUtils::MessageSecurity m_securityState;
    QPointer<QAbstractItemModel> m_partModel;
    QList<QMetaObject::Connection> m_partModelConnections;
    std::function<QString(const QModelIndex &)> m_accountName;
    bool m_hasContent = false;
};

}
