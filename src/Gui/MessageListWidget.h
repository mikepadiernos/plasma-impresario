/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QWidget>
#include "Imap/Model/FavoriteTagsModel.h"

class LineEdit;
class KConfigGroup;
class QHBoxLayout;
class QIcon;
class QLabel;
class QMenu;
class QTimer;
class QToolButton;

namespace Gui {

class MsgListView;

/** @short Widget containing a list of messages along with quick filtering toolbar */
class MessageListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel, const QString &accountId);

    void setFuzzySearchSupported(bool supported);
    void setRawSearchEnabled(bool enabled);

    QStringList searchConditions() const;

    /** Key saved searches to another account, for example after switching accounts. */
    void setAccountId(const QString &accountId);

    /** @short Add a compact button for @p action next to the quick search field */
    QToolButton *addToolButton(QAction *action);
    /** @short Add a button which opens @p menu next to the quick search field */
    QToolButton *addToolMenuButton(QMenu *menu, const QIcon &icon, const QString &text);
    /** @short Show @p text below the list, for example message counts; an empty text hides it */
    void setListStatus(const QString &text);
    /** @short Show which account and folder the list belongs to next to the status; an empty folder hides it */
    void setScope(const QString &account, const QString &folder, const QString &fullFolderName);

    // FIXME: consider making this private and moving the logic from Window with it
    MsgListView *tree;

public slots:
    void focusSearch();
    /** @short Put a field filter such as "from:ann@example.org" into the quick search and apply it */
    void showFieldFilter(const QString &text);
    void focusRawSearch();

signals:
    void requestingSearch(const QStringList &conditions);
    void rawSearchSettingChanged(bool enabled);

protected slots:
    void slotApplySearch();
    void slotAutoEnableDisableSearch();
    void slotSortingFailed();

private slots:
    /** Saves the current search for this account. */
    void saveCurrentSearch();
    /** Rebuilds the menu of searches saved for this account. */
    void rebuildSavedSearchMenu();
    void slotComplexSearchInput(QAction*);
    void slotConditionalSearchReset();
    void slotDeActivateSimpleSearch();
    void slotResetSortingFailed();
    void slotUpdateSearchCursor();

private:
    /** Returns the saved-search configuration for this account. */
    KConfigGroup savedSearchGroup() const;
    /** Adds the saved search identified by @p id to the menu. */
    void addSavedSearch(const QString &id);
    /** Applies the saved search identified by @p id to the current mailbox. */
    void runSavedSearch(const QString &id);
    /** Deletes the saved search identified by @p id after confirmation. */
    void deleteSavedSearch(const QString &id);

    LineEdit *m_quickSearchText;
    /** The quick search field and the buttons next to it */
    QHBoxLayout *m_toolRow;
    QLabel *m_listStatus;
    /** Account and folder of the list, in the row of the status */
    QLabel *m_scope;
    QWidget *m_statusRow;
    QAction *m_searchOptions;
    QAction *m_searchInSubject;
    QAction *m_searchInBody;
    QAction *m_searchInSenders;
    QAction *m_searchInRecipients;
    QAction *m_searchFuzzy;
    QAction *m_rawSearch;
    QMenu *m_savedSearches;
    QString m_accountId;
    bool m_supportsFuzzySearch;
    QTimer *m_searchResetTimer;
    QString m_queryPlaceholder;

    friend class MainWindow; // needs access to our private slots
};

}

#endif // MESSAGELISTWIDGET_H
