/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SavedFilesSearchDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <KConfig>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KProtocolInfo>

#include "Gui/FileChooser.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace {

using UiUtils::FileSearch::Coverage;

QString displayedPath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

}

SavedFilesSearchDialog::SavedFilesSearchDialog(QWidget *parent)
    : QDialog(parent)
    , m_text(new QLineEdit(this))
    , m_folder(new QLineEdit(this))
    , m_chooseFolder(new QPushButton(UiUtils::loadIcon(QStringLiteral("document-open-folder")),
                                     i18nc("@action:button", "Choose…"), this))
    , m_status(new QLabel(this))
    , m_indexed(new QPushButton(UiUtils::loadIcon(QStringLiteral("system-search")),
                                i18nc("@action:button search Plasma's file index", "Search Index"), this))
    , m_live(new QPushButton(this))
    , m_hasIndexedSearch(KProtocolInfo::isKnownProtocol(QStringLiteral("baloosearch")))
    , m_hasLiveSearch(KProtocolInfo::isKnownProtocol(QStringLiteral("filenamesearch")))
{
    setWindowTitle(i18nc("@title:window", "Find Saved Files"));
    m_text->setObjectName(QStringLiteral("savedFilesText"));
    m_text->setClearButtonEnabled(true);
    m_text->setPlaceholderText(i18nc("@info:placeholder", "Part of the name or text of a file"));
    m_folder->setObjectName(QStringLiteral("savedFilesFolder"));
    m_folder->setAccessibleName(i18nc("@label:textbox accessible name", "Folder to search"));
    m_folder->setClearButtonEnabled(true);
    m_chooseFolder->setObjectName(QStringLiteral("savedFilesChooseFolder"));
    m_status->setObjectName(QStringLiteral("savedFilesStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_indexed->setObjectName(QStringLiteral("savedFilesSearchIndex"));
    m_indexed->setToolTip(i18nc("@info:tooltip", "Find the files with Plasma's File Search in your file manager"));
    m_live->setObjectName(QStringLiteral("savedFilesSearchLive"));

    auto *folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(m_chooseFolder);
    auto *form = new QFormLayout;
    form->addRow(i18nc("@label:textbox", "&Find:"), m_text);
    form->addRow(i18nc("@label:textbox the folder to search", "&In:"), folderRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_indexed, QDialogButtonBox::ActionRole);
    buttons->addButton(m_live, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(m_text, &QLineEdit::textChanged, this, &SavedFilesSearchDialog::refresh);
    connect(m_folder, &QLineEdit::textChanged, this, &SavedFilesSearchDialog::refresh);
    connect(m_chooseFolder, &QPushButton::clicked, this, &SavedFilesSearchDialog::chooseFolder);
    connect(m_indexed, &QPushButton::clicked, this, &SavedFilesSearchDialog::searchIndex);
    connect(m_live, &QPushButton::clicked, this, &SavedFilesSearchDialog::searchLive);

    m_opener = [this](const QUrl &url) {
        // Searches and folders both open in the application registered for folders, such as Plasma Opus
        auto *job = new KIO::OpenUrlJob(url, QStringLiteral("inode/directory"));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, parentWidget()));
        job->start();
    };
    refresh();
    resize(qMax(width(), 480), sizeHint().height());
}

void SavedFilesSearchDialog::setSearchText(const QString &text)
{
    m_text->setText(text.trimmed());
}

QString SavedFilesSearchDialog::searchText() const
{
    return m_text->text().trimmed();
}

void SavedFilesSearchDialog::setFolder(const QString &folder)
{
    const QString text = displayedPath(QDir::cleanPath(folder));
    if (m_folder->text() == text)
        refresh();
    else
        m_folder->setText(text);
}

bool SavedFilesSearchDialog::event(QEvent *event)
{
    // Coming back from System Settings, File Search may cover the folder differently
    if (event->type() == QEvent::WindowActivate)
        refresh();
    return QDialog::event(event);
}

QString SavedFilesSearchDialog::folder() const
{
    const QString text = m_folder->text().trimmed();
    return text.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(text));
}

UiUtils::FileSearch::Status SavedFilesSearchDialog::status() const
{
    return m_coverage;
}

QString SavedFilesSearchDialog::statusText() const
{
    return m_status->text();
}

void SavedFilesSearchDialog::setSearchWorkers(bool indexedSearch, bool liveSearch)
{
    m_hasIndexedSearch = indexedSearch;
    m_hasLiveSearch = liveSearch;
    refresh();
}

void SavedFilesSearchDialog::setUrlOpener(const UrlOpener &opener)
{
    m_opener = opener;
}

void SavedFilesSearchDialog::refresh()
{
    const QString path = folder();
    const bool folderExists = !path.isEmpty() && QDir(path).exists();
    // Baloo's own settings; read again each time, as System Settings may change them while the dialog is open
    const KConfig balooConfig(QStringLiteral("baloofilerc"), KConfig::NoGlobals);
    m_coverage = folderExists ? UiUtils::FileSearch::coverage(balooConfig, path, m_hasIndexedSearch)
                              : UiUtils::FileSearch::Status();

    const QString deciding = displayedPath(m_coverage.decidingFolder);
    const QString indexedInside = displayedPath(m_coverage.indexedSubfolder);
    QString text;
    if (!folderExists) {
        text = path.isEmpty() ? i18nc("@info", "Choose the folder in which you saved the files.")
                              : i18nc("@info %1 is a folder", "The folder %1 does not exist.", displayedPath(path));
    } else {
        switch (m_coverage.coverage) {
        case Coverage::NotInstalled:
            text = i18nc("@info", "Plasma's File Search is not available, so this folder is not indexed. A live "
                                  "search looks through it instead.");
            break;
        case Coverage::Disabled:
            text = i18nc("@info", "File Search is turned off, so this folder is not indexed. A live search looks "
                                  "through it instead, or turn on File Search in System Settings.");
            break;
        case Coverage::NotIncluded:
            if (indexedInside.isEmpty()) {
                text = i18nc("@info", "File Search is not set to index this folder. A live search looks through it "
                                      "instead, or add the folder to File Search in System Settings.");
            } else {
                text = i18nc("@info %1 is a folder", "File Search is set to index only %1 inside this folder. Search "
                                                     "the index for files there, or a live search looks through the "
                                                     "whole folder.", indexedInside);
            }
            break;
        case Coverage::Excluded:
            if (indexedInside.isEmpty()) {
                text = i18nc("@info %1 is a folder", "File Search skips %1, so this folder is not indexed. A live "
                                                     "search looks through it instead.", deciding);
            } else {
                text = i18nc("@info %1 and %2 are folders", "File Search skips %1, except %2 inside it. Search the "
                                                             "index for files there, or a live search looks through "
                                                             "the whole folder.", deciding, indexedInside);
            }
            break;
        case Coverage::Hidden:
            text = i18nc("@info %1 is a folder", "File Search skips hidden folders such as %1, so this folder is not "
                                                 "indexed. A live search looks through it instead.", deciding);
            break;
        case Coverage::NamesOnly:
            text = i18nc("@info", "File Search indexes only the names of the files in this folder, not their text. "
                                  "A live search also looks inside the files.");
            break;
        case Coverage::Covered:
            text = i18nc("@info", "File Search is set to index this folder, which does not guarantee that every "
                                  "file is indexed yet: files saved in the last few minutes may be missing. A live "
                                  "search finds them right away.");
            break;
        }
        if (!m_hasLiveSearch)
            text += QLatin1Char(' ') + i18nc("@info", "Live search is not installed, so the folder opens in your "
                                                      "file manager instead.");
    }
    m_status->setText(text);

    const bool hasText = !searchText().isEmpty();
    m_indexed->setVisible(m_hasIndexedSearch);
    m_indexed->setEnabled(folderExists && hasText && m_coverage.isSearchable());
    if (m_hasLiveSearch) {
        m_live->setText(i18nc("@action:button search through the files of a folder", "Live Search"));
        m_live->setIcon(UiUtils::loadIcon(QStringLiteral("edit-find")));
        m_live->setToolTip(i18nc("@info:tooltip", "Look through the names and text of the files in the folder in "
                                                  "your file manager"));
        m_live->setEnabled(folderExists && hasText);
    } else {
        m_live->setText(i18nc("@action:button", "Open Folder"));
        m_live->setIcon(UiUtils::loadIcon(QStringLiteral("document-open-folder")));
        m_live->setToolTip(i18nc("@info:tooltip", "Open the folder in your file manager"));
        m_live->setEnabled(folderExists);
    }
    // Only part of the folder is indexed: the live search is the one that looks through all of it
    const bool preferIndexed = m_indexed->isEnabled() && (m_coverage.coversWholeFolder() || !m_live->isEnabled());
    m_indexed->setDefault(preferIndexed);
    m_live->setDefault(!preferIndexed && m_live->isEnabled());
}

QString SavedFilesSearchDialog::title() const
{
    return i18nc("@title search results in the file manager, %1 is the searched text", "Saved files matching “%1”",
                 searchText());
}

void SavedFilesSearchDialog::searchIndex()
{
    if (!m_indexed->isEnabled())
        return;
    open(UiUtils::FileSearch::indexedSearchUrl(searchText(), folder(), title()));
}

void SavedFilesSearchDialog::searchLive()
{
    if (!m_live->isEnabled())
        return;
    if (m_hasLiveSearch)
        open(UiUtils::FileSearch::liveSearchUrl(searchText(), folder(), title()));
    else
        open(QUrl::fromLocalFile(folder()));
}

void SavedFilesSearchDialog::chooseFolder()
{
    FileChooser::Request request;
    request.title = i18nc("@title:window", "Choose Folder");
    request.mode = FileChooser::Mode::Folder;
    request.folder = QUrl::fromLocalFile(folder());
    const QString chosen = FileChooser::chooseLocalPath(this, request);
    if (!chosen.isEmpty())
        setFolder(chosen);
}

void SavedFilesSearchDialog::open(const QUrl &url)
{
    if (m_opener)
        m_opener(url);
    accept();
}

}
