/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QPointer>
#include <QTimer>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;

namespace Imap {
namespace Mailbox {
class SearchIndexer;
}
}

namespace Gui {

/** @short Shows how far an account's offline search index covers its mail, and pauses, resumes or rebuilds it

The folders listed are those of the offline cache, which is what the index is meant to cover; each says whether it is
confirmed as indexed, so that folders which are only set to be indexed are not mistaken for searchable ones. A warning
tells when offline searches may miss messages.

The dialog does not keep the pause; pausedChanged() lets the owner remember it.
*/
class SearchIndexDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchIndexDialog(QWidget *parent = nullptr);

    /** @short Show the index of @arg indexer, of the account called @arg accountName; nullptr when it is off */
    void setIndexer(Imap::Mailbox::SearchIndexer *indexer, const QString &accountName);
    Imap::Mailbox::SearchIndexer *indexer() const;

    /** @short Read the state of the index again */
    void refresh();

signals:
    /** @short The user paused or resumed the index */
    void pausedChanged(bool paused);

private:
    void togglePaused();
    void askRebuild();

    QPointer<Imap::Mailbox::SearchIndexer> m_indexer;
    QString m_accountName;
    QLabel *m_state;
    QProgressBar *m_progress;
    QLabel *m_summary;
    QLabel *m_warningIcon;
    QLabel *m_warning;
    QTreeWidget *m_folders;
    QPushButton *m_pause;
    QPushButton *m_rebuild;
    QTimer m_refresh;
    QList<QMetaObject::Connection> m_connections;
};

}
