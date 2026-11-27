/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>

#include <functional>

#include "UiUtils/FileSearch.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace Gui {

/** @short Find attachments and messages which the user saved, in the default file manager

The dialog searches a folder, by default the one in which the user last saved an attachment or a message. It explains
how Plasma's File Search (Baloo) covers that folder: when it does, Search Index opens a baloosearch: URL in the default
file manager, such as Plasma Opus; when it does not, or only indexes file names, a live search through the folder is
offered instead. Without the live search worker, the folder itself opens. Nothing is sent to Baloo.
*/
class SavedFilesSearchDialog : public QDialog
{
    Q_OBJECT
public:
    using UrlOpener = std::function<void(const QUrl &)>;

    explicit SavedFilesSearchDialog(QWidget *parent = nullptr);

    void setSearchText(const QString &text);
    QString searchText() const;
    void setFolder(const QString &folder);
    QString folder() const;
    UiUtils::FileSearch::Status status() const;
    /** The explanation of the folder's coverage which the dialog shows */
    QString statusText() const;

    /** Whether the baloosearch: and filenamesearch: KIO workers are installed; detected by default */
    void setSearchWorkers(bool indexedSearch, bool liveSearch);
    /** How URLs are opened; by default in the application registered for folders */
    void setUrlOpener(const UrlOpener &opener);

public slots:
    void searchIndex();
    void searchLive();

protected:
    bool event(QEvent *event) override;

private:
    void refresh();
    void chooseFolder();
    void open(const QUrl &url);
    QString title() const;

    QLineEdit *m_text;
    QLineEdit *m_folder;
    QPushButton *m_chooseFolder;
    QLabel *m_status;
    QPushButton *m_indexed;
    QPushButton *m_live;
    bool m_hasIndexedSearch;
    bool m_hasLiveSearch;
    UiUtils::FileSearch::Status m_coverage;
    UrlOpener m_opener;
};

}
