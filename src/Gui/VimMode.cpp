/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "VimMode.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace Gui {

namespace {

bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
        return true;
    default:
        return false;
    }
}

}

VimMode::VimMode(QWidget *window, ContextResolver resolver, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_resolver(std::move(resolver))
    , m_timeout(new QTimer(this))
    , m_hintTimer(new QTimer(this))
    , m_hints(new QLabel(window, Qt::ToolTip))
{
    m_timeout->setSingleShot(true);
    m_hintTimer->setSingleShot(true);
    m_hintTimer->setInterval(400);
    connect(m_timeout, &QTimer::timeout, this, &VimMode::cancel);
    connect(m_hintTimer, &QTimer::timeout, this, &VimMode::showHints);
    m_hints->setObjectName(QStringLiteral("vimHints"));
    m_hints->setTextFormat(Qt::RichText);
    m_hints->setForegroundRole(QPalette::ToolTipText);
    m_hints->setBackgroundRole(QPalette::ToolTipBase);
    m_hints->setAutoFillBackground(true);
    m_hints->setMargin(6);
    m_hints->hide();
    qApp->installEventFilter(this);
}

VimMode::~VimMode()
{
    if (qApp)
        qApp->removeEventFilter(this);
}

void VimMode::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    cancel();
}

bool VimMode::isEnabled() const
{
    return m_enabled;
}

void VimMode::setBindings(const UiUtils::Keymap::Bindings &bindings)
{
    cancel();
    m_matcher.setBindings(bindings);
}

void VimMode::setTimeout(int milliseconds)
{
    m_timeoutMs = std::max(0, milliseconds);
}

void VimMode::setHints(bool enabled, int delay)
{
    m_hintsEnabled = enabled;
    m_hintTimer->setInterval(std::max(0, delay));
    if (!enabled)
        hideHints();
}

QString VimMode::pending() const
{
    return m_matcher.pending();
}

void VimMode::cancel()
{
    const bool hadKeys = !m_matcher.pending().isEmpty();
    m_matcher.reset();
    m_pendingContext.reset();
    m_timeout->stop();
    hideHints();
    if (hadKeys)
        emit pendingChanged(QString());
}

void VimMode::sendKey(QWidget *target, int key, Qt::KeyboardModifiers modifiers)
{
    if (!target)
        return;
    const bool wasSynthesizing = std::exchange(m_synthesizing, true);
    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QCoreApplication::sendEvent(target, &release);
    m_synthesizing = wasSynthesizing;
}

bool VimMode::takesText(const QWidget *widget)
{
    if (qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget)
        || qobject_cast<const QPlainTextEdit *>(widget) || qobject_cast<const QAbstractSpinBox *>(widget))
        return true;
    if (auto *combo = qobject_cast<const QComboBox *>(widget))
        return combo->isEditable();
    // A read-only text view is not typed into, but its keys move the cursor; that one stays for Vim mode to decide
    return false;
}

std::optional<UiUtils::Keymap::Context> VimMode::contextOf(QObject *watched) const
{
    // Key events climb from the focus widget to its parents; only the first delivery is looked at
    QWidget *focus = QApplication::focusWidget();
    if (!m_window || !focus || watched != focus || focus->window() != m_window)
        return std::nullopt;
    if (takesText(focus))
        return std::nullopt;
    return m_resolver ? m_resolver(focus) : std::nullopt;
}

bool VimMode::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_enabled || m_synthesizing)
        return false;
    const QEvent::Type type = event->type();
    if (type != QEvent::ShortcutOverride && type != QEvent::KeyPress)
        return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (isModifierKey(key->key()))
        return false;
    const auto context = contextOf(watched);
    if (!context) {
        if (type == QEvent::KeyPress && watched == QApplication::focusWidget())
            cancel();
        return false;
    }
    const QString token = UiUtils::Keymap::token(key->key(), key->modifiers(), key->text());
    const bool hadKeys = !m_matcher.pending().isEmpty();

    if (type == QEvent::ShortcutOverride) {
        // Taking the override keeps the window's shortcut, such as M for Mark as Read, from firing first
        if (token.isEmpty())
            return false;
        UiUtils::Keymap::Matcher probe = m_matcher;
        if (hadKeys || probe.feed(token, *context) != UiUtils::Keymap::Matcher::Result::None) {
            key->accept();
            return true;
        }
        return false;
    }

    if (token.isEmpty()) {
        cancel();
        return false;
    }
    if (hadKeys && m_pendingContext && *m_pendingContext != *context)
        cancel();
    using Result = UiUtils::Keymap::Matcher::Result;
    switch (m_matcher.feed(token, *context)) {
    case Result::None:
        // A key which continues no sequence ends the one typed so far, and is used up by it
        cancel();
        if (hadKeys)
            emit pendingChanged(QString());
        return hadKeys;
    case Result::Pending:
        m_pendingContext = context;
        if (m_timeoutMs > 0)
            m_timeout->start(m_timeoutMs);
        hideHints();
        if (m_hintsEnabled)
            m_hintTimer->start();
        emit pendingChanged(m_matcher.pending());
        return true;
    case Result::Matched: {
        const QString command = m_matcher.matched();
        cancel();
        if (hadKeys)
            emit pendingChanged(QString());
        emit commandTriggered(command, *context);
        return true;
    }
    case Result::Cancelled:
        cancel();
        emit pendingChanged(QString());
        return true;
    }
    return false;
}

void VimMode::showHints()
{
    if (!m_window || !m_pendingContext || m_matcher.pending().isEmpty())
        return;
    const auto continuations = m_matcher.continuations(*m_pendingContext);
    if (continuations.isEmpty())
        return;
    QString rows;
    for (const auto &next : continuations) {
        rows += QStringLiteral("<tr><td><b>%1%2</b></td><td>&nbsp;&nbsp;%3</td></tr>")
                    .arg(m_matcher.pending().toHtmlEscaped(), next.keys.toHtmlEscaped(),
                         UiUtils::Keymap::description(next.command).toHtmlEscaped());
    }
    m_hints->setText(QStringLiteral("<table>%1</table>").arg(rows));
    m_hints->adjustSize();
    const QPoint margin(12, 12);
    QPoint position = m_window->mapToGlobal(QPoint(margin.x(), m_window->height() - m_hints->height() - margin.y()));
    if (const QScreen *screen = m_window->screen()) {
        const QRect area = screen->availableGeometry();
        position.setX(std::clamp(position.x(), area.left(), std::max(area.left(), area.right() - m_hints->width())));
        position.setY(std::clamp(position.y(), area.top(), std::max(area.top(), area.bottom() - m_hints->height())));
    }
    m_hints->move(position);
    m_hints->show();
}

void VimMode::hideHints()
{
    m_hintTimer->stop();
    m_hints->hide();
}

}
