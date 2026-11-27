/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_MESSAGEID_H
#define TEST_MESSAGEID_H

#include <QObject>

/** @short Verify reading Message-IDs and mid: URLs, and looking them up without accounts */
class MessageIdTest : public QObject
{
    Q_OBJECT

private slots:
    void parses_data();
    void parses();
    void buildsUrls();
    void searchesTheHeader();
    void comparesIds();
    void lookupWithoutAccounts();
};

#endif
