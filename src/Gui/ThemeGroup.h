/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "Gui/SettingsGroups.h"
#include "UiUtils/FolderColors.h"
#include "UiUtils/Theme.h"
#include "UiUtils/ThemeProfiles.h"

class KColorButton;
class KFontRequester;
class QFormLayout;
class QPlainTextEdit;

namespace Gui {

/** @short The appearance of Plasma Impresario: colours, effects, lines, lists,
text, profiles and the stylesheet

Every option follows Plasma until it is changed here, and nothing here changes
the desktop. Changes can be previewed in the window before they are saved;
closing the dialog without saving goes back to the saved appearance.
*/
class ThemeGroup : public SettingsGroupPage {
    Q_OBJECT
public:
    ThemeGroup(const SettingsContext &context, QWidget *parent);
    ~ThemeGroup() override;

    /** @short The options as the page shows them, validated */
    UiUtils::Theme::Values values() const;

protected:
    void saveSettings() override;

private:
    QWidget *colorsPage();
    QWidget *effectsPage();
    QWidget *listsPage();
    QWidget *textPage();
    QWidget *profilesPage();
    QWidget *styleSheetPage();

    QComboBox *addChoice(QFormLayout *form, const QString &entry, const QString &label);
    QCheckBox *addFlag(QFormLayout *form, const QString &entry, const QString &text);
    QSpinBox *addNumber(QFormLayout *form, const QString &entry, const QString &label, const QString &suffix);
    KColorButton *addColor(QFormLayout *form, const QString &entry, const QString &label);
    void addFont(QFormLayout *form, const QString &entry, const QString &label);
    void watch(QWidget *editor);

    void showValues();
    void readWidgets();
    void onChanged();
    void updateDependent();
    void updateContrast();
    void preview();
    void resetToPlasma();

    void fillProfiles();
    void saveProfile();
    void useProfile();
    void deleteProfile();
    void importProfile();
    void exportProfile();
    void followTheme();
    void addProfile(UiUtils::ThemeProfiles::Profile profile);
    const UiUtils::ThemeProfiles::Profile *selectedProfile() const;

    void fillRules();
    void addRule();
    void removeRule();

    void checkStyleSheet();
    void importStyleSheet();
    void exportStyleSheet();

    UiUtils::Theme::Values m_values;
    QList<UiUtils::ThemeProfiles::Profile> m_profiles;
    QList<UiUtils::FolderColors::Rule> m_rules;
    QString m_followed;
    QHash<QString, QWidget *> m_editors;
    QHash<QString, QCheckBox *> m_fontCustom;
    QHash<QString, std::pair<QCheckBox *, KColorButton *>> m_roles;
    QHash<QString, std::pair<QCheckBox *, KColorButton *>> m_accountColors;

    QComboBox *m_accentSource = nullptr;
    KColorButton *m_accent = nullptr;
    QLabel *m_contrast = nullptr;
    QComboBox *m_spacingPreset = nullptr;
    QCheckBox *m_livePreview = nullptr;
    QLabel *m_problems = nullptr;

    QListWidget *m_profileList = nullptr;
    QLineEdit *m_profileName = nullptr;
    QCheckBox *m_exportForOpus = nullptr;
    QCheckBox *m_exportStyleSheet = nullptr;
    QLineEdit *m_followedFile = nullptr;
    QLabel *m_profileStatus = nullptr;

    QListWidget *m_ruleList = nullptr;
    QComboBox *m_ruleAccount = nullptr;
    QComboBox *m_ruleMatch = nullptr;
    QLineEdit *m_rulePattern = nullptr;
    KColorButton *m_ruleColor = nullptr;
    QCheckBox *m_ruleSubfolders = nullptr;

    QLabel *m_styleSheetStatus = nullptr;

    bool m_loading = false;
    bool m_previewed = false;
    bool m_saved = false;
};

} // namespace Gui
