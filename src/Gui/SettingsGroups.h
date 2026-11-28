/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QKeySequence>
#include <QMap>
#include <QPointer>
#include <QScrollArea>
#include <functional>

#include <KSharedConfig>

#include "Gui/SettingsDialog.h"
#include "UiUtils/Keymap.h"
#include "UiUtils/SettingsSchema.h"
#include "UiUtils/Workspaces.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QVBoxLayout;

namespace Gui {

/** @short What the application settings need to know about the window, and how
they ask it for help

The groups of the settings dialog read and write KConfig themselves; everything
else, such as the accounts and their folders or the dialogs of the window, comes
through here, so that the groups can be tested without a window and without
modal dialogs.
*/
struct SettingsContext {
    struct Account {
        QString id;
        QString name;
    };
    QList<Account> accounts;
    /** @short Folders of an account which are known now */
    std::function<QStringList(const QString &accountId)> folders;
    /** @short The settings of an account, for its search index */
    std::function<QSettings *(const QString &accountId)> accountSettings;

    struct Activity {
        QString id;
        QString name;
    };
    QList<Activity> activities;
    bool activitiesAvailable = false;
    /** @short Integrations which are set up on this computer */
    UiUtils::SettingsSchema::Features available;
    /** @short Single-key shortcuts of the window, which Vim mode may shadow */
    QList<QKeySequence> shortcuts;

    std::function<void()> configureNotifications;
    std::function<void(QWidget *parent)> manageSearchIndex;
    std::function<void()> clearSearchHistory;
    /** @short Ask a yes-or-no question; true for yes */
    std::function<bool(QWidget *parent, const QString &title, const QString &text)> confirm;

    enum class FileMode {
        Directory,
        Open,
        Save,
    };
    /** @short Ask for a file or a folder; empty when cancelled */
    std::function<QString(QWidget *parent, const QString &title, FileMode mode, const QString &start)> chooseFile;

    struct NextcloudAccount {
        QString id;
        QString title;
        /** Whether it comes from KDE Online Accounts, which disconnects it */
        bool onlineAccount = false;
    };
    std::function<QList<NextcloudAccount>()> nextcloudAccounts;
    /** @short Ask for a Nextcloud account to connect; @arg changed runs once one is */
    std::function<void(QWidget *parent, std::function<void()> changed)> connectNextcloud;
    /** @short Disconnect a Nextcloud account and revoke its app password; @arg done gets a warning or nothing */
    std::function<void(QWidget *parent, const QString &id, std::function<void(const QString &warning)> done)>
        disconnectNextcloud;

    /** @short The profile's settings, where settings which predate KConfig keep
     * their application value */
    QSettings *legacy = nullptr;
};

/** @short A group of the application settings, shown as one page of the
 * settings dialog */
class SettingsGroupPage : public QScrollArea, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    SettingsGroupPage(UiUtils::SettingsSchema::Group group, const SettingsContext &context, QWidget *parent);

    UiUtils::SettingsSchema::Group group() const;
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

public slots:
    void updateWidgets();

signals:
    void saved();
    void widgetsUpdated();

protected:
    virtual void saveSettings() = 0;
    QVBoxLayout *contents() const;
    /** @short A titled section of the page, with a form for its rows */
    QFormLayout *addSection(const QString &title);
    /** @short A note under the rows of a section, in a smaller font */
    QLabel *addNote(QFormLayout *form, const QString &text);
    KSharedConfig::Ptr config() const;
    QVariant application(const QString &key) const;
    void setApplication(const QString &key, const QVariant &value);

    const SettingsContext &m_context;

private:
    UiUtils::SettingsSchema::Group m_group;
    QVBoxLayout *m_contents;
};

/** @short Starting, the system tray and running in the background */
class GeneralGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    GeneralGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;
};

/** @short Settings of one account or one of its folders which replace the
 * application's */
class AccountsGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    AccountsGroup(const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    using Target = std::pair<QString, QString>;
    void fillScopes();
    void showOverrides();
    UiUtils::SettingsSchema::Values storedValues(const Target &target) const;
    UiUtils::SettingsSchema::Values values(const Target &target) const;
    void setPending(const QString &key, const QVariant &value);
    QWidget *editorFor(const UiUtils::SettingsSchema::Definition &definition, const QVariant &value);
    QVariant editorValue(const UiUtils::SettingsSchema::Definition &definition, QWidget *editor) const;

    QComboBox *m_account;
    QComboBox *m_scope;
    QGridLayout *m_rows;
    QWidget *m_rowsWidget;
    QLabel *m_empty;
    /** Changes not saved yet; an invalid value removes the override */
    QMap<Target, UiUtils::SettingsSchema::Values> m_pending;
};

/** @short Layout presets, the folder sidebar, the header, tabs and favorite
 * tags */
class AppearanceGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    AppearanceGroup(FavoriteTagsPage *favoriteTags, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    void updateTabs();

    QComboBox *m_preset;
    QComboBox *m_sidebar;
    QCheckBox *m_header;
    QCheckBox *m_symbolicTray;
    QCheckBox *m_tabs;
    QComboBox *m_tabRegion;
    QComboBox *m_tabOrientation;
};

/** @short Showing messages and marking them as read */
class ReadingGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    ReadingGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;
};

/** @short Spell checking and the address book */
class CompositionGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    CompositionGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;
};

/** @short What leaves the computer, attachments, junk and passwords */
class PrivacyGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    PrivacyGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;
};

/** @short The unread count and the notifications of new mail */
class NotificationsGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    NotificationsGroup(GeneralPage *general, const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;
};

/** @short Where searches start, the server or the index, the history and the
 * indexed accounts */
class SearchGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    SearchGroup(const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    void load();

    QComboBox *m_places;
    QComboBox *m_useIndex;
    QSpinBox *m_historyLimit;
    QCheckBox *m_remember;
    QCheckBox *m_suggest;
    QTableWidget *m_accounts;
    /** Whether Reset was chosen, so that saving forgets the search settings first */
    bool m_reset = false;
};

/** @short Mail to calendar, mail to notes and work-context actions */
class IntegrationsGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    IntegrationsGroup(const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    void updateAvailability();
    void loadNextcloud();
    void updateNextcloud();
    void disconnectNextcloud();

    QListWidget *m_nextcloud;
    QPushButton *m_connectNextcloud;
    QPushButton *m_disconnectNextcloud;
    QLabel *m_nextcloudStatus;
    QCheckBox *m_calendar;
    QLabel *m_calendarReason;
    QCheckBox *m_notes;
    QLabel *m_notesReason;
    QLineEdit *m_notesFolder;
    QCheckBox *m_workContext;
};

/** @short Accounts, a notes folder and a Plasma Activity which belong together
 */
class WorkspacesGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    WorkspacesGroup(const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    void fillList();
    void showWorkspace();
    void storeEditor();
    void captureAppearance();
    UiUtils::Workspaces::Workspace *current();

    QList<UiUtils::Workspaces::Workspace> m_workspaces;
    QList<UiUtils::ThemeProfiles::Profile> m_profiles;
    QListWidget *m_list;
    QPushButton *m_remove;
    QWidget *m_editor;
    QLineEdit *m_name;
    QListWidget *m_accounts;
    QComboBox *m_activity;
    QLineEdit *m_notesFolder;
    QComboBox *m_appearance;
    QComboBox *m_profile;
    QListWidget *m_options;
    QLabel *m_overrides;
    QPushButton *m_clearOverrides;
    QLabel *m_problems;
    QCheckBox *m_follow;
    bool m_loading = false;
};

/** @short Vim mode, its bindings and the keymap shared with Plasma Opus */
class KeyboardGroup : public SettingsGroupPage
{
    Q_OBJECT
public:
    KeyboardGroup(const SettingsContext &context, QWidget *parent);

protected:
    void saveSettings() override;

private:
    void load();
    void fillBindings();
    void updateProblems();
    UiUtils::Keymap::Profile profile() const;
    void importProfile();
    void exportProfile();
    void chooseFollowed();

    QCheckBox *m_vimMode;
    QSpinBox *m_timeout;
    QCheckBox *m_hints;
    QSpinBox *m_hintDelay;
    QTableWidget *m_bindings;
    QLabel *m_problems;
    QLabel *m_status;
    QCheckBox *m_follow;
    QLineEdit *m_followed;
    QPushButton *m_undo;
    UiUtils::Keymap::Bindings m_keys;
    bool m_loading = false;
};

/** @short A label for @arg key in the settings dialog, such as "Mark messages
 * as read when shown" */
QString settingLabel(const QString &key);

} // namespace Gui
