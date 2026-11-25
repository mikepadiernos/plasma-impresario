/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageRowsMenu.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QActionGroup>
#include <QDebug>

#include "MsgListView.h"

namespace Gui {

namespace {
const auto layoutKey = "MessageListRows";
const auto initialsKey = "MessageListSenderInitials";
}

MessageRowsMenu::MessageRowsMenu(MsgListView *messages, KSharedConfigPtr config, QWidget *parent)
    : QMenu(parent), m_messages(messages), m_config(config)
{
    setTitle(i18nc("@title:menu", "Message Rows"));
    setObjectName(QStringLiteral("messageRowsMenu"));
    auto *group = new QActionGroup(this);
    m_multiLine = addAction(i18nc("@option:radio message list layout", "Two Lines"));
    m_multiLine->setObjectName(QStringLiteral("messageRowsMultiLine"));
    m_multiLine->setToolTip(i18nc("@info:tooltip", "Show the sender, date and indicators above the subject"));
    m_columns = addAction(i18nc("@option:radio message list layout", "Columns"));
    m_columns->setObjectName(QStringLiteral("messageRowsColumns"));
    m_columns->setToolTip(i18nc("@info:tooltip", "Show one line per message with resizable columns"));
    for (auto *action : {m_multiLine, m_columns}) {
        action->setCheckable(true);
        group->addAction(action);
    }
    addSeparator();
    m_senderInitials = addAction(i18nc("@option:check", "Show Sender Initials"));
    m_senderInitials->setObjectName(QStringLiteral("messageRowsSenderInitials"));
    m_senderInitials->setCheckable(true);

    KConfigGroup settings(m_config, QStringLiteral("Appearance"));
    const QString layout = settings.readEntry(layoutKey, QStringLiteral("multiline"));
    if (layout != QLatin1String("multiline") && layout != QLatin1String("columns"))
        qWarning() << "Unknown message list layout:" << layout;
    (layout == QLatin1String("columns") ? m_columns : m_multiLine)->setChecked(true);
    m_senderInitials->setChecked(settings.readEntry(initialsKey, true));
    apply();

    connect(group, &QActionGroup::triggered, this, [this] {
        apply();
        save();
    });
    connect(m_senderInitials, &QAction::toggled, this, [this] {
        apply();
        save();
    });
}

bool MessageRowsMenu::isMultiLine() const
{
    return m_multiLine->isChecked();
}

void MessageRowsMenu::setMultiLine(bool multiLine)
{
    if (multiLine != isMultiLine())
        (multiLine ? m_multiLine : m_columns)->trigger();
}

void MessageRowsMenu::apply()
{
    m_senderInitials->setEnabled(isMultiLine());
    m_messages->setSenderInitials(m_senderInitials->isChecked());
    m_messages->setMultiLineRows(isMultiLine());
}

void MessageRowsMenu::save()
{
    KConfigGroup settings(m_config, QStringLiteral("Appearance"));
    settings.writeEntry(layoutKey, isMultiLine() ? QStringLiteral("multiline") : QStringLiteral("columns"));
    settings.writeEntry(initialsKey, m_senderInitials->isChecked());
    if (!m_config->sync())
        qWarning() << "Unable to save the message list layout";
}

}
