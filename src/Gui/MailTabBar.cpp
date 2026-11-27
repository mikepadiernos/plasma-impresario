/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MailTabBar.h"

#include <KLocalizedString>
#include <utility>

#include "AppearanceManager.h"
#include "UiUtils/Color.h"

namespace Gui {

MailTabBar::MailTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setObjectName(QStringLiteral("mailTabBar"));
    setAccessibleName(i18nc("@info:whatsthis", "Mail tabs"));
    setDocumentMode(true);
    setMovable(true);
    setExpanding(false);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    connect(this, &QTabBar::currentChanged, this, [this](int index) {
        if (!m_updating && index >= 0)
            emit tabActivated(index);
    });
    connect(AppearanceManager::instance(), &AppearanceManager::changed, this, &MailTabBar::applyColors);
}

void MailTabBar::setEntries(const QList<Entry> &entries, int current)
{
    const bool wasUpdating = std::exchange(m_updating, true);
    while (count() > entries.size())
        removeTab(count() - 1);
    while (count() < entries.size())
        addTab(QString());
    for (int i = 0; i < entries.size(); ++i) {
        // Ampersands in subjects and folder names are text, not mnemonics
        QString label = entries.at(i).label;
        setTabText(i, label.replace(QLatin1Char('&'), QLatin1String("&&")));
        setTabToolTip(i, entries.at(i).toolTip);
    }
    m_colors.clear();
    for (const Entry &entry : entries)
        m_colors.append(entry.color);
    applyColors();
    setTabsClosable(entries.size() > 1);
    if (current >= 0 && current < count())
        setCurrentIndex(current);
    m_updating = wasUpdating;
}

void MailTabBar::setPlacement(UiUtils::MailTabs::Region region, Qt::Orientation orientation)
{
    m_region = region;
    m_orientation = orientation;
    setShape(shapeFor(region, orientation));
}

QColor MailTabBar::tabColor(int index) const
{
    return m_colors.value(index);
}

void MailTabBar::applyColors()
{
    const QString strength
        = AppearanceManager::instance()->value(QStringLiteral("TabColorStrength")).toString();
    const QColor window = palette().color(QPalette::Window);
    for (int i = 0; i < count(); ++i) {
        const QColor color = m_colors.value(i);
        QColor text;
        if (color.isValid() && strength == QLatin1String("Text"))
            text = UiUtils::ensureContrast(color, window);
        else if (color.isValid() && strength == QLatin1String("Strong"))
            // The style fills the tab with the colour, so the text has to stand out against it
            text = UiUtils::readableTextColor(color);
        // An invalid colour gives the tab the text colour of the palette back
        setTabTextColor(i, text);
    }
}

UiUtils::MailTabs::Region MailTabBar::region() const
{
    return m_region;
}

Qt::Orientation MailTabBar::orientation() const
{
    return m_orientation;
}

QTabBar::Shape MailTabBar::shapeFor(UiUtils::MailTabs::Region region, Qt::Orientation orientation)
{
    using UiUtils::MailTabs::Region;
    if (orientation == Qt::Horizontal)
        return region == Region::Footer ? QTabBar::RoundedSouth : QTabBar::RoundedNorth;
    return region == Region::Right ? QTabBar::RoundedEast : QTabBar::RoundedWest;
}

}
