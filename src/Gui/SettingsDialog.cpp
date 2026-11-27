/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>
   Copyright (C) 2013        Pali Rohár <pali.rohar@gmail.com>

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
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDataWidgetMapper>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include "SettingsDialog.h"
#include "ColoredItemDelegate.h"
#include "Common/InvokeMethod.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Composer/AccountIdentities.h"
#include "Gui/StartAfterLogin.h"
#include "Gui/Util.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "MSA/Account.h"
#include "OAuth/OAuth.h"
#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/BackgroundRunning.h"
#include "UiUtils/PlasmaSearch.h"
#include "UiUtils/IconLoader.h"
#include "SettingsOAuthControls.h"
#include "UiUtils/PasswordWatcher.h"
#include <KLocalizedString>
#include <KSharedConfig>
#include "ShortcutHandler/ShortcutHandler.h"

namespace Gui
{

QString SettingsDialog::warningStyleSheet = Util::cssWarningBorder() + QStringLiteral("font-weight: bold;");

/** @short Check a text field for being non empty. If it's empty, show an error to the user. */
template<typename T>
bool checkProblemWithEmptyTextField(T *field, const QString &message)
{
    if (field->text().isEmpty()) {
        QToolTip::showText(field->mapToGlobal(QPoint(10, field->height() / 2)), message, nullptr);
        return true;
    } else {
        return false;
    }
}

SettingsDialog::SettingsDialog(MainWindow *parent, Scope scope, Composer::SenderIdentitiesModel *sharedIdentities,
        Composer::SenderIdentitiesModel *accountIdentities, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel,
        QSettings *settings):
    QDialog(parent), mainWindow(parent), m_scope(scope), m_accountSettings(parent->accountSettings()),
        m_senderIdentities(sharedIdentities), m_accountIdentities(accountIdentities),
        m_favoriteTags(favoriteTagsModel), m_settings(settings)
{
    setWindowTitle(scope == Scope::Account ? i18nc("@title:window", "Account Settings")
                                           : i18nc("@title:window", "Settings"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    stack = new QTabWidget(this);
    layout->addWidget(stack);
    stack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    if (scope == Scope::Account) {
        addPage(new IdentitiesPage(this, *m_accountSettings, *m_settings, m_accountIdentities, m_senderIdentities),
                i18nc("@title:tab", "I&dentities"), m_accountSettings);
        m_imapPage = new ImapPage(this, *m_accountSettings);
        addPage(m_imapPage, tr("I&MAP"), m_accountSettings);
        addPage(new CachePage(this, *m_accountSettings), tr("&Offline"), m_accountSettings);
        addPage(new OutgoingPage(this, *m_accountSettings), tr("&SMTP"), m_accountSettings);
    } else {
        addPage(new GeneralPage(this, *m_settings), tr("&General"), m_settings);
        addPage(new FavoriteTagsPage(this, *m_settings, m_favoriteTags), tr("Favorite &tags"), m_settings);
    }

    buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    EMIT_LATER_NOARG(this, reloadPasswordsRequested);
}

SettingsDialog::Scope SettingsDialog::scope() const
{
    return m_scope;
}

void SettingsDialog::setOriginalPlugins(const QString &passwordPlugin, const QString &addressBookPlugin,
                                        const QString &spellcheckerPlugin)
{
    m_originalPasswordPlugin = passwordPlugin;
    m_originalAddressbookPlugin = addressBookPlugin;
    m_originalSpellcheckerPlugin = spellcheckerPlugin;
}

void SettingsDialog::adjustSizeToScrollAreas()
{
    QScrollArea *area = qobject_cast<QScrollArea*>(sender());
    Q_ASSERT(area && area->widget());

    // task #A: figure the "minimum" size for the tabwidget

    // #A.1: search scrollareas and align their size to their content
    // update size of the widget in the tabbed scrollarea
    area->widget()->adjustSize();

    // figure the size demand of this scroll area (content + margins)
    const auto margins = area->contentsMargins();
    QSize minSize(area->widget()->size() + QSize(margins.left() + margins.right(), margins.top() + margins.bottom()));

    // TODO: clamp this to 640x480 or QDesktopWidget::availableGeometry() dependent?

    // do not shrink (prevent nasty size jumps for no reason)
    minSize.setWidth(qMax(area->width(), minSize.width()));
    minSize.setHeight(qMax(area->height(), minSize.height()));

    // task #B: find the QStackedWidget inside the QTabWidget to determine its margins
    Q_FOREACH(const QObject *o, stack->children()) {
        if (const QStackedWidget *actualStack = qobject_cast<const QStackedWidget*>(o)) {
            minSize.setWidth(minSize.width() + stack->width() - actualStack->width());
            minSize.setHeight(minSize.height() + stack->height() - actualStack->height());
            break;
        }
    }

    // task #C: convince the dialog to the new size
    // #C.1: arrest the tabwidget
    stack->setMinimumSize(minSize);
    // #C.2: force a relayout of the dialog (do NOT use "adjustSize", which may still shrink)
    layout()->activate();
    // #C.3: release the tabwidget minimum size
    stack->setMinimumSize(QSize(0, 0));
}

Plugins::PluginManager *SettingsDialog::pluginManager()
{
    return mainWindow->pluginManager();
}

Imap::ImapAccess *SettingsDialog::imapAccess()
{
    return mainWindow->imapAccess();
}

void SettingsDialog::accept()
{
    m_saveSignalCount = 0;

    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        if (!page->checkValidity()) {
            stack->setCurrentWidget(page->asWidget());
            return;
        }
        connect(page->asWidget(), SIGNAL(saved()), this, SLOT(slotAccept())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
        ++m_saveSignalCount;
    }

#ifndef Q_OS_WIN
    // Try to wour around QSettings' inability to set umask for its file access. We don't want to set umask globally.
    QFile settingsFile(m_settings->fileName());
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
    QFile accountFile(m_accountSettings->fileName());
    accountFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif

    buttons->setEnabled(false);
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        page->asWidget()->setEnabled(false);
    }

    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        page->save(*m_pageSettings.value(page));
    }
    // Only now do the settings say whether the SMTP page uses a sign-in made on the IMAP page
    if (m_imapPage)
        m_imapPage->finish(*m_accountSettings);

    m_settings->sync();
    m_accountSettings->sync();
#ifndef Q_OS_WIN
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
    accountFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif
}

void SettingsDialog::slotAccept()
{
    disconnect(sender(), SIGNAL(saved()), this, SLOT(slotAccept())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
    if (--m_saveSignalCount > 0) {
        return;
    }

    QStringList passwordFailures;
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        QString message;
        if (page->passwordFailures(message)) {
            passwordFailures << message;
        }
    }
    if (!passwordFailures.isEmpty()) {
        Gui::Util::messageBoxWarning(this, tr("Saving passwords failed"),
                                     tr("<p>Couldn't save passwords. These were the error messages:</p>\n<p>%1</p>")
                                     .arg(passwordFailures.join(QStringLiteral("<br/>"))));
    }

    buttons->setEnabled(true);
    QDialog::accept();
}

void SettingsDialog::reject()
{
    // The changes were performed on the live data, so we have to make sure they are discarded when user cancels
#define HANDLE_PLUGIN(LOWERCASE, UPPERCASE) \
    if (!m_original##UPPERCASE##Plugin.isEmpty() && pluginManager()->LOWERCASE##Plugin() != m_original##UPPERCASE##Plugin) { \
        pluginManager()->set##UPPERCASE##Plugin(m_original##UPPERCASE##Plugin); \
    }
    HANDLE_PLUGIN(addressbook, Addressbook)
    HANDLE_PLUGIN(password, Password)
    HANDLE_PLUGIN(spellchecker, Spellchecker)
#undef HANDLE_PLUGIN
    m_senderIdentities->loadFromSettings(*m_settings);
    m_accountIdentities->loadFromSettings(*m_accountSettings);
    m_favoriteTags->loadFromSettings(*m_settings);
    if (m_imapPage)
        m_imapPage->finish(*m_accountSettings);
    QDialog::reject();
}

void SettingsDialog::addPage(ConfigurationWidgetInterface *page, const QString &title, QSettings *pageSettings)
{
    m_pageSettings.insert(page, pageSettings);
    stack->addTab(page->asWidget(), title);
    connect(page->asWidget(), SIGNAL(widgetsUpdated()), SLOT(adjustSizeToScrollAreas())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
    QMetaObject::invokeMethod(page->asWidget(), "updateWidgets", Qt::QueuedConnection);
    pages << page;
}

FavoriteTagsPage::FavoriteTagsPage(SettingsDialog *parent, QSettings &s, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel):
    QScrollArea(parent), Ui_FavoriteTagsPage(), m_favoriteTagsModel(favoriteTagsModel), m_parent(parent)
{
    Ui_FavoriteTagsPage::setupUi(this);
    Q_ASSERT(m_favoriteTagsModel);
    moveUpButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-up")));
    moveDownButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-down")));
    tagTableView->setModel(m_favoriteTagsModel);
    tagTableView->setItemDelegate(new ColoredItemDelegate(this));
    tagTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tagTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tagTableView->setGridStyle(Qt::NoPen);
    tagTableView->resizeRowsToContents();
    tagTableView->horizontalHeader()->setStretchLastSection(true);
    // show tag name in color instead
    tagTableView->hideColumn(Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR);

    connect(tagTableView, &QAbstractItemView::clicked, this, &FavoriteTagsPage::updateWidgets);
    connect(tagTableView, &QAbstractItemView::doubleClicked, this, &FavoriteTagsPage::editButtonClicked);
    connect(m_favoriteTagsModel, &QAbstractItemModel::modelReset, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::rowsInserted, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::rowsRemoved, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::dataChanged, this, &FavoriteTagsPage::updateWidgets);
    connect(moveUpButton, &QAbstractButton::clicked, this, [this](){ FavoriteTagsPage::moveTagBy(-1); });
    connect(moveDownButton, &QAbstractButton::clicked, this, [this](){ FavoriteTagsPage::moveTagBy(1); });
    connect(addButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::addButtonClicked);
    connect(editButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::editButtonClicked);
    connect(deleteButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::deleteButtonClicked);

    updateWidgets();
}

void FavoriteTagsPage::updateWidgets()
{
    bool enabled = tagTableView->currentIndex().isValid();
    deleteButton->setEnabled(enabled);
    editButton->setEnabled(enabled);
    bool upEnabled = m_favoriteTagsModel->rowCount() > 0 && tagTableView->currentIndex().row() > 0;
    bool downEnabled = m_favoriteTagsModel->rowCount() > 0 && tagTableView->currentIndex().isValid() &&
            tagTableView->currentIndex().row() < m_favoriteTagsModel->rowCount() - 1;
    moveUpButton->setEnabled(upEnabled);
    moveDownButton->setEnabled(downEnabled);

    tagTableView->resizeColumnToContents(Imap::Mailbox::FavoriteTagsModel::COLUMN_INDEX);
    tagTableView->resizeColumnToContents(Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME);

    emit widgetsUpdated();
}

void FavoriteTagsPage::moveTagBy(const int offset)
{
    int from = tagTableView->currentIndex().row();
    int to = tagTableView->currentIndex().row() + offset;

    m_favoriteTagsModel->moveTag(from, to);
    updateWidgets();
}

void FavoriteTagsPage::addButtonClicked()
{
    m_favoriteTagsModel->appendTag(Imap::Mailbox::ItemFavoriteTagItem());
    tagTableView->setCurrentIndex(m_favoriteTagsModel->index(m_favoriteTagsModel->rowCount() - 1, 0));
    EditFavoriteTag *dialog = new EditFavoriteTag(this, m_favoriteTagsModel, tagTableView->currentIndex());
    dialog->setDeleteOnReject();
    dialog->setWindowTitle(tr("Add New Tag"));
    dialog->show();
    updateWidgets();
}

void FavoriteTagsPage::editButtonClicked()
{
    EditFavoriteTag *dialog = new EditFavoriteTag(this, m_favoriteTagsModel, tagTableView->currentIndex());
    dialog->setWindowTitle(tr("Edit Tag"));
    dialog->show();
}

void FavoriteTagsPage::deleteButtonClicked()
{
    Q_ASSERT(tagTableView->currentIndex().isValid());
    m_favoriteTagsModel->removeTagAt(tagTableView->currentIndex().row());
    updateWidgets();
}

void FavoriteTagsPage::save(QSettings &s)
{
    m_favoriteTagsModel->saveToSettings(s);

    emit saved();
}

QWidget *FavoriteTagsPage::asWidget()
{
    return this;
}

bool FavoriteTagsPage::checkValidity() const
{
    return true;
}

bool FavoriteTagsPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

GeneralPage::GeneralPage(SettingsDialog *parent, QSettings &s):
    QScrollArea(parent), Ui_GeneralPage(), m_parent(parent)
{
    Ui_GeneralPage::setupUi(this);

    Plugins::PluginManager *pluginManager = parent->pluginManager();
    QMap<QString, QString>::const_iterator it;
    int i;

#define HANDLE_PLUGIN(LOWERCASE, UPPERCASE, DISABLE, NOTFOUND) \
    const QMap<QString, QString> &LOWERCASE##Plugins = pluginManager->available##UPPERCASE##Plugins(); \
    const QString &LOWERCASE##Plugin = pluginManager->LOWERCASE##Plugin(); \
    int LOWERCASE##Index = -1; \
    \
    for (it = LOWERCASE##Plugins.constBegin(), i = 0; it != LOWERCASE##Plugins.constEnd(); ++it, ++i) { \
        LOWERCASE##Box->addItem(it.value(), it.key()); \
        if (LOWERCASE##Index < 0 && LOWERCASE##Plugin == it.key()) \
            LOWERCASE##Index = i; \
    } \
    \
    LOWERCASE##Box->addItem(DISABLE); \
    \
    if (LOWERCASE##Plugin == QLatin1String("none")) \
        LOWERCASE##Index = LOWERCASE##Box->count()-1; \
    \
    if (LOWERCASE##Index == -1) { \
        if (!LOWERCASE##Plugin.isEmpty()) \
            LOWERCASE##Box->addItem(NOTFOUND.arg(LOWERCASE##Plugin), LOWERCASE##Plugin); \
        LOWERCASE##Index = LOWERCASE##Box->count()-1; \
    } \
    \
    LOWERCASE##Box->setCurrentIndex(LOWERCASE##Index);

    QString pluginNotFound = tr("Plugin not found (%1)");
    HANDLE_PLUGIN(addressbook, Addressbook, tr("Disable address book"), pluginNotFound)
    HANDLE_PLUGIN(password, Password, tr("Disable passwords"), pluginNotFound)
    HANDLE_PLUGIN(spellchecker, Spellchecker, tr("Disable spell checking"), pluginNotFound)
#undef HANDLE_PLUGIN

    m_parent->setOriginalPlugins(passwordPlugin, addressbookPlugin, spellcheckerPlugin);


    markReadCheckbox->setChecked(s.value(Common::SettingsNames::autoMarkReadEnabled, QVariant(true)).toBool());
    markReadSeconds->setValue(s.value(Common::SettingsNames::autoMarkReadSeconds, QVariant(0)).toUInt());
    connect(markReadCheckbox, &QAbstractButton::toggled, markReadSeconds, &QWidget::setEnabled);

    auto mboxDropAction = s.value(Common::SettingsNames::mboxDropAction, QVariant(QStringLiteral("ask"))).toString();

    connect(mboxDropActionCheckbox, &QAbstractButton::toggled, mboxDropActionBox, &QWidget::setEnabled);
    if (mboxDropAction != QStringLiteral("ask"))
        mboxDropActionCheckbox->setChecked(true);

    mboxDropActionBox->addItem(tr("Move"), QStringLiteral("move"));
    if (mboxDropAction == QStringLiteral("move"))
        mboxDropActionBox->setCurrentIndex(mboxDropActionBox->count() - 1);
    mboxDropActionBox->addItem(tr("Copy"), QStringLiteral("copy"));
    if (mboxDropAction == QStringLiteral("copy"))
        mboxDropActionBox->setCurrentIndex(mboxDropActionBox->count() - 1);

    guiSystrayCheckbox->setChecked(s.value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool());
    guiStartMinimizedCheckbox->setChecked(s.value(Common::SettingsNames::guiStartMinimized, QVariant(false)).toBool());
    {
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        guiRunInBackgroundCheckbox->setChecked(UiUtils::BackgroundRunning::runsInBackground(*config));
        // Starting after login starts in the background, so it needs running in the background
        guiStartAfterLoginCheckbox->setChecked(guiRunInBackgroundCheckbox->isChecked()
                                               && UiUtils::BackgroundRunning::startsAfterLogin(*config));
        guiStartAfterLoginCheckbox->setEnabled(guiRunInBackgroundCheckbox->isChecked());
        connect(guiRunInBackgroundCheckbox, &QAbstractButton::toggled, this, [this](bool checked) {
            guiStartAfterLoginCheckbox->setEnabled(checked);
            if (!checked)
                guiStartAfterLoginCheckbox->setChecked(false);
        });
        guiPlasmaSearchCheckbox->setChecked(UiUtils::PlasmaSearch::isEnabled(*config));
    }

    preferPlaintextCheckbox->setChecked(s.value(Common::SettingsNames::guiPreferPlaintextRendering).toBool());
    revealTrojitaVersions->setChecked(s.value(Common::SettingsNames::interopRevealVersions, QVariant(true)).toBool());

    connect(passwordBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &GeneralPage::passwordPluginChanged);

    connect(this, &GeneralPage::reloadPasswords, m_parent, &SettingsDialog::reloadPasswordsRequested);

    updateWidgets();
}

void GeneralPage::passwordPluginChanged()
{
    const QString &passwordPlugin = m_parent->pluginManager()->passwordPlugin();
    const QString &selectedPasswordPlugin = passwordBox->itemData(passwordBox->currentIndex()).toString();

    if (selectedPasswordPlugin != passwordPlugin) {
        m_parent->pluginManager()->setPasswordPlugin(selectedPasswordPlugin);
        emit reloadPasswords();
    }
}

void GeneralPage::updateWidgets()
{
    emit widgetsUpdated();
}

IdentitiesPage::IdentitiesPage(SettingsDialog *parent, QSettings &account, QSettings &shared,
                               Composer::SenderIdentitiesModel *accountIdentities,
                               Composer::SenderIdentitiesModel *sharedIdentities):
    QScrollArea(parent), Ui_IdentitiesPage(), m_accountIdentities(accountIdentities),
    m_sharedIdentities(sharedIdentities), m_identitiesModel(nullptr), m_shared(shared)
{
    Ui_IdentitiesPage::setupUi(this);
    Q_ASSERT(m_accountIdentities);
    Q_ASSERT(m_sharedIdentities);
    identitySourceLabel->setText(i18nc("@label:listbox", "Send from:"));
    identitySourceCombo->addItem(i18nc("@item:inlistbox identity source", "This account's own identities"),
                                 static_cast<int>(Composer::AccountIdentities::Source::Own));
    identitySourceCombo->addItem(i18nc("@item:inlistbox identity source", "Identities shared by all accounts"),
                                 static_cast<int>(Composer::AccountIdentities::Source::Shared));
    identitySourceCombo->addItem(i18nc("@item:inlistbox identity source", "No identity"),
                                 static_cast<int>(Composer::AccountIdentities::Source::None));
    addButton->setText(i18nc("@action:button", "Add"));
    editButton->setText(i18nc("@action:button", "Edit"));
    deleteButton->setText(i18nc("@action:button", "Delete"));
    moveUpButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-up")));
    moveDownButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-down")));
    moveUpButton->setToolTip(i18nc("@info:tooltip", "Move identity up"));
    moveDownButton->setToolTip(i18nc("@info:tooltip", "Move identity down"));
    identityTabelView->setSelectionBehavior(QAbstractItemView::SelectRows);
    identityTabelView->setSelectionMode(QAbstractItemView::SingleSelection);
    identityTabelView->setGridStyle(Qt::NoPen);
    identityTabelView->horizontalHeader()->setStretchLastSection(true);

    identitySourceCombo->setCurrentIndex(identitySourceCombo->findData(
            static_cast<int>(Composer::AccountIdentities::source(account))));

    for (auto *model : {m_accountIdentities, m_sharedIdentities}) {
        connect(model, &QAbstractItemModel::modelReset, this, &IdentitiesPage::updateWidgets);
        connect(model, &QAbstractItemModel::layoutChanged, this, &IdentitiesPage::updateWidgets);
        connect(model, &QAbstractItemModel::rowsInserted, this, &IdentitiesPage::updateWidgets);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &IdentitiesPage::updateWidgets);
        connect(model, &QAbstractItemModel::dataChanged, this, &IdentitiesPage::updateWidgets);
    }
    connect(identitySourceCombo, &QComboBox::currentIndexChanged, this, &IdentitiesPage::showIdentitiesInUse);
    connect(identityTabelView, &QAbstractItemView::clicked, this, &IdentitiesPage::updateWidgets);
    connect(identityTabelView, &QAbstractItemView::doubleClicked, this, &IdentitiesPage::editButtonClicked);
    connect(moveUpButton, &QAbstractButton::clicked, this, &IdentitiesPage::moveIdentityUp);
    connect(moveDownButton, &QAbstractButton::clicked, this, &IdentitiesPage::moveIdentityDown);
    connect(addButton, &QAbstractButton::clicked, this, &IdentitiesPage::addButtonClicked);
    connect(editButton, &QAbstractButton::clicked, this, &IdentitiesPage::editButtonClicked);
    connect(deleteButton, &QAbstractButton::clicked, this, &IdentitiesPage::deleteButtonClicked);

    showIdentitiesInUse();
}

Composer::AccountIdentities::Source IdentitiesPage::selectedSource() const
{
    return static_cast<Composer::AccountIdentities::Source>(identitySourceCombo->currentData().toInt());
}

void IdentitiesPage::showIdentitiesInUse()
{
    using Composer::AccountIdentities::Source;
    const Source source = selectedSource();
    // With no identity, the account's own list stays visible but read-only, so it can be reviewed before switching back.
    m_identitiesModel = source == Source::Shared ? m_sharedIdentities : m_accountIdentities;
    identityTabelView->setModel(m_identitiesModel);
    identityTabelView->hideColumn(Composer::SenderIdentitiesModel::COLUMN_ORGANIZATION);
    identityTabelView->hideColumn(Composer::SenderIdentitiesModel::COLUMN_SIGNATURE);
    identityTabelView->resizeRowsToContents();
    identityTabelView->setEnabled(source != Source::None);
    addButton->setEnabled(source != Source::None);
    switch (source) {
    case Source::Shared:
        scopeLabel->setText(i18nc("@info", "This account sends from the shared identities. "
                                           "Changes here apply to every account that uses them."));
        break;
    case Source::None:
        scopeLabel->setText(i18nc("@info", "This account does not use an identity. New messages start with an empty "
                                           "From address and no signature. This account's own identities are kept "
                                           "for when you switch back."));
        break;
    case Source::Own:
        scopeLabel->setText(
                i18nc("@info", "This account sends from its own identities. Other accounts are not affected."));
        break;
    }
    updateWidgets();
}

void IdentitiesPage::updateWidgets()
{
    bool enabled = identityTabelView->isEnabled() && identityTabelView->currentIndex().isValid();
    deleteButton->setEnabled(enabled);
    editButton->setEnabled(enabled);
    bool upEnabled = enabled && m_identitiesModel->rowCount() > 0 && identityTabelView->currentIndex().row() > 0;
    bool downEnabled = enabled && m_identitiesModel->rowCount() > 0 &&
            identityTabelView->currentIndex().row() < m_identitiesModel->rowCount() - 1;
    moveUpButton->setEnabled(upEnabled);
    moveDownButton->setEnabled(downEnabled);

    identityTabelView->resizeColumnToContents(Composer::SenderIdentitiesModel::COLUMN_NAME);

    emit widgetsUpdated();
}

void IdentitiesPage::moveIdentityUp()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() - 1;

    m_identitiesModel->moveIdentity(from, to);
    updateWidgets();
}

void IdentitiesPage::moveIdentityDown()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() + 1;

    m_identitiesModel->moveIdentity(from, to);
    updateWidgets();
}

void IdentitiesPage::addButtonClicked()
{
    m_identitiesModel->appendIdentity(Composer::ItemSenderIdentity());
    identityTabelView->setCurrentIndex(m_identitiesModel->index(m_identitiesModel->rowCount() - 1, 0));
    EditIdentity *dialog = new EditIdentity(this, m_identitiesModel, identityTabelView->currentIndex());
    dialog->setDeleteOnReject();
    dialog->setWindowTitle(tr("Add New Identity"));
    dialog->show();
    updateWidgets();
}

void IdentitiesPage::editButtonClicked()
{
    EditIdentity *dialog = new EditIdentity(this,  m_identitiesModel, identityTabelView->currentIndex());
    dialog->setWindowTitle(tr("Edit Identity"));
    dialog->show();
}

void IdentitiesPage::deleteButtonClicked()
{
    Q_ASSERT(identityTabelView->currentIndex().isValid());
    QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Delete Identity?"),
                                  tr("Are you sure you want to delete identity %1 <%2>?").arg(
                                      m_identitiesModel->index(identityTabelView->currentIndex().row(),
                                                               Composer::SenderIdentitiesModel::COLUMN_NAME).data().toString(),
                                      m_identitiesModel->index(identityTabelView->currentIndex().row(),
                                                               Composer::SenderIdentitiesModel::COLUMN_EMAIL).data().toString()),
                                  QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        m_identitiesModel->removeIdentityAt(identityTabelView->currentIndex().row());
        updateWidgets();
    }
}

void IdentitiesPage::save(QSettings &s)
{
    // Both lists are saved: an account that switches away from its own identities keeps them for later.
    Composer::AccountIdentities::setSource(s, selectedSource());
    m_accountIdentities->saveToSettings(s);
    m_sharedIdentities->saveToSettings(m_shared);
    emit saved();
}

QWidget *IdentitiesPage::asWidget()
{
    return this;
}

bool IdentitiesPage::checkValidity() const
{
    // A new account starts without identities; the composer then accepts a typed From address.
    return true;
}

bool IdentitiesPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

void GeneralPage::save(QSettings &s)
{
    s.setValue(Common::SettingsNames::autoMarkReadEnabled, markReadCheckbox->isChecked());
    s.setValue(Common::SettingsNames::autoMarkReadSeconds, markReadSeconds->value());
    s.setValue(Common::SettingsNames::mboxDropAction,
            mboxDropActionCheckbox->isChecked() ? mboxDropActionBox->currentData() : QStringLiteral("ask"));
    s.setValue(Common::SettingsNames::guiPreferPlaintextRendering, preferPlaintextCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiShowSystray, guiSystrayCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiStartMinimized, guiStartMinimizedCheckbox->isChecked());
    s.setValue(Common::SettingsNames::interopRevealVersions, revealTrojitaVersions->isChecked());
    {
        namespace Background = UiUtils::BackgroundRunning;
        const auto config = KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
        Background::setRunsInBackground(*config, guiRunInBackgroundCheckbox->isChecked());
        const bool startAfterLogin = guiRunInBackgroundCheckbox->isChecked() && guiStartAfterLoginCheckbox->isChecked();
        if (startAfterLogin != Background::startsAfterLogin(*config)) {
            // Turning it off removes the autostart entry; a failure keeps the earlier choice so that it can be retried
            if (StartAfterLogin::apply(startAfterLogin, this))
                Background::setStartsAfterLogin(*config, startAfterLogin);
        }
        UiUtils::PlasmaSearch::setEnabled(*config, guiPlasmaSearchCheckbox->isChecked());
    }

#define HANDLE_PLUGIN(LOWERCASE, UPPERCASE) \
    const QString &LOWERCASE##Plugin = m_parent->pluginManager()->LOWERCASE##Plugin(); \
    const QString &selected##UPPERCASE##Plugin = LOWERCASE##Box->itemData(LOWERCASE##Box->currentIndex()).toString(); \
    if (selected##UPPERCASE##Plugin != LOWERCASE##Plugin) { \
        m_parent->pluginManager()->set##UPPERCASE##Plugin(selected##UPPERCASE##Plugin); \
    }

    HANDLE_PLUGIN(addressbook, Addressbook);
    HANDLE_PLUGIN(password, Password);
    HANDLE_PLUGIN(spellchecker, Spellchecker);
#undef HANDLE_PLUGIN

    emit saved();
}

QWidget *GeneralPage::asWidget()
{
    return this;
}

bool GeneralPage::checkValidity() const
{
    return true;
}

bool GeneralPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

EditIdentity::EditIdentity(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, const QModelIndex &currentIndex):
    QDialog(parent), Ui_EditIdentity(), m_identitiesModel(identitiesModel), m_deleteOnReject(false)
{
    Ui_EditIdentity::setupUi(this);
    m_mapper = new QDataWidgetMapper(this);
    m_mapper->setModel(m_identitiesModel);
    m_mapper->addMapping(realNameLineEdit, Composer::SenderIdentitiesModel::COLUMN_NAME);
    m_mapper->addMapping(emailLineEdit, Composer::SenderIdentitiesModel::COLUMN_EMAIL);
    m_mapper->addMapping(organisationLineEdit, Composer::SenderIdentitiesModel::COLUMN_ORGANIZATION);
    m_mapper->addMapping(signaturePlainTextEdit, Composer::SenderIdentitiesModel::COLUMN_SIGNATURE);
    m_mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    m_mapper->setCurrentIndex(currentIndex.row());
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(realNameLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(emailLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(organisationLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(signaturePlainTextEdit, &QPlainTextEdit::textChanged, this, &EditIdentity::enableButton);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &QDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::accepted, m_mapper, &QDataWidgetMapper::submit);
    connect(this, &QDialog::rejected, this, &EditIdentity::onReject);
    setModal(true);
    signaturePlainTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

void EditIdentity::enableButton()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
        !realNameLineEdit->text().isEmpty() && !emailLineEdit->text().isEmpty());
}

/** @short If enabled, make sure that the current row gets deleted when the dialog is rejected */
void EditIdentity::setDeleteOnReject(const bool reject)
{
    m_deleteOnReject = reject;
}

void EditIdentity::onReject()
{
    if (m_deleteOnReject)
        m_identitiesModel->removeIdentityAt(m_mapper->currentIndex());
}

EditFavoriteTag::EditFavoriteTag(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel, const QModelIndex &currentIndex):
    QDialog(parent), Ui_EditFavoriteTag(), m_favoriteTagsModel(favoriteTagsModel), currentIndex(currentIndex), m_deleteOnReject(false)
{
    Ui_EditFavoriteTag::setupUi(this);

    nameLineEdit->setText(name());
    setColorButtonColor(color());
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(colorButton, &QAbstractButton::clicked, this, &EditFavoriteTag::colorButtonClick);
    connect(nameLineEdit, &QLineEdit::textChanged, this, &EditFavoriteTag::tryEnableButton);

    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &QDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &EditFavoriteTag::onAccept);
    connect(this, &QDialog::rejected, this, &EditFavoriteTag::onReject);
    setModal(true);
}

QString EditFavoriteTag::name()
{
    return m_favoriteTagsModel->data(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME)).toString();
}

QString EditFavoriteTag::color()
{
    return m_favoriteTagsModel->data(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR)).toString();
}

void EditFavoriteTag::setColorButtonColor(const QString color)
{
    colorButton->setProperty("colorName", color);
    QPalette pal = colorButton->palette();
    pal.setColor(QPalette::Button, QColor(color));
    colorButton->setAutoFillBackground(true);
    colorButton->setPalette(pal);
    colorButton->setFlat(true);
    colorButton->update();
}

void EditFavoriteTag::colorButtonClick()
{
    const QColor color = QColorDialog::getColor(QColor(colorButton->property("colorName").toString()), this, tr("Select tag color"));
    if (color.isValid()) {
        setColorButtonColor(color.name());
        tryEnableButton();
    }
}

void EditFavoriteTag::tryEnableButton()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
        !nameLineEdit->text().isEmpty() && QColor(colorButton->property("colorName").toString()).isValid()
    );
}

/** @short If enabled, make sure that the current row gets deleted when the dialog is rejected */
void EditFavoriteTag::setDeleteOnReject(const bool reject)
{
    m_deleteOnReject = reject;
}

void EditFavoriteTag::onAccept()
{
    m_favoriteTagsModel->setData(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME),
            nameLineEdit->text());
    m_favoriteTagsModel->setData(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR),
            colorButton->property("colorName"));
}

void EditFavoriteTag::onReject()
{
    if (m_deleteOnReject)
        m_favoriteTagsModel->removeTagAt(currentIndex.row());
}

ImapPage::ImapPage(SettingsDialog *parent, QSettings &s): QScrollArea(parent), Ui_ImapPage(), m_parent(parent)
{
    Ui_ImapPage::setupUi(this);
    method->insertItem(NETWORK, tr("Network Connection"));
    method->insertItem(PROCESS, tr("Local Process"));

    encryption->insertItem(NONE, tr("No encryption"));
    encryption->insertItem(STARTTLS, tr("Use encryption (STARTTLS)"));
    encryption->insertItem(SSL, tr("Force encryption (TLS)"));
    using Common::SettingsNames;
    int defaultImapPort = Common::PORT_IMAPS;

    if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodTCP) {
        method->setCurrentIndex(NETWORK);

        if (s.value(SettingsNames::imapStartTlsKey,true).toBool())
            encryption->setCurrentIndex(STARTTLS);
        else
            encryption->setCurrentIndex(NONE);

        defaultImapPort = Common::PORT_IMAP;
    } else if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodSSL) {
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSL);
    } else if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodProcess) {
        method->setCurrentIndex(PROCESS);
    } else {
        // Default settings -- let's assume SSL and hope that users who just press Cancel will configure when they see
        // the network error...
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSL);
    }

    imapHost->setText(s.value(SettingsNames::imapHostKey).toString());
    imapPort->setText(s.value(SettingsNames::imapPortKey, QString::number(defaultImapPort)).toString());
    imapPort->setValidator(new QIntValidator(1, 65535, this));
    connect(imapPort, &QLineEdit::textChanged, this, &ImapPage::maybeShowPortWarning);
    connect(encryption, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::maybeShowPortWarning);
    connect(method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::maybeShowPortWarning);
    connect(encryption, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::changePort);
    portWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    connect(imapPass, &QLineEdit::textChanged, this, &ImapPage::updateWidgets);
    imapUser->setText(s.value(SettingsNames::imapUserKey).toString());
    processPath->setText(s.value(SettingsNames::imapProcessKey).toString());

    imapCapabilitiesBlacklist->setText(s.value(SettingsNames::imapBlacklistedCapabilities).toStringList().join(QStringLiteral(" ")));
    imapUseSystemProxy->setChecked(s.value(SettingsNames::imapUseSystemProxy, true).toBool());
    imapNeedsNetwork->setChecked(s.value(SettingsNames::imapNeedsNetwork, true).toBool());
    imapIdleRenewal->setValue(s.value(SettingsNames::imapIdleRenewal, QVariant(29)).toInt());
    imapNumberRefreshInterval->setValue(m_parent->imapAccess()->numberRefreshInterval());
    accountIcon->setText(s.value(SettingsNames::imapAccountIcon).toString());
    archiveFolderName->setText(s.value(SettingsNames::imapArchiveFolderName).toString().isEmpty() ?
        SettingsNames::imapDefaultArchiveFolderName : s.value(SettingsNames::imapArchiveFolderName).toString());

    m_imapPort = s.value(SettingsNames::imapPortKey, QString::number(defaultImapPort)).value<quint16>();

    connect(method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::updateWidgets);

    // FIXME: use another account-id
    m_pwWatcher = m_parent->imapAccess()->passwordWatcher();
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::stateChanged, this, &ImapPage::updateWidgets);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingFailed, this, &ImapPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingDone, this, &ImapPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::readingDone, this, &ImapPage::slotSetPassword);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, imapPass, &QLineEdit::clear);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, m_pwWatcher, &UiUtils::PasswordWatcher::reloadPassword);

    imapAuthMethod = new QComboBox(this);
    imapAuthMethod->setObjectName(QStringLiteral("imapAuthMethod"));
    imapAuthMethod->addItem(tr("Password"), SettingsNames::authMethodPassword);
    imapAuthMethod->addItem(tr("OAuth 2.0 sign-in with the provider"), SettingsNames::authMethodOAuth);
    imapAuthMethod->setCurrentIndex(OAuth::isEnabled(s, OAuth::Service::Imap) ? 1 : 0);
    int authRow = 0;
    QFormLayout::ItemRole role;
    formLayout->getWidgetPosition(imapPass, &authRow, &role);
    formLayout->insertRow(authRow, tr("Authentication:"), imapAuthMethod);
    int oauthRow = 0;
    formLayout->getWidgetPosition(passwordPluginStatus, &oauthRow, &role);
    QPointer<Plugins::PluginManager> pluginManager = m_parent->pluginManager();
    m_oauth = new SettingsOAuthControls(formLayout, oauthRow + 1, s, m_parent->imapAccess()->credentialAccountId(),
                                        [pluginManager]() -> Plugins::PasswordPlugin * {
                                            return pluginManager ? pluginManager->password() : nullptr;
                                        }, this);
    m_oauth->setLoginHint(imapUser->text());
    connect(imapUser, &QLineEdit::textChanged, m_oauth, &SettingsOAuthControls::setLoginHint);
    connect(imapAuthMethod, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::updateWidgets);

    updateWidgets();
    maybeShowPortWarning();
}

bool ImapPage::usesOAuth() const
{
    return imapAuthMethod && imapAuthMethod->currentData().toString() == Common::SettingsNames::authMethodOAuth;
}

void ImapPage::slotSetPassword()
{
    imapPass->setText(m_pwWatcher->password());
}

void ImapPage::changePort()
{
    imapPort->setText(QString::number(encryption->currentIndex() == SSL ? Common::PORT_IMAPS : Common::PORT_IMAP));
}

void ImapPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);

    switch (method->currentIndex()) {
    case NETWORK:
        imapHost->setVisible(true);
        imapPort->setVisible(true);
        encryption->setVisible(true);
        lay->labelForField(imapHost)->setVisible(true);
        lay->labelForField(imapPort)->setVisible(true);
        lay->labelForField(encryption)->setVisible(true);
        processPath->setVisible(false);
        lay->labelForField(processPath)->setVisible(false);
        imapUseSystemProxy->setVisible(true);
        lay->labelForField(imapUseSystemProxy)->setVisible(true);
        // the "needs network" can very well apply to accounts using "local process" via SSH, so it is not disabled here
        break;
    default:
        imapHost->setVisible(false);
        imapPort->setVisible(false);
        encryption->setVisible(false);
        lay->labelForField(imapHost)->setVisible(false);
        lay->labelForField(imapPort)->setVisible(false);
        lay->labelForField(encryption)->setVisible(false);
        processPath->setVisible(true);
        lay->labelForField(processPath)->setVisible(true);
        imapUseSystemProxy->setVisible(false);
        lay->labelForField(imapUseSystemProxy)->setVisible(false);
    }

    switch (encryption->currentIndex()) {
    case NONE:
    case STARTTLS:
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAPS))
            imapPort->setText(QString::number(Common::PORT_IMAP));
        break;
    default:
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAP))
            imapPort->setText(QString::number(Common::PORT_IMAPS));
    }

    if (!m_pwWatcher->isPluginAvailable())
        imapPass->setText(QString());

    passwordWarning->setVisible(!imapPass->text().isEmpty());
    if (m_pwWatcher->isStorageEncrypted()) {
        passwordWarning->setStyleSheet(QString());
        passwordWarning->setText(tr("This password will be saved in encrypted storage. "
            "If you do not enter password here, Plasma Impresario will prompt for one when needed."));
    } else {
        passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
        passwordWarning->setText(tr("This password will be saved in clear text. "
            "If you do not enter password here, Plasma Impresario will prompt for one when needed."));
    }

    passwordPluginStatus->setVisible(!m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || !m_pwWatcher->didReadOk() || !m_pwWatcher->didWriteOk());
    passwordPluginStatus->setText(m_pwWatcher->progressMessage());

    imapPass->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());
    imapPassLabel->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());

    if (m_oauth) {
        // With OAuth, the stored sign-in replaces the password
        const bool oauth = usesOAuth();
        lay->setRowVisible(imapPass, !oauth);
        if (oauth) {
            passwordWarning->setVisible(false);
            passwordPluginStatus->setVisible(false);
        }
        m_oauth->setActive(oauth);
    }

    emit widgetsUpdated();
}

void ImapPage::save(QSettings &s)
{
    using Common::SettingsNames;
    if (s.value(SettingsNames::imapHostKey) != imapHost->text()) {
        s.remove(Common::SettingsNames::imapSslPemPubKey);
    }
    switch (method->currentIndex()) {
    case NETWORK:
        if (imapHost->text().isEmpty()) {
            s.remove(SettingsNames::imapMethodKey);
        } else if (encryption->currentIndex() == NONE){
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
            s.setValue(SettingsNames::imapStartTlsKey, false);
        } else if (encryption->currentIndex() == STARTTLS){
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
            s.setValue(SettingsNames::imapStartTlsKey, true);
        } else {
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodSSL);
            s.setValue(SettingsNames::imapStartTlsKey, true);
        }
        s.setValue(SettingsNames::imapHostKey, imapHost->text());
        s.setValue(SettingsNames::imapPortKey, imapPort->text());
        s.setValue(SettingsNames::imapUseSystemProxy, imapUseSystemProxy->isChecked());
        break;
    default:
        if (processPath->text().isEmpty()) {
            s.remove(SettingsNames::imapMethodKey);
        } else {
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodProcess);
        }
        s.setValue(SettingsNames::imapProcessKey, processPath->text());
    }
    s.setValue(SettingsNames::imapUserKey, imapUser->text());
    s.setValue(SettingsNames::imapBlacklistedCapabilities, imapCapabilitiesBlacklist->text().split(QStringLiteral(" ")));
    s.setValue(SettingsNames::imapNeedsNetwork, imapNeedsNetwork->isChecked());
    s.setValue(SettingsNames::imapIdleRenewal, imapIdleRenewal->value());
    m_parent->imapAccess()->setNumberRefreshInterval(imapNumberRefreshInterval->value());

    s.setValue(SettingsNames::imapAccountIcon, accountIcon->text().isEmpty() ? QVariant() : QVariant(accountIcon->text()));
    s.setValue(SettingsNames::imapArchiveFolderName, archiveFolderName->text());

    if (usesOAuth())
        s.setValue(SettingsNames::imapAuthMethodKey, SettingsNames::authMethodOAuth);
    else
        s.remove(SettingsNames::imapAuthMethodKey);
    m_oauth->save(s);

    if (usesOAuth()) {
        emit saved();
    } else if (m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin()) {
        m_pwWatcher->setPassword(imapPass->text());
    } else {
        emit saved();
    }
}

QWidget *ImapPage::asWidget()
{
    return this;
}

void ImapPage::finish(const QSettings &s)
{
    if (m_oauth)
        m_oauth->finish(s);
}

bool ImapPage::checkValidity() const
{
    switch (method->currentIndex()) {
    case NETWORK:
        // We don't require the username, and that's on purpose. Some servers *could* possibly support PREAUTH :)
        if (checkProblemWithEmptyTextField(imapHost, tr("The IMAP server hostname is missing here")))
            return false;
        if (usesOAuth() && m_oauth->accountConfig().clientId.isEmpty()
                && checkProblemWithEmptyTextField(m_oauth->clientId, tr("The OAuth client ID is missing here")))
            return false;
        if (m_oauth->hasUnsavedClientSecret()) {
            QToolTip::showText(m_oauth->clientSecret->mapToGlobal(QPoint(10, m_oauth->clientSecret->height() / 2)),
                               tr("Sign in to store this client secret, or clear it"), nullptr);
            return false;
        }
        break;
    default:
        // PREAUTH must definitely be supported here -- think imap-over-ssh-with-ssh-keys etc.
        if (checkProblemWithEmptyTextField(processPath,
                               tr("The command line to the IMAP server is missing here. Perhaps you need to use SSL or TCP?"))) {
            return false;
        }
        break;
    }
    return true;
}

void ImapPage::maybeShowPortWarning()
{
    if (method->currentIndex() == PROCESS) {
        portWarning->setVisible(false);
        return;
    }

    if (encryption->currentIndex() == SSL) {
        portWarning->setVisible(imapPort->text() != QString::number(Common::PORT_IMAPS));
        portWarning->setText(tr("This port is nonstandard. The default port for IMAP secured over SSL/TLS is %1.").arg(Common::PORT_IMAPS));
    } else {
        portWarning->setVisible(imapPort->text() != QString::number(Common::PORT_IMAP));
        if (encryption->currentIndex() == STARTTLS) {
            portWarning->setText(tr("This port is nonstandard. The default port for IMAP secured via STARTTLS is %1.").arg(Common::PORT_IMAP));
        } else {
            portWarning->setText(tr("This port is nonstandard. The default port for IMAP over cleartext is %1.").arg(Common::PORT_IMAP));
        }
    }
}

bool ImapPage::passwordFailures(QString &message) const
{
    if (usesOAuth() || !m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || m_pwWatcher->didWriteOk()) {
        return false;
    } else {
        message = m_pwWatcher->progressMessage();
        return true;
    }
}


CachePage::CachePage(QWidget *parent, QSettings &s): QScrollArea(parent), Ui_CachePage()
{
    Ui_CachePage::setupUi(this);

    using Common::SettingsNames;

    QString val = s.value(SettingsNames::cacheOfflineKey).toString();
    if (val == SettingsNames::cacheOfflineAll) {
        offlineEverything->setChecked(true);
    } else if (val == SettingsNames::cacheOfflineNone) {
        offlineNope->setChecked(true);
    } else {
        offlineXDays->setChecked(true);
    }

    offlineNumberOfDays->setValue(s.value(SettingsNames::cacheOfflineNumberDaysKey, QVariant(30)).toInt());

    searchIndex = new QCheckBox(i18nc("@option:check", "&Index downloaded mail for searching offline"), offlineModeGroup);
    searchIndex->setObjectName(QStringLiteral("searchIndex"));
    searchIndex->setChecked(s.value(SettingsNames::searchIndexKey, false).toBool());
    searchIndex->setToolTip(i18nc("@info:tooltip",
                                  "Keeps an index of the headers and the downloaded text of the cached messages on "
                                  "this computer, so that searches work without a connection. Nothing is downloaded "
                                  "for the index. Turning it off deletes the index."));
    searchIndexDecrypted = new QCheckBox(i18nc("@option:check", "Also index the &decrypted text of encrypted messages I open"),
                                         offlineModeGroup);
    searchIndexDecrypted->setObjectName(QStringLiteral("searchIndexDecrypted"));
    searchIndexDecrypted->setChecked(s.value(SettingsNames::searchIndexDecryptedKey, false).toBool());
    searchIndexDecrypted->setToolTip(i18nc("@info:tooltip",
                                           "Adds the text of an encrypted message to the index once you open and decrypt "
                                           "it, so that offline searches find it. The index itself is not encrypted, so "
                                           "anyone who can read your files can read that text. Turning this off removes "
                                           "the decrypted text from the index."));
    auto *attachmentRow = new QWidget(offlineModeGroup);
    auto *attachmentLayout = new QHBoxLayout(attachmentRow);
    attachmentLayout->setContentsMargins(0, 0, 0, 0);
    searchIndexAttachments = new QCheckBox(i18nc("@option:check followed by a size limit",
                                                 "Also index the text of downloaded &attachments up to"),
                                           attachmentRow);
    searchIndexAttachments->setObjectName(QStringLiteral("searchIndexAttachments"));
    searchIndexAttachments->setChecked(s.value(SettingsNames::searchIndexAttachmentsKey, false).toBool());
    searchIndexAttachments->setToolTip(i18nc("@info:tooltip",
                                             "Reads the text of attached files once you download them, such as by "
                                             "opening or saving them, using the text extractors of Plasma's file "
                                             "search. Nothing is downloaded for this, and larger files are skipped. "
                                             "Offline Search Index lists the formats which no installed extractor "
                                             "can read."));
    searchIndexAttachmentLimit = new QSpinBox(attachmentRow);
    searchIndexAttachmentLimit->setObjectName(QStringLiteral("searchIndexAttachmentLimit"));
    searchIndexAttachmentLimit->setRange(1, 1024);
    searchIndexAttachmentLimit->setSuffix(i18nc("@item:valuesuffix size in megabytes", " MB"));
    searchIndexAttachmentLimit->setValue(s.value(SettingsNames::searchIndexAttachmentLimitKey, 10).toInt());
    searchIndexAttachmentLimit->setToolTip(i18nc("@info:tooltip", "Largest attached file to read, per file"));
    attachmentLayout->addWidget(searchIndexAttachments);
    attachmentLayout->addWidget(searchIndexAttachmentLimit);
    attachmentLayout->addStretch();
    searchIndexNote = new QLabel(offlineModeGroup);
    searchIndexNote->setObjectName(QStringLiteral("searchIndexNote"));
    searchIndexNote->setWordWrap(true);
    searchIndexNote->setEnabled(false);
    formLayout->addRow(searchIndex);
    formLayout->addRow(searchIndexDecrypted);
    formLayout->addRow(attachmentRow);
    formLayout->addRow(searchIndexNote);

    val = s.value(SettingsNames::watchedFoldersKey).toString();
    if (val == Common::SettingsNames::watchAll) {
        watchAll->setChecked(true);
    } else if (val == Common::SettingsNames::watchSubscribed) {
        watchSubscribed->setChecked(true);
    } else {
        watchInbox->setChecked(true);
    }

    updateWidgets();

    connect(offlineNope, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
    connect(offlineXDays, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
    connect(offlineEverything, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
    connect(searchIndex, &QAbstractButton::toggled, this, &CachePage::updateWidgets);
    connect(searchIndexAttachments, &QAbstractButton::toggled, this, &CachePage::updateWidgets);
}

void CachePage::updateWidgets()
{
    offlineNumberOfDays->setEnabled(offlineXDays->isChecked());
    // The index lives in the offline cache, so there is nothing to index without one
    searchIndex->setEnabled(!offlineNope->isChecked());
    searchIndexDecrypted->setEnabled(!offlineNope->isChecked() && searchIndex->isChecked());
    searchIndexAttachments->setEnabled(!offlineNope->isChecked() && searchIndex->isChecked());
    searchIndexAttachmentLimit->setEnabled(searchIndexAttachments->isEnabled() && searchIndexAttachments->isChecked());
    searchIndexNote->setText(offlineNope->isChecked()
                                 ? i18nc("@info", "Without mail kept offline there is nothing to index; an existing "
                                                  "index is deleted.")
                                 : i18nc("@info", "Only mail kept offline is indexed; nothing extra is downloaded."));
    emit widgetsUpdated();
}

void CachePage::save(QSettings &s)
{
    using Common::SettingsNames;

    if (offlineEverything->isChecked())
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineAll);
    else if (offlineXDays->isChecked())
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineXDays);
    else
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineNone);

    s.setValue(SettingsNames::cacheOfflineNumberDaysKey, offlineNumberOfDays->value());
    s.setValue(SettingsNames::searchIndexKey, searchIndex->isChecked());
    s.setValue(SettingsNames::searchIndexDecryptedKey, searchIndexDecrypted->isChecked());
    s.setValue(SettingsNames::searchIndexAttachmentsKey, searchIndexAttachments->isChecked());
    s.setValue(SettingsNames::searchIndexAttachmentLimitKey, searchIndexAttachmentLimit->value());

    if (watchAll->isChecked()) {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchAll);
    } else if (watchSubscribed->isChecked()) {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchSubscribed);
    } else {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchOnlyInbox);
    }

    emit saved();
}

QWidget *CachePage::asWidget()
{
    return this;
}

bool CachePage::checkValidity() const
{
    // Nothing really special for this class
    return true;
}

bool CachePage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

OutgoingPage::OutgoingPage(SettingsDialog *parent, QSettings &s): QScrollArea(parent), Ui_OutgoingPage(), m_parent(parent)
{
    using Common::SettingsNames;
    Ui_OutgoingPage::setupUi(this);
    m_smtpAccountSettings = new MSA::Account(this, &s, m_parent->imapAccess()->accountName());

    portWarningLabel->setStyleSheet(SettingsDialog::warningStyleSheet);

    method->insertItem(NETWORK, tr("Network"));
    method->insertItem(SENDMAIL, tr("Local sendmail-compatible"));
    method->insertItem(IMAP_SENDMAIL, tr("IMAP SENDMAIL Extension"));;

    encryption->insertItem(SMTP, tr("No encryption"));
    encryption->insertItem(SMTP_STARTTLS, tr("Use encryption (STARTTLS)"));
    encryption->insertItem(SSMTP, tr("Force encryption (TLS)"));
    encryption->setCurrentIndex(SSMTP);

    connect(method, static_cast<void (QComboBox::*)(const int)>(&QComboBox::currentIndexChanged), this, &OutgoingPage::slotSetSubmissionMethod);
    connect(encryption, static_cast<void (QComboBox::*)(const int)>(&QComboBox::currentIndexChanged), this, &OutgoingPage::slotSetSubmissionMethod);

    connect(m_smtpAccountSettings, &MSA::Account::submissionMethodChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::saveToImapChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::authenticateEnabledChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::reuseImapAuthenticationChanged, this, &OutgoingPage::updateWidgets);
    connect(smtpPass, &QLineEdit::textChanged, this, &OutgoingPage::updateWidgets);
    connect(smtpHost, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setServer);
    connect(smtpUser, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setUsername);
    connect(smtpPort, &LineEdit::textEditingFinished, this, &OutgoingPage::setPortByText);
    connect(m_smtpAccountSettings, &MSA::Account::showPortWarning, this, &OutgoingPage::showPortWarning);
    connect(smtpAuth, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setAuthenticateEnabled);
    connect(smtpAuthReuseImapCreds, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setReuseImapAuthentication);
    connect(saveToImap, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setSaveToImap);
    connect(saveFolderName, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setSentMailboxName);
    connect(smtpBurl, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setUseBurl);
    connect(sendmail, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setPathToSendmail);

    m_pwWatcher = new UiUtils::PasswordWatcher(this, m_parent->pluginManager(),
                                               m_parent->imapAccess()->credentialAccountId(),
                                               QStringLiteral("smtp"));
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::stateChanged, this, &OutgoingPage::updateWidgets);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingFailed, this, &OutgoingPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingDone, this, &OutgoingPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::readingDone, this, &OutgoingPage::slotSetPassword);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, smtpPass, &QLineEdit::clear);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, m_pwWatcher, &UiUtils::PasswordWatcher::reloadPassword);

    smtpAuthMethod = new QComboBox(this);
    smtpAuthMethod->setObjectName(QStringLiteral("smtpAuthMethod"));
    smtpAuthMethod->addItem(tr("Password"), SettingsNames::authMethodPassword);
    smtpAuthMethod->addItem(tr("OAuth 2.0 sign-in with the provider"), SettingsNames::authMethodOAuth);
    smtpAuthMethod->setCurrentIndex(OAuth::isEnabled(s, OAuth::Service::Smtp) ? 1 : 0);
    smtpOAuthNote = new QLabel(tr("Sending uses the OAuth provider, client and sign-in configured on the IMAP page."),
                               this);
    smtpOAuthNote->setObjectName(QStringLiteral("smtpOAuthNote"));
    smtpOAuthNote->setWordWrap(true);
    int authRow = 0;
    QFormLayout::ItemRole role;
    formLayout->getWidgetPosition(smtpAuthReuseImapCreds, &authRow, &role);
    formLayout->insertRow(authRow + 1, tr("Authentication method:"), smtpAuthMethod);
    formLayout->insertRow(authRow + 2, smtpOAuthNote);
    connect(smtpAuthMethod, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &OutgoingPage::updateWidgets);

    updateWidgets();
}

bool OutgoingPage::usesOAuth() const
{
    return smtpAuthMethod && smtpAuthMethod->currentData().toString() == Common::SettingsNames::authMethodOAuth;
}

void OutgoingPage::slotSetPassword()
{
    smtpPass->setText(m_pwWatcher->password());
}

void OutgoingPage::slotSetSubmissionMethod()
{
    switch (method->currentIndex()) {
    case SENDMAIL:
        m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SENDMAIL);
        break;
    case IMAP_SENDMAIL:
        m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::IMAP_SENDMAIL);
        break;
    case NETWORK:
        switch (encryption->currentIndex()) {
        case SMTP:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SMTP);
            break;
        case SMTP_STARTTLS:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SMTP_STARTTLS);
            break;
        case SSMTP:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SSMTP);
            break;
        }
        break;
    default:
        Q_ASSERT(false);
    }
    // Toggle the default ports upon changing the delivery method
    smtpPort->setText(QString::number(m_smtpAccountSettings->port()));
}

void OutgoingPage::setPortByText(const QString &text)
{
    m_smtpAccountSettings->setPort(text.toUShort());
}

void OutgoingPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);

    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SMTP);
        break;
    case MSA::Account::Method::SMTP_STARTTLS:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SMTP_STARTTLS);
        break;
    case MSA::Account::Method::SSMTP:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSMTP);
        break;
    case MSA::Account::Method::SENDMAIL:
        method->setCurrentIndex(SENDMAIL);
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        break;
    case MSA::Account::Method::IMAP_SENDMAIL:
        method->setCurrentIndex(IMAP_SENDMAIL);
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        break;
    }

    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
    case MSA::Account::Method::SMTP_STARTTLS:
    case MSA::Account::Method::SSMTP:
    {
        encryption->setVisible(true);
        encryptionLabel->setVisible(true);
        smtpHost->setVisible(true);
        lay->labelForField(smtpHost)->setVisible(true);
        smtpHost->setText(m_smtpAccountSettings->server());
        smtpPort->setVisible(true);
        lay->labelForField(smtpPort)->setVisible(true);
        smtpPort->setText(QString::number(m_smtpAccountSettings->port()));
        smtpPort->setValidator(new QIntValidator(1, 65535, this));
        smtpAuth->setVisible(true);
        lay->labelForField(smtpAuth)->setVisible(true);
        bool authEnabled = m_smtpAccountSettings->authenticateEnabled();
        smtpAuth->setChecked(authEnabled);
        smtpAuthReuseImapCreds->setVisible(authEnabled);
        lay->labelForField(smtpAuthReuseImapCreds)->setVisible(authEnabled);
        bool reuseImapCreds = m_smtpAccountSettings->reuseImapAuthentication();
        smtpAuthReuseImapCreds->setChecked(reuseImapCreds);
        smtpUser->setVisible(authEnabled && !reuseImapCreds);
        lay->labelForField(smtpUser)->setVisible(authEnabled && !reuseImapCreds);
        smtpUser->setText(m_smtpAccountSettings->username());
        sendmail->setVisible(false);
        lay->labelForField(sendmail)->setVisible(false);
        saveToImap->setVisible(true);
        lay->labelForField(saveToImap)->setVisible(true);
        saveToImap->setChecked(m_smtpAccountSettings->saveToImap());
        smtpBurl->setVisible(saveToImap->isChecked());
        lay->labelForField(smtpBurl)->setVisible(saveToImap->isChecked());
        smtpBurl->setChecked(m_smtpAccountSettings->useBurl());

        if (!m_pwWatcher->isPluginAvailable())
            smtpPass->setText(QString());

        passwordWarning->setVisible(authEnabled && !reuseImapCreds && !smtpPass->text().isEmpty());
        if (m_pwWatcher->isStorageEncrypted()) {
            passwordWarning->setStyleSheet(QString());
            passwordWarning->setText(tr("This password will be saved in encrypted storage. "
                "If you do not enter password here, Plasma Impresario will prompt for one when needed."));
        } else {
            passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
            passwordWarning->setText(tr("This password will be saved in clear text. "
                "If you do not enter password here, Plasma Impresario will prompt for one when needed."));
        }

        passwordPluginStatus->setVisible(authEnabled && !reuseImapCreds &&
                                         (!m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || !m_pwWatcher->didReadOk() || !m_pwWatcher->didWriteOk()));
        passwordPluginStatus->setText(m_pwWatcher->progressMessage());

        smtpPass->setVisible(authEnabled && !reuseImapCreds);
        smtpPass->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());
        lay->labelForField(smtpPass)->setVisible(authEnabled && !reuseImapCreds);
        lay->labelForField(smtpPass)->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());

        if (smtpAuthMethod) {
            lay->setRowVisible(smtpAuthMethod, authEnabled);
            smtpOAuthNote->setVisible(authEnabled && usesOAuth());
            if (authEnabled && usesOAuth()) {
                // The account's OAuth sign-in replaces the SMTP password
                smtpPass->setVisible(false);
                lay->labelForField(smtpPass)->setVisible(false);
                passwordWarning->setVisible(false);
                passwordPluginStatus->setVisible(false);
            }
        }

        break;
    }
    case MSA::Account::Method::SENDMAIL:
    case MSA::Account::Method::IMAP_SENDMAIL:
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        smtpHost->setVisible(false);
        lay->labelForField(smtpHost)->setVisible(false);
        smtpPort->setVisible(false);
        lay->labelForField(smtpPort)->setVisible(false);
        showPortWarning(QString());
        smtpAuth->setVisible(false);
        lay->labelForField(smtpAuth)->setVisible(false);
        smtpUser->setVisible(false);
        lay->labelForField(smtpUser)->setVisible(false);
        smtpPass->setVisible(false);
        lay->labelForField(smtpPass)->setVisible(false);
        passwordWarning->setVisible(false);
        passwordPluginStatus->setVisible(false);
        if (smtpAuthMethod) {
            lay->setRowVisible(smtpAuthMethod, false);
            smtpOAuthNote->setVisible(false);
        }
        if (m_smtpAccountSettings->submissionMethod() == MSA::Account::Method::SENDMAIL) {
            sendmail->setVisible(true);
            lay->labelForField(sendmail)->setVisible(true);
            sendmail->setText(m_smtpAccountSettings->pathToSendmail());
            if (sendmail->text().isEmpty())
                sendmail->setText(Common::SettingsNames::sendmailDefaultCmd);
            saveToImap->setVisible(true);
            saveToImap->setChecked(m_smtpAccountSettings->saveToImap());
            lay->labelForField(saveToImap)->setVisible(true);
        } else {
            sendmail->setVisible(false);
            lay->labelForField(sendmail)->setVisible(false);
            saveToImap->setChecked(true);
            saveToImap->setVisible(false);
            lay->labelForField(saveToImap)->setVisible(false);
        }
        smtpBurl->setVisible(false);
        lay->labelForField(smtpBurl)->setVisible(false);
        smtpBurl->setChecked(m_smtpAccountSettings->useBurl());
        passwordPluginStatus->setVisible(false);
    }
    saveFolderName->setVisible(saveToImap->isChecked());
    lay->labelForField(saveFolderName)->setVisible(saveToImap->isChecked());
    saveFolderName->setText(m_smtpAccountSettings->sentMailboxName());

    emit widgetsUpdated();

}

void OutgoingPage::save(QSettings &s)
{
    m_smtpAccountSettings->saveSettings();

    const bool oauth = method->currentIndex() == NETWORK && smtpAuth->isChecked() && usesOAuth();
    if (oauth)
        s.setValue(Common::SettingsNames::smtpAuthMethodKey, Common::SettingsNames::authMethodOAuth);
    else
        s.remove(Common::SettingsNames::smtpAuthMethodKey);

    if (oauth) {
        emit saved();
    } else if (smtpAuth->isVisibleTo(this) && smtpAuth->isChecked() && m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin()) {
        m_pwWatcher->setPassword(smtpPass->text());
    } else {
        emit saved();
    }
}

void OutgoingPage::showPortWarning(const QString &warning)
{
    if (!warning.isEmpty()) {
        portWarningLabel->setVisible(true);
        portWarningLabel->setText(warning);
    } else {
        portWarningLabel->setVisible(false);
    }

}

QWidget *OutgoingPage::asWidget()
{
    return this;
}

bool OutgoingPage::checkValidity() const
{
    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
    case MSA::Account::Method::SMTP_STARTTLS:
    case MSA::Account::Method::SSMTP:
        if (checkProblemWithEmptyTextField(smtpHost, tr("The SMTP server hostname is missing here")))
            return false;
        if (smtpAuth->isChecked() && !smtpAuthReuseImapCreds->isChecked() && checkProblemWithEmptyTextField(smtpUser, tr("The SMTP username is missing here")))
            return false;
        break;
    case MSA::Account::Method::SENDMAIL:
        if (checkProblemWithEmptyTextField(sendmail, tr("The SMTP server hostname is missing here")))
            return false;
        break;
    case MSA::Account::Method::IMAP_SENDMAIL:
        break;
    }

    if (saveToImap->isChecked() && checkProblemWithEmptyTextField(saveFolderName, tr("Please specify the folder name here")))
        return false;

    return true;
}

bool OutgoingPage::passwordFailures(QString &message) const
{
    // The const_cast is needed as Qt4 does not define the arguement of isVisibleTo as const
    if (usesOAuth() || !smtpAuth->isVisibleTo(const_cast<Gui::OutgoingPage*>(this)) || !smtpAuth->isChecked() || !m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || m_pwWatcher->didWriteOk()) {
        return false;
    } else {
        message = m_pwWatcher->progressMessage();
        return true;
    }
}

}
