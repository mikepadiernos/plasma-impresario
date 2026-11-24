/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_RELATEDMESSAGES_H
#define TEST_RELATEDMESSAGES_H

#include <QObject>

/** @short Verify how the related messages panel groups the loaded rows of a message list */
class RelatedMessagesTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesSubjects();
    void readsFirstAddress();
    void needsFetchedCurrentMessage();
    void groupsConversationAndSender();
    void linksThroughMessageIds();
    void skipsUnfetchedRows();
    void honoursLimits();
};

#endif
