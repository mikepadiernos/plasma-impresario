/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>

namespace UiUtils {

/** @short Keep at most one desktop notification for each account

A newer notification replaces the account's older one, and a notification is closed once it no longer applies:
when the account connects again, is shown in the window or is removed. A notification must have a close() slot;
KNotification has one and deletes itself once closed.
*/
class AccountNotices : public QObject
{
    Q_OBJECT
public:
    explicit AccountNotices(QObject *parent = nullptr);
    ~AccountNotices() override;

    /** Track @arg notice as the account's notification, closing the one shown before */
    void show(const QString &accountId, QObject *notice);

    /** Close the account's notification, if it is still shown */
    void close(const QString &accountId);

    /** Close every account's notification */
    void closeAll();

    /** Whether the account has a notification which is still shown */
    bool isShown(const QString &accountId) const;

private:
    QHash<QString, QPointer<QObject>> m_notices;
};

}
