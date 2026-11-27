/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AccountNotices.h"

#include <QMetaObject>

#include <utility>

namespace UiUtils {

namespace {

void closeNotice(QObject *notice)
{
    if (notice)
        QMetaObject::invokeMethod(notice, "close");
}

}

AccountNotices::AccountNotices(QObject *parent)
    : QObject(parent)
{
}

AccountNotices::~AccountNotices()
{
    closeAll();
}

void AccountNotices::show(const QString &accountId, QObject *notice)
{
    const QPointer<QObject> previous = m_notices.value(accountId);
    m_notices.insert(accountId, notice);
    if (previous != notice)
        closeNotice(previous);
}

void AccountNotices::close(const QString &accountId)
{
    closeNotice(m_notices.take(accountId));
}

void AccountNotices::closeAll()
{
    const auto notices = std::exchange(m_notices, {});
    for (const QPointer<QObject> &notice : notices)
        closeNotice(notice);
}

bool AccountNotices::isShown(const QString &accountId) const
{
    return !m_notices.value(accountId).isNull();
}

}
