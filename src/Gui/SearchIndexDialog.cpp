/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SearchIndexDialog.h"

#include <algorithm>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KLocalizedString>

#include "Imap/Model/SearchIndexer.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

namespace {

using Coverage = Imap::Mailbox::SearchIndexer::FolderCoverage;

enum Column {
    ColumnFolder,
    ColumnState,
    ColumnCached,
    ColumnIndexed,
    ColumnWithText,
    ColumnUpdated,
    ColumnCount,
};

QString stateText(Coverage::State state)
{
    switch (state) {
    case Coverage::State::NotCached:
        return i18nc("@item:intable index state of a folder", "Not on this computer");
    case Coverage::State::NotIndexed:
        return i18nc("@item:intable index state of a folder", "Not indexed yet");
    case Coverage::State::OutOfDate:
        return i18nc("@item:intable index state of a folder", "Changed since indexed");
    case Coverage::State::Partial:
        return i18nc("@item:intable index state of a folder", "Partly indexed");
    case Coverage::State::Indexed:
        return i18nc("@item:intable index state of a folder", "Indexed");
    }
    return QString();
}

QString stateToolTip(Coverage::State state)
{
    switch (state) {
    case Coverage::State::NotCached:
        return i18nc("@info:tooltip", "The folder was never opened while online with offline mail on, so none of its "
                                      "messages can be searched offline.");
    case Coverage::State::NotIndexed:
        return i18nc("@info:tooltip", "The folder is kept offline, but the index has not reached it yet.");
    case Coverage::State::OutOfDate:
        return i18nc("@info:tooltip", "Messages arrived or went away since the folder was indexed; the next update "
                                      "catches up.");
    case Coverage::State::Partial:
        return i18nc("@info:tooltip", "Some messages of the folder have no headers on this computer yet, so the "
                                      "index cannot hold them. Opening the folder while online loads them.");
    case Coverage::State::Indexed:
        return i18nc("@info:tooltip", "Every message of the folder kept on this computer is indexed.");
    }
    return QString();
}

}

SearchIndexDialog::SearchIndexDialog(QWidget *parent)
    : QDialog(parent)
    , m_state(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_summary(new QLabel(this))
    , m_warningIcon(new QLabel(this))
    , m_warning(new QLabel(this))
    , m_folders(new QTreeWidget(this))
    , m_pause(new QPushButton(this))
    , m_rebuild(new QPushButton(UiUtils::loadIcon(QStringLiteral("view-refresh")),
                                i18nc("@action:button", "Rebuild…"), this))
{
    setWindowTitle(i18nc("@title:window", "Offline Search Index"));
    m_state->setObjectName(QStringLiteral("indexState"));
    m_state->setWordWrap(true);
    m_progress->setObjectName(QStringLiteral("indexProgress"));
    m_progress->setTextVisible(true);
    m_summary->setObjectName(QStringLiteral("indexSummary"));
    m_summary->setWordWrap(true);
    m_warning->setObjectName(QStringLiteral("indexWarning"));
    m_warning->setWordWrap(true);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_warningIcon->setPixmap(UiUtils::loadIcon(QStringLiteral("dialog-warning")).pixmap(iconSize, iconSize));
    m_warningIcon->setAlignment(Qt::AlignTop);
    auto *warningRow = new QHBoxLayout;
    warningRow->addWidget(m_warningIcon);
    warningRow->addWidget(m_warning, 1);

    m_folders->setObjectName(QStringLiteral("indexFolders"));
    m_folders->setRootIsDecorated(false);
    m_folders->setUniformRowHeights(true);
    m_folders->setAlternatingRowColors(true);
    m_folders->setColumnCount(ColumnCount);
    m_folders->setHeaderLabels({
        i18nc("@title:column", "Folder"),
        i18nc("@title:column index state", "State"),
        i18nc("@title:column messages kept offline", "Offline"),
        i18nc("@title:column", "Indexed"),
        i18nc("@title:column indexed messages whose text is offline", "With Text"),
        i18nc("@title:column", "Updated"),
    });
    m_folders->header()->setStretchLastSection(false);
    m_folders->header()->setSectionResizeMode(ColumnFolder, QHeaderView::Stretch);
    for (int column = ColumnState; column < ColumnCount; ++column)
        m_folders->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    m_pause->setObjectName(QStringLiteral("indexPause"));
    m_rebuild->setObjectName(QStringLiteral("indexRebuild"));
    m_rebuild->setToolTip(i18nc("@info:tooltip", "Empty the index and build it again from the mail on this computer"));
    connect(m_pause, &QPushButton::clicked, this, &SearchIndexDialog::togglePaused);
    connect(m_rebuild, &QPushButton::clicked, this, &SearchIndexDialog::askRebuild);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_pause, QDialogButtonBox::ActionRole);
    buttons->addButton(m_rebuild, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_state);
    layout->addWidget(m_progress);
    layout->addWidget(m_summary);
    layout->addLayout(warningRow);
    layout->addWidget(m_folders, 1);
    layout->addWidget(buttons);
    resize(640, 420);

    // A pass reports every folder and batch; reading the coverage that often would slow it down
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(500);
    connect(&m_refresh, &QTimer::timeout, this, &SearchIndexDialog::refresh);
    refresh();
}

void SearchIndexDialog::setIndexer(Imap::Mailbox::SearchIndexer *indexer, const QString &accountName)
{
    for (const QMetaObject::Connection &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    m_indexer = indexer;
    m_accountName = accountName;
    setWindowTitle(accountName.isEmpty() ? i18nc("@title:window", "Offline Search Index")
                                         : i18nc("@title:window %1 is an account name", "Offline Search Index: %1",
                                                 accountName));
    if (indexer) {
        const auto later = [this]() {
            if (!m_refresh.isActive())
                m_refresh.start();
        };
        m_connections << connect(indexer, &Imap::Mailbox::SearchIndexer::progressChanged, this, later);
        m_connections << connect(indexer, &Imap::Mailbox::SearchIndexer::finished, this, &SearchIndexDialog::refresh);
        m_connections << connect(indexer, &QObject::destroyed, this, &SearchIndexDialog::refresh, Qt::QueuedConnection);
    }
    refresh();
}

Imap::Mailbox::SearchIndexer *SearchIndexDialog::indexer() const
{
    return m_indexer;
}

void SearchIndexDialog::refresh()
{
    m_refresh.stop();
    m_folders->clear();
    m_progress->hide();
    m_summary->clear();
    const bool available = m_indexer && m_indexer->isAvailable();
    m_pause->setEnabled(available);
    m_rebuild->setEnabled(available);
    const bool paused = m_indexer && m_indexer->isPaused();
    m_pause->setText(paused ? i18nc("@action:button", "Resume") : i18nc("@action:button", "Pause"));
    m_pause->setIcon(UiUtils::loadIcon(paused ? QStringLiteral("media-playback-start")
                                              : QStringLiteral("media-playback-pause")));
    QString warning;
    if (!m_indexer) {
        m_state->setText(i18nc("@info", "Offline search is off for this account. To search mail kept on this "
                                        "computer while offline, turn on <interface>Index downloaded mail for "
                                        "searching offline</interface> in the account settings."));
    } else if (!available) {
        m_state->setText(i18nc("@info %1 is the reason", "The offline search index cannot be used: %1",
                               m_indexer->unavailableReason()));
    } else if (paused) {
        m_state->setText(m_indexer->hasPendingChanges()
                             ? i18nc("@info", "Indexing is paused. Changes to the mail on this computer since then "
                                              "are indexed when it is resumed.")
                             : i18nc("@info", "Indexing is paused."));
    } else if (m_indexer->isRunning()) {
        const Imap::Mailbox::SearchIndexer::Progress progress = m_indexer->progress();
        m_state->setText(progress.mailbox.isEmpty()
                             ? i18nc("@info", "Indexing…")
                             : i18nc("@info folder name, folder number, folders", "Indexing %1, folder %2 of %3…",
                                     progress.mailbox, progress.folder, progress.folders));
        m_progress->setRange(0, std::max(1, progress.messages));
        m_progress->setValue(std::min(progress.position, std::max(1, progress.messages)));
        m_progress->setFormat(i18nc("@info:progress messages done, messages", "%1 of %2 messages", progress.position,
                                    progress.messages));
        m_progress->show();
    } else if (m_indexer->hasPendingChanges()) {
        m_state->setText(i18nc("@info", "Up to date as of the last update; newer changes are indexed shortly."));
    } else {
        m_state->setText(i18nc("@info", "The index is up to date with the mail on this computer."));
    }

    if (available) {
        const QList<Coverage> coverage = m_indexer->coverage();
        int confirmed = 0;
        int incomplete = 0;
        QDateTime lastUpdate;
        const QLocale locale;
        for (const Coverage &folder : coverage) {
            auto *item = new QTreeWidgetItem(m_folders);
            item->setText(ColumnFolder, folder.mailbox);
            item->setToolTip(ColumnFolder, folder.mailbox);
            item->setText(ColumnState, stateText(folder.state));
            item->setToolTip(ColumnState, stateToolTip(folder.state));
            item->setData(ColumnState, Qt::UserRole, int(folder.state));
            if (folder.state != Coverage::State::NotCached)
                item->setText(ColumnCached, locale.toString(folder.cached));
            if (folder.updated.isValid()) {
                item->setText(ColumnIndexed, locale.toString(folder.indexed));
                item->setText(ColumnWithText, locale.toString(folder.withText));
                item->setText(ColumnUpdated, locale.toString(folder.updated.toLocalTime(), QLocale::ShortFormat));
                if (!lastUpdate.isValid() || folder.updated > lastUpdate)
                    lastUpdate = folder.updated;
            }
            for (int column = ColumnCached; column <= ColumnWithText; ++column)
                item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
            if (folder.state == Coverage::State::Indexed) {
                ++confirmed;
            } else {
                ++incomplete;
                item->setIcon(ColumnState, UiUtils::loadIcon(folder.state == Coverage::State::NotCached
                                                                 ? QStringLiteral("dialog-information")
                                                                 : QStringLiteral("dialog-warning")));
            }
        }
        QString summary = i18ncp("@info %2 is the folders confirmed as indexed",
                                 "%1 folder to index, %2 confirmed as indexed.",
                                 "%1 folders to index, %2 confirmed as indexed.", int(coverage.size()), confirmed);
        if (lastUpdate.isValid()) {
            summary += QLatin1Char(' ')
                + i18nc("@info date and time", "Last updated %1.",
                        locale.toString(lastUpdate.toLocalTime(), QLocale::ShortFormat));
        }
        m_summary->setText(summary);
        if (incomplete > 0) {
            warning = i18ncp("@info", "%1 folder is not fully indexed, so offline searches may miss some of its messages.",
                             "%1 folders are not fully indexed, so offline searches may miss some of their messages.",
                             incomplete);
        }
        if (paused && !warning.isEmpty())
            warning += QLatin1Char(' ') + i18nc("@info", "Resume indexing to catch up.");
    }
    m_warning->setText(warning);
    m_warning->setVisible(!warning.isEmpty());
    m_warningIcon->setVisible(!warning.isEmpty());
}

void SearchIndexDialog::togglePaused()
{
    if (!m_indexer)
        return;
    const bool paused = !m_indexer->isPaused();
    m_indexer->setPaused(paused);
    emit pausedChanged(paused);
    refresh();
}

void SearchIndexDialog::askRebuild()
{
    if (!m_indexer || !m_indexer->isAvailable())
        return;
    const QString question = i18nc("@info", "Empty the offline search index and build it again from the mail on this "
                                            "computer? Offline searches find less until it is done.");
    if (QMessageBox::question(this, i18nc("@title:window", "Rebuild Search Index"), question) != QMessageBox::Yes)
        return;
    m_indexer->rebuild();
    refresh();
}

}
