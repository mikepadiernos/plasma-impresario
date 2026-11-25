/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "DensityMenu.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QActionGroup>
#include <QDebug>
#include <QTreeView>

#include "DensityDelegate.h"

namespace Gui {

DensityMenu::DensityMenu(QTreeView *mailboxes, QTreeView *messages, KSharedConfigPtr config, QWidget *parent)
    : QMenu(parent), m_mailboxes(mailboxes), m_messages(messages), m_config(config)
{
    setTitle(i18nc("@title:menu", "Row Density"));
    auto *group = new QActionGroup(this);
    auto *compact = addAction(i18nc("@option:radio", "Compact"));
    auto *comfortable = addAction(i18nc("@option:radio", "Comfortable"));
    for (auto *action : {compact, comfortable}) {
        action->setCheckable(true);
        group->addAction(action);
    }

    KConfigGroup settings(m_config, QStringLiteral("Appearance"));
    const QString density = settings.readEntry("RowDensity", QStringLiteral("compact"));
    if (density != QLatin1String("compact") && density != QLatin1String("comfortable"))
        qWarning() << "Unknown row density:" << density;
    const bool isComfortable = density == QLatin1String("comfortable");
    (isComfortable ? comfortable : compact)->setChecked(true);
    applyDensity(isComfortable);

    connect(group, &QActionGroup::triggered, this, [this, comfortable](QAction *action) {
        const bool isComfortable = action == comfortable;
        applyDensity(isComfortable);
        KConfigGroup settings(m_config, QStringLiteral("Appearance"));
        settings.writeEntry("RowDensity", isComfortable ? QStringLiteral("comfortable") : QStringLiteral("compact"));
        if (!m_config->sync())
            qWarning() << "Unable to save row density preference";
    });
}

void DensityMenu::applyDensity(bool comfortable)
{
    for (auto *view : {m_mailboxes, m_messages}) {
        auto *delegate = qobject_cast<DensityDelegate *>(view->itemDelegate());
        Q_ASSERT(delegate);
        delegate->setComfortable(comfortable);
        view->doItemsLayout();
    }
}

}
