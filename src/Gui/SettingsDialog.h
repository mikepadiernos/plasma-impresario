/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>

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
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QSettings>
#include "ui_EditIdentity.h"
#include "ui_EditFavoriteTag.h"
#include "ui_SettingsCachePage.h"
#include "ui_SettingsFavoriteTagsPage.h"
#include "ui_SettingsGeneralPage.h"
#include "ui_SettingsIdentitiesPage.h"
#include "ui_SettingsImapPage.h"
#include "ui_SettingsOutgoingPage.h"
#include "Composer/AccountIdentities.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Plugins/PasswordPlugin.h"

class QCheckBox;
class QComboBox;
class QDataWidgetMapper;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Composer
{
class SenderIdentitiesModel;
}

namespace Plugins
{
class PluginManager;
}

namespace UiUtils {
class PasswordWatcher;
}

namespace Imap {
class ImapAccess;
}

namespace MSA {
class Account;
}

namespace Gui
{
class MainWindow;
class SettingsDialog;
class SettingsOAuthControls;

/** @short Common interface for any page of the settings dialogue */
class ConfigurationWidgetInterface {
public:
    virtual void save(QSettings &s) = 0;
    virtual QWidget *asWidget() = 0;
    virtual bool checkValidity() const = 0;
    virtual bool passwordFailures(QString &message) const = 0;
};


class FavoriteTagsPage : public QScrollArea, Ui_FavoriteTagsPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    FavoriteTagsPage(SettingsDialog *parent, QSettings &s, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

private slots:
    void updateWidgets();
    void moveTagBy(const int offset);
    void addButtonClicked();
    void editButtonClicked();
    void deleteButtonClicked();

private:
    Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel;
    SettingsDialog *m_parent;

    FavoriteTagsPage(const FavoriteTagsPage &) = delete;
    FavoriteTagsPage &operator=(const FavoriteTagsPage &) = delete;

signals:
    void saved();
    void widgetsUpdated();
};

class GeneralPage : public QScrollArea, Ui_GeneralPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    GeneralPage(SettingsDialog *parent, QSettings &s);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

private slots:
    void updateWidgets();
    void passwordPluginChanged();

private:
    SettingsDialog *m_parent;

    GeneralPage(const GeneralPage &); // don't implement
    GeneralPage &operator=(const GeneralPage &); // don't implement

signals:
    void saved();
    void reloadPasswords();
    void widgetsUpdated();
};

/** @short Sender identities of one account: its own by default, the identities shared by all accounts, or none */
class IdentitiesPage : public QScrollArea, Ui_IdentitiesPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    /**
     * @param account settings of the account being configured
     * @param shared application-wide settings that store the shared identities
     */
    IdentitiesPage(SettingsDialog *parent, QSettings &account, QSettings &shared,
                   Composer::SenderIdentitiesModel *accountIdentities, Composer::SenderIdentitiesModel *sharedIdentities);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

private slots:
    void updateWidgets();
    void showIdentitiesInUse();
    void moveIdentityUp();
    void moveIdentityDown();
    void addButtonClicked();
    void editButtonClicked();
    void deleteButtonClicked();

private:
    Composer::AccountIdentities::Source selectedSource() const;

    Composer::SenderIdentitiesModel *m_accountIdentities;
    Composer::SenderIdentitiesModel *m_sharedIdentities;
    /** The list shown, depending on the identity source; read-only when the account uses no identity */
    Composer::SenderIdentitiesModel *m_identitiesModel;
    QSettings &m_shared;

    IdentitiesPage(const IdentitiesPage &) = delete;
    IdentitiesPage &operator=(const IdentitiesPage &) = delete;

signals:
    void saved();
    void widgetsUpdated();
};

class EditIdentity : public QDialog, Ui_EditIdentity
{
    Q_OBJECT
public:
    EditIdentity(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, const QModelIndex &currentIndex);
    void setDeleteOnReject(const bool reject = true);

public slots:
    void enableButton();
    void onReject();

private:
    Composer::SenderIdentitiesModel *m_identitiesModel;
    QDataWidgetMapper *m_mapper;
    bool m_deleteOnReject;

    EditIdentity(const EditIdentity &); // don't implement
    EditIdentity &operator=(const EditIdentity &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

class EditFavoriteTag : public QDialog, Ui_EditFavoriteTag
{
    Q_OBJECT
public:
    EditFavoriteTag(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel, const QModelIndex &currentIndex);
    void setDeleteOnReject(const bool reject = true);

public slots:
    void tryEnableButton();
    void onAccept();
    void onReject();
    void colorButtonClick();

private:
    Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel;
    const QModelIndex currentIndex; // index of the edited item in model
    bool m_deleteOnReject;
    QString name();
    QString color();
    void setColorButtonColor(const QString color);

    EditFavoriteTag(const EditFavoriteTag &) = delete;
    EditFavoriteTag &operator=(const EditFavoriteTag &) = delete;

signals:
    void saved();
    void widgetsUpdated();
};


class OutgoingPage : public QScrollArea, Ui_OutgoingPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    OutgoingPage(SettingsDialog *parent, QSettings &s);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

private slots:
    void slotSetSubmissionMethod();
    void updateWidgets();
    void slotSetPassword();
    void showPortWarning(const QString &warning);
    void setPortByText(const QString &text);

private:

    enum { NETWORK, SENDMAIL, IMAP_SENDMAIL };
    enum Encryption { SMTP, SMTP_STARTTLS, SSMTP };

    SettingsDialog *m_parent;
    UiUtils::PasswordWatcher *m_pwWatcher;

    MSA::Account *m_smtpAccountSettings;
    QComboBox *smtpAuthMethod = nullptr;
    QLabel *smtpOAuthNote = nullptr;

    bool usesOAuth() const;

    OutgoingPage(const OutgoingPage &); // don't implement
    OutgoingPage &operator=(const OutgoingPage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

class ImapPage : public QScrollArea, Ui_ImapPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    ImapPage(SettingsDialog *parent, QSettings &s);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;
    /** @short Revoke an OAuth sign-in made in the dialog unless the saved settings use it */
    void finish(const QSettings &s);

private:
    enum { NETWORK, PROCESS };
    enum Encryption { NONE, STARTTLS, SSL };
    SettingsDialog *m_parent;
    quint16 m_imapPort;
    bool m_imapStartTls;
    UiUtils::PasswordWatcher *m_pwWatcher;
    QComboBox *imapAuthMethod = nullptr;
    SettingsOAuthControls *m_oauth = nullptr;

    bool usesOAuth() const;

private slots:
    void updateWidgets();
    void maybeShowPortWarning();
    void changePort();
    void slotSetPassword();

private:
    ImapPage(const ImapPage &); // don't implement
    ImapPage &operator=(const ImapPage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

class CachePage : public QScrollArea, Ui_CachePage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    CachePage(QWidget *parent, QSettings &s);
    void save(QSettings &s) override;
    QWidget *asWidget() override;
    bool checkValidity() const override;
    bool passwordFailures(QString &message) const override;

private:
    QCheckBox *startOffline;
    QCheckBox *searchIndex;
    QCheckBox *searchIndexDecrypted;
    QCheckBox *searchIndexAttachments;
    QSpinBox *searchIndexAttachmentLimit;
    QLabel *searchIndexNote;

private slots:
    void updateWidgets();

private:
    CachePage(const CachePage &); // don't implement
    CachePage &operator=(const CachePage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};


/** @short Settings of the application, or of the active account, each in its own dialog */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Scope {
        /** General and Favorite tags pages, stored in the application settings */
        Application,
        /** Identities, IMAP, Offline and SMTP pages of the active account */
        Account,
    };

    SettingsDialog(MainWindow *parent, Scope scope, Composer::SenderIdentitiesModel *sharedIdentities,
        Composer::SenderIdentitiesModel *accountIdentities, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel,
        QSettings *settings);

    Scope scope() const;

    Plugins::PluginManager *pluginManager();
    Imap::ImapAccess* imapAccess();

    void setOriginalPlugins(const QString &passwordPlugin, const QString &addressBookPlugin, const QString& spellcheckerPlugin);

    static QString warningStyleSheet();

signals:
    void reloadPasswordsRequested();

public slots:
    void accept() override;
    void reject() override;
private slots:
    void adjustSizeToScrollAreas();
    void slotAccept();
private:
    MainWindow *mainWindow;
    Scope m_scope;
    QDialogButtonBox *buttons;
    QTabWidget *stack;
    QVector<ConfigurationWidgetInterface*> pages;
    QHash<ConfigurationWidgetInterface *, QSettings *> m_pageSettings;
    QSettings *m_accountSettings;
    Composer::SenderIdentitiesModel *m_senderIdentities;
    Composer::SenderIdentitiesModel *m_accountIdentities;
    Imap::Mailbox::FavoriteTagsModel *m_favoriteTags;
    QSettings *m_settings;
    int m_saveSignalCount;
    QString m_originalPasswordPlugin;
    QString m_originalAddressbookPlugin;
    QString m_originalSpellcheckerPlugin;
    ImapPage *m_imapPage = nullptr;

    SettingsDialog(const SettingsDialog &); // don't implement
    SettingsDialog &operator=(const SettingsDialog &); // don't implement

    /** Add a page whose values are loaded from and saved to the given settings. */
    void addPage(ConfigurationWidgetInterface *page, const QString &title, QSettings *pageSettings);
};

}

#endif // SETTINGSDIALOG_H
