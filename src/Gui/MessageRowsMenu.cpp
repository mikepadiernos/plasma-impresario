/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "MessageRowsMenu.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <QActionGroup>
#include <QDebug>

#include "AppearanceManager.h"
#include "MsgListView.h"
#include "UiUtils/SettingsStore.h"

namespace Gui {

namespace {
const auto layoutKey = "MessageListRows";
const auto initialsKey = "MessageListSenderInitials";

bool cardsInEffect()
{
    return AppearanceManager::instance()->value(QStringLiteral("MessageListStyle")).toString() == QLatin1String("Cards");
}
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
    m_cards = addAction(i18nc("@option:radio message list layout", "Cards"));
    m_cards->setObjectName(QStringLiteral("messageRowsCards"));
    m_cards->setToolTip(i18nc("@info:tooltip", "Show each message as a card with its account or folder colour"));
    m_columns->setToolTip(i18nc("@info:tooltip", "Show one line per message with resizable columns"));
    // Cards come right after two-line rows, which they extend
    removeAction(m_columns);
    addAction(m_columns);
    for (auto *action : {m_multiLine, m_cards, m_columns}) {
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
    (layout == QLatin1String("columns") ? m_columns : cardsInEffect() ? m_cards : m_multiLine)->setChecked(true);
    m_senderInitials->setChecked(settings.readEntry(initialsKey, true));
    apply();

    connect(group, &QActionGroup::triggered, this, [this](QAction *action) {
        apply();
        save(action != m_columns);
    });
    connect(m_senderInitials, &QAction::toggled, this, [this] {
        apply();
        save(false);
    });
    connect(AppearanceManager::instance(), &AppearanceManager::changed, this, &MessageRowsMenu::followAppearance);
}

bool MessageRowsMenu::isMultiLine() const
{
    return m_multiLine->isChecked() || m_cards->isChecked();
}

void MessageRowsMenu::setMultiLine(bool multiLine)
{
    if (multiLine != isMultiLine())
        (multiLine ? m_multiLine : m_columns)->trigger();
}

bool MessageRowsMenu::isCards() const
{
    return m_cards->isChecked();
}

void MessageRowsMenu::setCards(bool cards)
{
    if (cards != isCards())
        (cards ? m_cards : m_multiLine)->trigger();
}

void MessageRowsMenu::followAppearance()
{
    // The style can change with a workspace or in the Theme settings; columns stay columns
    if (m_columns->isChecked() || cardsInEffect() == m_cards->isChecked())
        return;
    (cardsInEffect() ? m_cards : m_multiLine)->setChecked(true);
    apply();
}

void MessageRowsMenu::apply()
{
    m_senderInitials->setEnabled(isMultiLine());
    m_messages->setSenderInitials(m_senderInitials->isChecked());
    m_messages->setMultiLineRows(isMultiLine());
    m_messages->setCards(isCards());
}

void MessageRowsMenu::save(bool styleChosen)
{
    KConfigGroup settings(m_config, QStringLiteral("Appearance"));
    settings.writeEntry(layoutKey, isMultiLine() ? QStringLiteral("multiline") : QStringLiteral("columns"));
    settings.writeEntry(initialsKey, m_senderInitials->isChecked());
    bool styleSaved = false;
    if (styleChosen) {
        UiUtils::Theme::Values theme = UiUtils::SettingsStore::theme(*m_config);
        const QString style = isCards() ? QStringLiteral("Cards") : QStringLiteral("Rows");
        if (UiUtils::Theme::value(theme, QStringLiteral("MessageListStyle")).toString() != style) {
            theme.insert(QStringLiteral("MessageListStyle"), style);
            UiUtils::SettingsStore::setTheme(*m_config, theme);
            styleSaved = true;
        }
    }
    if (!m_config->sync())
        qWarning() << "Unable to save the message list layout";
    if (styleSaved)
        emit appearanceChanged();
}

}
