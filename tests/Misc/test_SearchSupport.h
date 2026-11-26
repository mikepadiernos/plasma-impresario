/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_SEARCHSUPPORT_H
#define TEST_SEARCHSUPPORT_H

#include <QObject>

/** @short Verify which search keys need an extension, and how a refused search is explained */
class SearchSupportTest : public QObject
{
    Q_OBJECT

private slots:
    void findsUnsupportedKeys_data();
    void findsUnsupportedKeys();
    void detectsTextBeyondAscii();
    void explainsRejections_data();
    void explainsRejections();
    void explainsUnsupportedKeys();
};

#endif
