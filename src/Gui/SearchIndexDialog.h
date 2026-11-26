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
class QTreeWidgetItem;

namespace Imap {
namespace Mailbox {
class SearchIndexer;
}
}

namespace Gui {

/** @short Shows how far an account's offline search index covers its mail, and pauses, resumes, rebuilds or deletes it

The folders listed are those of the offline cache, which is what the index is meant to cover; each says whether it is
confirmed as indexed, so that folders which are only set to be indexed are not mistaken for searchable ones. A warning
tells when offline searches may miss messages. Unchecking a folder keeps it out of the index.

The dialog does not keep the pause or the excluded folders, and does not delete the index itself; pausedChanged(),
excludedFoldersChanged() and deleteRequested() let the owner do so.
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
    /** @short Say that the index was deleted, until another index is shown */
    void showDeleted();

signals:
    /** @short The user paused or resumed the index */
    void pausedChanged(bool paused);
    /** @short The user chose the folders to keep out of the index; the indexer already skips them */
    void excludedFoldersChanged(const QStringList &folders);
    /** @short The user confirmed that the index is to be deleted */
    void deleteRequested();

private:
    void togglePaused();
    void askRebuild();
    void askDelete();
    void folderChanged(QTreeWidgetItem *item, int column);

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
    QPushButton *m_delete;
    QTimer m_refresh;
    QList<QMetaObject::Connection> m_connections;
    bool m_filling = false;
    bool m_deleted = false;
};

}
