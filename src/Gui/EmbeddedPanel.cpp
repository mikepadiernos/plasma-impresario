/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "EmbeddedPanel.h"

#include "UiUtils/IconLoader.h"

#include <algorithm>

#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "Imap/Model/ItemRoles.h"

namespace Gui {

QList<EmbeddedPanel::Placement> EmbeddedPanel::placements()
{
    return {Placement::BelowReader, Placement::LeftColumn, Placement::RightColumn, Placement::Footer};
}

QString EmbeddedPanel::placementId(Placement placement)
{
    switch (placement) {
    case Placement::BelowReader:
        return QStringLiteral("below-reader");
    case Placement::LeftColumn:
        return QStringLiteral("left");
    case Placement::RightColumn:
        return QStringLiteral("right");
    case Placement::Footer:
        return QStringLiteral("footer");
    }
    return QString();
}

EmbeddedPanel::Placement EmbeddedPanel::placementFromId(const QString &id, Placement fallback)
{
    for (const auto placement : placements()) {
        if (placementId(placement) == id)
            return placement;
    }
    return fallback;
}

QString EmbeddedPanel::placementTitle(Placement placement)
{
    switch (placement) {
    case Placement::BelowReader:
        return i18nc("@option:radio embedded panel position", "Below Reader");
    case Placement::LeftColumn:
        return i18nc("@option:radio embedded panel position", "Left Column");
    case Placement::RightColumn:
        return i18nc("@option:radio embedded panel position", "Right Column");
    case Placement::Footer:
        return i18nc("@option:radio embedded panel position", "Footer");
    }
    return QString();
}

QIcon EmbeddedPanel::themedIcon(const QString &name)
{
    return UiUtils::SymbolicIcon::fromTheme(name);
}

EmbeddedPanel::EmbeddedPanel(const Descriptor &descriptor, const QString &objectName, QWidget *parent)
    : QFrame(parent)
    , m_title(new QLabel(this))
    , m_stack(new QStackedWidget(this))
    , m_placeholder(new QLabel(m_stack))
    , m_header(new QHBoxLayout)
    , m_descriptor(descriptor)
    , m_enabled(descriptor.defaultEnabled)
    , m_placement(descriptor.defaultPlacement)
{
    setObjectName(objectName);
    setAccessibleName(descriptor.title);
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

    m_title->setObjectName(objectName + QLatin1String("Title"));
    m_title->setText(descriptor.title);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);

    m_placeholder->setObjectName(objectName + QLatin1String("Placeholder"));
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setEnabled(false);
    m_placeholder->setWordWrap(true);
    m_stack->addWidget(m_placeholder);
    m_stack->setMinimumHeight(fontMetrics().height() * 2);

    m_header->setContentsMargins(0, 0, 0, 0);
    m_header->addWidget(m_title);
    m_header->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(m_header);
    layout->addWidget(m_stack, 1);
}

const EmbeddedPanel::Descriptor &EmbeddedPanel::descriptor() const
{
    return m_descriptor;
}

bool EmbeddedPanel::isPanelEnabled() const
{
    return m_enabled;
}

bool EmbeddedPanel::autoHide() const
{
    return m_autoHide;
}

bool EmbeddedPanel::shouldBeShown() const
{
    return m_enabled && (!m_autoHide || hasContent());
}

EmbeddedPanel::Placement EmbeddedPanel::placement() const
{
    return m_placement;
}

void EmbeddedPanel::setPanelEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    panelEnabledChanged();
    updateVisibility();
}

void EmbeddedPanel::setAutoHide(bool autoHide)
{
    if (m_autoHide == autoHide)
        return;
    m_autoHide = autoHide;
    updateVisibility();
}

void EmbeddedPanel::setPlacement(Placement placement)
{
    if (m_placement == placement)
        return;
    m_placement = placement;
    emit placementChanged(placement);
}

void EmbeddedPanel::setContentWidget(QWidget *content)
{
    Q_ASSERT(!m_content);
    m_content = content;
    m_stack->addWidget(content);
    m_title->setBuddy(content);
}

QToolButton *EmbeddedPanel::addHeaderButton(const QString &objectNameSuffix, const QString &iconName, const QString &text)
{
    auto *button = new QToolButton(this);
    button->setObjectName(objectName() + objectNameSuffix);
    button->setIcon(themedIcon(iconName));
    button->setText(text);
    button->setToolTip(text);
    button->setAccessibleName(text);
    button->setAutoRaise(true);
    button->setIconSize(QSize(16, 16));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_header->addWidget(button);
    return button;
}

void EmbeddedPanel::setTitle(const QString &title)
{
    m_title->setText(title);
}

void EmbeddedPanel::showPlaceholder(const QString &text)
{
    m_placeholder->setText(text);
    m_stack->setCurrentWidget(m_placeholder);
}

void EmbeddedPanel::showContent()
{
    if (m_content)
        m_stack->setCurrentWidget(m_content);
}

void EmbeddedPanel::updateVisibility()
{
    // A never-shown child reports isHidden() without being explicitly hidden, so track what was applied.
    const bool show = shouldBeShown();
    if (m_appliedVisibility == int(show))
        return;
    m_appliedVisibility = int(show);
    // A panel without a parent would become a window of its own; the shell shows it once it is placed.
    if (parentWidget() || !show)
        setVisible(show);
    emit relevanceChanged(show);
}

int EmbeddedPanel::chromeHeight() const
{
    const QMargins margins = contentsMargins() + layout()->contentsMargins();
    return margins.top() + margins.bottom() + m_header->sizeHint().height() + layout()->spacing();
}

void EmbeddedPanel::setPreferredRows(int rows)
{
    m_preferredRows = std::max(1, rows);
    updateGeometry();
}

QSize EmbeddedPanel::sizeHint() const
{
    QSize hint = QFrame::sizeHint();
    // A list row holds a small icon and some padding; a text line is a little shorter.
    const int iconHeight = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    const int rowHeight = std::max(fontMetrics().height(), iconHeight) + fontMetrics().height() / 2;
    const int contentFrame = 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this);
    hint.setHeight(chromeHeight() + m_preferredRows * rowHeight + contentFrame);
    return hint;
}

MessageContextPanel::MessageContextPanel(const Descriptor &descriptor, const QString &objectName, QWidget *parent)
    : EmbeddedPanel(descriptor, objectName, parent)
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { refresh(); });
}

QModelIndex MessageContextPanel::message() const
{
    return m_message;
}

void MessageContextPanel::setMessage(const QModelIndex &message)
{
    if (m_message == message)
        return;
    for (const auto &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    m_message = message;
    if (const auto *model = message.model()) {
        // The envelope, flags and body structure arrive lazily and are announced through these signals.
        m_connections << connect(model, &QAbstractItemModel::dataChanged, this,
                                 [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                                     if (!m_message.isValid())
                                         return;
                                     if (topLeft.parent() == m_message.parent() && topLeft.row() <= m_message.row()
                                         && bottomRight.row() >= m_message.row())
                                         scheduleRefresh();
                                     else if (topLeft.parent() == m_message)
                                         scheduleRefresh();
                                 })
                      << connect(model, &QAbstractItemModel::rowsInserted, this,
                                 [this](const QModelIndex &parent) {
                                     if (parent == m_message)
                                         scheduleRefresh();
                                 })
                      << connect(model, &QAbstractItemModel::modelReset, this, &MessageContextPanel::scheduleRefresh)
                      << connect(model, &QAbstractItemModel::layoutChanged, this, &MessageContextPanel::scheduleRefresh)
                      << connect(model, &QAbstractItemModel::rowsRemoved, this, &MessageContextPanel::scheduleRefresh)
                      << connect(model, &QObject::destroyed, this, [this]() {
                             for (const auto &connection : std::as_const(m_connections))
                                 disconnect(connection);
                             m_connections.clear();
                             m_message = QModelIndex();
                             scheduleRefresh();
                         });
    }
    m_refreshTimer->stop();
    refresh();
}

void MessageContextPanel::scheduleRefresh()
{
    m_refreshTimer->start();
}

bool MessageContextPanel::messageIsFetched() const
{
    return m_message.isValid() && m_message.data(Imap::Mailbox::RoleIsFetched).toBool();
}

}
