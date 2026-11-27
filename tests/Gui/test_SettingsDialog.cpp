/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include "Common/SettingsNames.h"
#include "Gui/SettingsGroups.h"
#include "UiUtils/Keymap.h"
#include "UiUtils/SettingsStore.h"

using namespace Gui;
namespace SettingsStore = UiUtils::SettingsStore;

namespace {

KSharedConfig::Ptr config()
{
    return KSharedConfig::openConfig(QStringLiteral("plasma-impresariorc"));
}

void clearConfig()
{
    const auto cfg = config();
    for (const QString &group : cfg->groupList())
        cfg->deleteGroup(group);
    cfg->sync();
}

template<typename T> T *child(QWidget *parent, const QString &name)
{
    auto *widget = parent->findChild<T *>(name);
    if (!widget)
        qWarning() << "No child" << name;
    return widget;
}

} // namespace

class SettingsDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain(QByteArrayLiteral("plasma-impresario"));
        QVERIFY(m_directory.isValid());
    }

    void init()
    {
        clearConfig();
        m_legacy = std::make_unique<QSettings>(m_directory.filePath(QStringLiteral("main.ini")), QSettings::IniFormat);
        m_legacy->clear();
        m_work = std::make_unique<QSettings>(m_directory.filePath(QStringLiteral("work.ini")), QSettings::IniFormat);
        m_work->clear();
        m_context = SettingsContext();
        m_context.accounts = {{QStringLiteral("work"), QStringLiteral("Work")},
                              {QStringLiteral("home"), QStringLiteral("Home")}};
        m_context.folders = [](const QString &accountId) {
            return accountId == QLatin1String("work") ? QStringList{QStringLiteral("INBOX"), QStringLiteral("Lists")}
                                                       : QStringList{QStringLiteral("INBOX")};
        };
        m_context.accountSettings = [this](const QString &accountId) {
            return accountId == QLatin1String("work") ? m_work.get() : nullptr;
        };
        m_context.legacy = m_legacy.get();
    }

    void accountOverridesAreSavedAndRemoved()
    {
        AccountsGroup group(m_context, nullptr);
        auto *scope = child<QComboBox>(&group, QStringLiteral("overrideScope"));
        QVERIFY(scope);
        // The whole account, then its known folders
        QCOMPARE(scope->count(), 3);
        QCOMPARE(scope->itemData(2).toString(), QStringLiteral("Lists"));

        auto *check = child<QCheckBox>(&group, QStringLiteral("override-Reading/AutoMarkRead"));
        QVERIFY(check);
        QVERIFY(!check->isChecked());
        auto *value = child<QComboBox>(&group, QStringLiteral("overrideValue-Reading/AutoMarkRead"));
        QVERIFY(value && !value->isEnabled());
        check->setChecked(true);
        QVERIFY(value->isEnabled());
        value->setCurrentIndex(value->findData(false));

        scope->setCurrentIndex(2);
        auto *folderCheck = child<QCheckBox>(&group, QStringLiteral("override-Reading/PreferPlainText"));
        QVERIFY(folderCheck);
        folderCheck->setChecked(true);
        auto *folderValue = child<QComboBox>(&group, QStringLiteral("overrideValue-Reading/PreferPlainText"));
        folderValue->setCurrentIndex(folderValue->findData(true));

        QSettings unused;
        group.save(unused);
        QCOMPARE(SettingsStore::accountValues(*config(), QStringLiteral("work"))
                     .value(QStringLiteral("Reading/AutoMarkRead")),
                 QVariant(false));
        QCOMPARE(SettingsStore::folderValues(*config(), QStringLiteral("work"), QStringLiteral("Lists"))
                     .value(QStringLiteral("Reading/PreferPlainText")),
                 QVariant(true));
        QCOMPARE(SettingsStore::value(*config(), m_legacy.get(), QStringLiteral("Reading/AutoMarkRead"),
                                      QStringLiteral("home")),
                 QVariant(true));

        // A new page shows the overrides, and turning one off removes it
        AccountsGroup again(m_context, nullptr);
        auto *stored = child<QCheckBox>(&again, QStringLiteral("override-Reading/AutoMarkRead"));
        QVERIFY(stored && stored->isChecked());
        stored->setChecked(false);
        again.save(unused);
        QVERIFY(!SettingsStore::accountValues(*config(), QStringLiteral("work"))
                     .contains(QStringLiteral("Reading/AutoMarkRead")));
    }

    void integrationsExplainWhatIsMissing()
    {
        IntegrationsGroup group(m_context, nullptr);
        auto *calendar = child<QCheckBox>(&group, QStringLiteral("mailToCalendar"));
        auto *calendarReason = child<QLabel>(&group, QStringLiteral("mailToCalendarReason"));
        auto *notes = child<QCheckBox>(&group, QStringLiteral("mailToNotes"));
        auto *folder = child<QLineEdit>(&group, QStringLiteral("notesFolder"));
        QVERIFY(calendar && calendarReason && notes && folder);
        QVERIFY(!calendar->isEnabled());
        QVERIFY(!calendarReason->text().isEmpty());
        QVERIFY(!notes->isEnabled());

        const QString chosen = m_directory.filePath(QStringLiteral("Notes"));
        m_context.chooseFile = [&](QWidget *, const QString &, SettingsContext::FileMode mode, const QString &) {
            return mode == SettingsContext::FileMode::Directory ? chosen : QString();
        };
        child<QPushButton>(&group, QStringLiteral("chooseNotesFolder"))->click();
        QCOMPARE(folder->text(), chosen);
        QVERIFY(notes->isEnabled());
        notes->setChecked(true);
        QSettings unused;
        group.save(unused);
        QCOMPARE(SettingsStore::value(*config(), nullptr, QStringLiteral("Integrations/MailToNotes")), QVariant(true));
        QCOMPARE(SettingsStore::value(*config(), nullptr, QStringLiteral("Integrations/NotesFolder")).toString(), chosen);

        m_context.available = UiUtils::SettingsSchema::Feature::Calendar;
        IntegrationsGroup available(m_context, nullptr);
        QVERIFY(child<QCheckBox>(&available, QStringLiteral("mailToCalendar"))->isEnabled());
    }

    void searchHooksAndIndexedAccounts()
    {
        int cleared = 0;
        QWidget *managedOver = nullptr;
        m_context.clearSearchHistory = [&]() { ++cleared; };
        m_context.manageSearchIndex = [&](QWidget *parent) { managedOver = parent; };
        SearchGroup group(m_context, nullptr);
        child<QPushButton>(&group, QStringLiteral("clearSearchHistory"))->click();
        child<QPushButton>(&group, QStringLiteral("manageSearchIndex"))->click();
        QCOMPARE(cleared, 1);
        QCOMPARE(managedOver, &group);

        // Only accounts with settings are listed
        auto *accounts = child<QTableWidget>(&group, QStringLiteral("searchIndexedAccounts"));
        QCOMPARE(accounts->rowCount(), 1);
        accounts->item(0, 1)->setCheckState(Qt::Checked);
        QVERIFY(accounts->item(0, 2)->flags() & Qt::ItemIsEnabled);
        child<QComboBox>(&group, QStringLiteral("searchPlaces"))->setCurrentIndex(2);
        QSettings unused;
        group.save(unused);
        QVERIFY(m_work->value(Common::SettingsNames::searchIndexKey).toBool());
        QCOMPARE(SettingsStore::value(*config(), nullptr, QStringLiteral("Search/DefaultPlaces")).toString(),
                 QStringLiteral("all"));

        // Reset asks first, and then leaves the defaults unset
        bool asked = false;
        m_context.confirm = [&](QWidget *, const QString &, const QString &) {
            asked = true;
            return true;
        };
        child<QPushButton>(&group, QStringLiteral("resetSearch"))->click();
        QVERIFY(asked);
        group.save(unused);
        QVERIFY(!KConfigGroup(config(), QStringLiteral("Search")).hasKey("DefaultPlaces"));
    }

    void workspacesAreEditedAndSaved()
    {
        WorkspacesGroup group(m_context, nullptr);
        auto *list = child<QListWidget>(&group, QStringLiteral("workspaceList"));
        QCOMPARE(list->count(), 0);
        QVERIFY(!child<QCheckBox>(&group, QStringLiteral("followActivity"))->isEnabled());
        child<QPushButton>(&group, QStringLiteral("addWorkspace"))->click();
        QCOMPARE(list->count(), 1);
        auto *name = child<QLineEdit>(&group, QStringLiteral("workspaceName"));
        name->setText(QStringLiteral("Office"));
        emit name->textEdited(name->text());
        auto *accounts = child<QListWidget>(&group, QStringLiteral("workspaceAccounts"));
        QCOMPARE(accounts->count(), 2);
        accounts->item(0)->setCheckState(Qt::Checked);
        QCOMPARE(list->item(0)->text(), QStringLiteral("Office"));

        QSettings unused;
        group.save(unused);
        const auto saved = SettingsStore::workspaces(*config());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().name, QStringLiteral("Office"));
        QCOMPARE(saved.first().accounts, QStringList{QStringLiteral("work")});
    }

    void keymapExportImportAndUndo()
    {
        const QString exported = m_directory.filePath(QStringLiteral("exported.keymap.json"));
        QString next;
        m_context.chooseFile = [&](QWidget *, const QString &, SettingsContext::FileMode, const QString &) {
            return next;
        };
        bool confirmed = false;
        m_context.confirm = [&](QWidget *, const QString &, const QString &) {
            confirmed = true;
            return true;
        };
        KeyboardGroup group(m_context, nullptr);
        auto *vimMode = child<QCheckBox>(&group, QStringLiteral("vimMode"));
        auto *undo = child<QPushButton>(&group, QStringLiteral("undoKeymapImport"));
        QVERIFY(vimMode && undo);
        QVERIFY(!vimMode->isChecked());
        QVERIFY(!undo->isEnabled());

        next = exported;
        child<QPushButton>(&group, QStringLiteral("exportKeymap"))->click();
        QFile file(exported);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto parsed = UiUtils::Keymap::fromJson(file.readAll());
        file.close();
        QVERIFY(parsed.profile);
        QVERIFY(!parsed.profile->vimMode);

        // Importing the same keymap changes nothing and asks nothing
        child<QPushButton>(&group, QStringLiteral("importKeymap"))->click();
        QVERIFY(!confirmed);

        UiUtils::Keymap::Profile changed = *parsed.profile;
        changed.vimMode = true;
        changed.timeout = 1500;
        const QString incoming = m_directory.filePath(QStringLiteral("incoming.keymap.json"));
        QFile incomingFile(incoming);
        QVERIFY(incomingFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        incomingFile.write(UiUtils::Keymap::toJson(changed));
        incomingFile.close();
        next = incoming;
        child<QPushButton>(&group, QStringLiteral("importKeymap"))->click();
        QVERIFY(confirmed);
        QVERIFY(vimMode->isChecked());
        QCOMPARE(child<QSpinBox>(&group, QStringLiteral("vimTimeout"))->value(), 1500);
        QCOMPARE(SettingsStore::value(*config(), nullptr, QStringLiteral("Keyboard/VimMode")), QVariant(true));
        QVERIFY(undo->isEnabled());

        undo->click();
        QVERIFY(!vimMode->isChecked());
        QCOMPARE(SettingsStore::value(*config(), nullptr, QStringLiteral("Keyboard/VimMode")), QVariant(false));
    }

private:
    QTemporaryDir m_directory;
    std::unique_ptr<QSettings> m_legacy;
    std::unique_ptr<QSettings> m_work;
    SettingsContext m_context;
};

QTEST_MAIN(SettingsDialogTest)
#include "test_SettingsDialog.moc"
