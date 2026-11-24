/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TEST_SPECIALFOLDER_H
#define TEST_SPECIALFOLDER_H

#include <QObject>

/** @short Verify the special-use folder classification */
class SpecialFolderTest : public QObject
{
    Q_OBJECT

private slots:
    void attributes_data();
    void attributes();
    void names_data();
    void names();
    void inboxIsAlwaysInbox();
    void attributeBeatsName();
    void attributeSuppressesNameGuess();
    void firstNameMatchWins();
    void configuredArchive();
    void configuredArchiveSuppressesArchiveGuess();
    void archiveAttributeAndConfiguredArchive();
    void nonSelectableIsNotATarget();
    void rankAndNavigableKinds();
};

#endif
