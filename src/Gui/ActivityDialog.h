/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QToolButton>

#include "UiUtils/ActivityLog.h"

class QPushButton;
class QTreeWidget;

namespace Gui {

/** @short The activity log: what runs in the background, what waits and what failed, newest first

Each entry shows its state, what it is about, its detail or error and when it last changed. An entry which names a fix,
such as signing in or reviewing a rule, offers it as a button; the window carries it out through actionRequested().
*/
class ActivityDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ActivityDialog(UiUtils::ActivityLog *log, QWidget *parent = nullptr);

    QTreeWidget *tree() const;
    /** Carry out the action of the selected entry, as its button does */
    void triggerSelected();

signals:
    void actionRequested(const UiUtils::ActivityLog::Entry &entry);

private:
    void rebuild();
    void updateButtons();

    UiUtils::ActivityLog *m_log;
    QTreeWidget *m_tree;
    QPushButton *m_action;
    QPushButton *m_clear;
};

/** @short A status bar button which shows how much runs and how much failed, and opens the activity log */
class ActivityButton : public QToolButton
{
    Q_OBJECT
public:
    explicit ActivityButton(UiUtils::ActivityLog *log, QWidget *parent = nullptr);

private:
    void refresh();

    UiUtils::ActivityLog *m_log;
};

}
