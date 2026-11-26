/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_MAILINGLISTS_H
#define TEST_MAILINGLISTS_H

#include <QObject>

/** @short Verify how mailing lists are recognized from List-Id and List-Post, and which list actions are offered */
class MailingListsTest : public QObject
{
    Q_OBJECT

private slots:
    void readsListHeaders();
    void parsesListId_data();
    void parsesListId();
    void readsHeaderUrls();
    void picksUsableUrls();
    void recognizesListsOfMessages();
    void offersListActions();
    void findsListsInModel();
    void searchesListIdAndListPost();
};

#endif
