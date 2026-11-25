/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "InlineNotificationBar.h"

#include <QAccessible>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include <KLocalizedString>

namespace {

QIcon themedIcon(const QString &name)
{
    return QIcon::fromTheme(name + QLatin1String("-symbolic"), QIcon::fromTheme(name));
}

QColor accentColor(const QPalette &palette, Gui::InlineNotificationBar::Kind kind)
{
    if (kind == Gui::InlineNotificationBar::Kind::Error) {
        // Breeze "negative" colour; the palette has no dedicated error role
        return QColor(218, 68, 83);
    }
    return palette.color(QPalette::Highlight);
}

QColor mixed(const QColor &base, const QColor &accent, qreal amount)
{
    return QColor::fromRgbF(base.redF() * (1 - amount) + accent.redF() * amount,
                            base.greenF() * (1 - amount) + accent.greenF() * amount,
                            base.blueF() * (1 - amount) + accent.blueF() * amount);
}

}

namespace Gui {

InlineNotificationBar::InlineNotificationBar(QWidget *parent)
    : QFrame(parent)
    , m_icon(new QLabel(this))
    , m_text(new QLabel(this))
    , m_progress(new QProgressBar(this))
    , m_buttonLayout(new QHBoxLayout)
    , m_close(new QToolButton(this))
    , m_autoHide(new QTimer(this))
{
    setObjectName(QStringLiteral("inlineNotificationBar"));
    setAccessibleName(i18nc("@info:accessible", "Notification"));
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    m_icon->setFixedSize(iconSize, iconSize);

    m_text->setObjectName(QStringLiteral("inlineNotificationText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_progress->setObjectName(QStringLiteral("inlineNotificationProgress"));
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumWidth(iconSize * 8);
    m_progress->setMaximumHeight(iconSize);
    m_progress->setAccessibleName(i18nc("@info:accessible", "Operation in progress"));
    m_progress->hide();

    m_close->setObjectName(QStringLiteral("inlineNotificationClose"));
    m_close->setAutoRaise(true);
    m_close->setIcon(themedIcon(QStringLiteral("window-close")));
    m_close->setToolTip(i18nc("@info:tooltip", "Close"));
    m_close->setAccessibleName(i18nc("@action:button", "Close notification"));
    connect(m_close, &QToolButton::clicked, this, &InlineNotificationBar::dismiss);

    m_buttonLayout->setContentsMargins(0, 0, 0, 0);

    auto *layout = new QHBoxLayout(this);
    const int margin = style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this) / 2 + 2;
    layout->setContentsMargins(margin * 2, margin, margin, margin);
    layout->addWidget(m_icon, 0, Qt::AlignTop);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_progress, 0, Qt::AlignVCenter);
    layout->addLayout(m_buttonLayout);
    layout->addWidget(m_close, 0, Qt::AlignTop);

    m_autoHide->setSingleShot(true);
    connect(m_autoHide, &QTimer::timeout, this, &InlineNotificationBar::slotAutoHideTimeout);

    hide();
}

void InlineNotificationBar::showMessage(Kind kind, const QString &text, const QList<Action> &actions)
{
    ++m_serial;
    m_kind = kind;
    m_progress->setVisible(kind == Kind::Busy);
    m_text->setText(text);
    m_text->setAccessibleName(text);
    setAccessibleDescription(text);

    const QString iconName = kind == Kind::Error ? QStringLiteral("dialog-error")
                                                 : QStringLiteral("dialog-information");
    m_icon->setPixmap(themedIcon(iconName).pixmap(m_icon->size()));

    // A button's callback may show the next message, so never delete a button from within its own signal
    for (QPushButton *button : std::as_const(m_buttons)) {
        button->hide();
        button->deleteLater();
    }
    m_buttons.clear();
    for (const Action &action : actions) {
        auto *button = new QPushButton(action.text, this);
        if (!action.objectName.isEmpty())
            button->setObjectName(action.objectName);
        if (!action.iconName.isEmpty())
            button->setIcon(themedIcon(action.iconName));
        button->setAccessibleName(QString(action.text).remove(QLatin1Char('&')));
        const auto callback = action.callback;
        connect(button, &QPushButton::clicked, this, [this, callback]() {
            // Hide first: the callback may well replace this message with a new one
            dismiss();
            if (callback)
                callback();
        });
        m_buttonLayout->addWidget(button);
        m_buttons << button;
    }

    show();
    update();

    m_autoHide->stop();
    if (kind == Kind::Info && m_timeout > 0)
        m_autoHide->start(m_timeout);

    QAccessibleEvent alert(this, QAccessible::Alert);
    QAccessible::updateAccessibility(&alert);
}

void InlineNotificationBar::showInfo(const QString &text, const QList<Action> &actions)
{
    showMessage(Kind::Info, text, actions);
}

void InlineNotificationBar::showError(const QString &text, const QList<Action> &actions)
{
    showMessage(Kind::Error, text, actions);
}

quint64 InlineNotificationBar::showBusy(const QString &text)
{
    showMessage(Kind::Busy, text);
    return m_serial;
}

void InlineNotificationBar::dismissBusy(const quint64 id)
{
    if (isBusy() && m_serial == id)
        dismiss();
}

bool InlineNotificationBar::isBusy() const
{
    return m_kind == Kind::Busy && !isHidden();
}

InlineNotificationBar::Kind InlineNotificationBar::kind() const
{
    return m_kind;
}

QString InlineNotificationBar::text() const
{
    return m_text->text();
}

QList<QPushButton *> InlineNotificationBar::actionButtons() const
{
    return m_buttons;
}

QToolButton *InlineNotificationBar::closeButton() const
{
    return m_close;
}

QProgressBar *InlineNotificationBar::progressBar() const
{
    return m_progress;
}

void InlineNotificationBar::setAutoHideTimeout(int msec)
{
    m_timeout = msec;
    if (m_timeout <= 0)
        m_autoHide->stop();
}

int InlineNotificationBar::autoHideTimeout() const
{
    return m_timeout;
}

bool InlineNotificationBar::isAutoHideActive() const
{
    return m_autoHide->isActive();
}

void InlineNotificationBar::dismiss()
{
    m_autoHide->stop();
    const bool wasVisible = !isHidden();
    hide();
    if (wasVisible)
        emit dismissed();
}

bool InlineNotificationBar::isInUse() const
{
    if (underMouse())
        return true;
    const QWidget *focus = QApplication::focusWidget();
    return focus && isAncestorOf(focus);
}

void InlineNotificationBar::slotAutoHideTimeout()
{
    if (isInUse()) {
        // Don't pull the Undo button away from under the user's pointer or keyboard focus
        m_autoHide->start(m_timeout);
        return;
    }
    dismiss();
}

bool InlineNotificationBar::event(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        update();
    return QFrame::event(event);
}

void InlineNotificationBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor accent = accentColor(palette(), m_kind);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(frame, 4, 4);
    painter.fillPath(path, mixed(palette().color(QPalette::Window), accent, 0.2));
    painter.setPen(QPen(accent, 1));
    painter.drawPath(path);
}

}
