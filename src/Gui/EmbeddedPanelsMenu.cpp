/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "EmbeddedPanelsMenu.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QActionGroup>
#include <QDebug>

namespace Gui {

namespace {

QString placementActionSuffix(EmbeddedPanel::Placement placement)
{
    switch (placement) {
    case EmbeddedPanel::Placement::BelowReader:
        return QStringLiteral("BelowReader");
    case EmbeddedPanel::Placement::LeftColumn:
        return QStringLiteral("LeftColumn");
    case EmbeddedPanel::Placement::RightColumn:
        return QStringLiteral("RightColumn");
    case EmbeddedPanel::Placement::Footer:
        return QStringLiteral("Footer");
    }
    return QString();
}

QString boolValue(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

}

EmbeddedPanelsMenu::EmbeddedPanelsMenu(KSharedConfigPtr config, QWidget *parent)
    : QMenu(parent)
    , m_config(config)
{
    setObjectName(QStringLiteral("embeddedPanelsMenu"));
    setTitle(i18nc("@title:menu", "Embedded Panels"));
}

void EmbeddedPanelsMenu::addPanel(EmbeddedPanel *panel)
{
    const auto &descriptor = panel->descriptor();
    const QString actionName = QLatin1String("embeddedPanel") + descriptor.id;
    const QString key = descriptor.configKey;

    auto *submenu = addMenu(descriptor.title);
    submenu->setObjectName(actionName + QLatin1String("Menu"));
    submenu->setToolTipsVisible(true);

    Entry entry{panel, nullptr, nullptr, {}};
    entry.visible = submenu->addAction(i18nc("@option:check embedded panel", "Show Panel"));
    entry.visible->setObjectName(actionName);
    entry.visible->setCheckable(true);
    entry.autoHide = submenu->addAction(i18nc("@option:check embedded panel", "Hide When Empty"));
    entry.autoHide->setObjectName(actionName + QLatin1String("AutoHide"));
    entry.autoHide->setCheckable(true);
    entry.autoHide->setToolTip(descriptor.autoHideToolTip);

    submenu->addSection(i18nc("@title:menu embedded panel position", "Position"));
    auto *group = new QActionGroup(submenu);
    group->setExclusive(true);
    for (const auto placement : EmbeddedPanel::placements()) {
        auto *action = submenu->addAction(EmbeddedPanel::placementTitle(placement));
        action->setObjectName(actionName + QLatin1String("Placement") + placementActionSuffix(placement));
        action->setCheckable(true);
        action->setData(QVariant::fromValue(placement));
        group->addAction(action);
        entry.placements.append(action);
    }

    const KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    const bool visible = appearance.readEntry(key + QLatin1String("Visible"), descriptor.defaultEnabled);
    const bool autoHide = appearance.readEntry(key + QLatin1String("AutoHide"), false);
    const auto placement = EmbeddedPanel::placementFromId(
        appearance.readEntry(key + QLatin1String("Placement"), EmbeddedPanel::placementId(descriptor.defaultPlacement)),
        descriptor.defaultPlacement);
    entry.visible->setChecked(visible);
    entry.autoHide->setChecked(autoHide);
    entry.autoHide->setEnabled(visible);
    entry.placements.at(EmbeddedPanel::placements().indexOf(placement))->setChecked(true);
    panel->setAutoHide(autoHide);
    panel->setPlacement(placement);
    panel->setPanelEnabled(visible);

    connect(entry.visible, &QAction::toggled, this, [this, panel, key, autoHideAction = entry.autoHide](bool checked) {
        panel->setPanelEnabled(checked);
        autoHideAction->setEnabled(checked);
        save(key + QLatin1String("Visible"), boolValue(checked));
    });
    connect(entry.autoHide, &QAction::toggled, this, [this, panel, key](bool checked) {
        panel->setAutoHide(checked);
        save(key + QLatin1String("AutoHide"), boolValue(checked));
    });
    connect(group, &QActionGroup::triggered, this, [this, panel, key](QAction *action) {
        const auto placement = action->data().value<EmbeddedPanel::Placement>();
        panel->setPlacement(placement);
        save(key + QLatin1String("Placement"), EmbeddedPanel::placementId(placement));
    });
    m_entries.append(entry);
}

const EmbeddedPanelsMenu::Entry *EmbeddedPanelsMenu::entry(const EmbeddedPanel *panel) const
{
    for (const auto &entry : m_entries) {
        if (entry.panel == panel)
            return &entry;
    }
    return nullptr;
}

QAction *EmbeddedPanelsMenu::visibleAction(const EmbeddedPanel *panel) const
{
    const auto *found = entry(panel);
    return found ? found->visible : nullptr;
}

QAction *EmbeddedPanelsMenu::autoHideAction(const EmbeddedPanel *panel) const
{
    const auto *found = entry(panel);
    return found ? found->autoHide : nullptr;
}

QAction *EmbeddedPanelsMenu::placementAction(const EmbeddedPanel *panel, EmbeddedPanel::Placement placement) const
{
    const auto *found = entry(panel);
    return found ? found->placements.value(EmbeddedPanel::placements().indexOf(placement)) : nullptr;
}

void EmbeddedPanelsMenu::save(const QString &key, const QString &value)
{
    KConfigGroup appearance(m_config, QStringLiteral("Appearance"));
    appearance.writeEntry(key, value);
    if (!m_config->sync())
        qWarning() << "Unable to save embedded panel preference" << key;
}

}
