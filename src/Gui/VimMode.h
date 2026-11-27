/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QPointer>
#include <functional>
#include <optional>

#include "UiUtils/Keymap.h"

class QLabel;
class QTimer;
class QWidget;

namespace Gui {

/** @short Optional Vim-style keys for the folder tree, the message list and the reader

The filter watches the keys typed into @arg window. Keys which start or complete a command are taken before the
window's own shortcuts see them; every other key, and every key typed into a text field, goes on as usual. While a
sequence such as "g" is incomplete, a panel lists what may follow it after the hint delay, and the sequence is
dropped after the timeout or with Escape.

The window carries out the commands; see commandTriggered().
*/
class VimMode : public QObject
{
    Q_OBJECT
public:
    /** @short The part of the window @arg focus is in, or nothing when Vim mode leaves its keys alone */
    using ContextResolver = std::function<std::optional<UiUtils::Keymap::Context>(QWidget *focus)>;

    VimMode(QWidget *window, ContextResolver resolver, QObject *parent = nullptr);
    ~VimMode() override;

    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setBindings(const UiUtils::Keymap::Bindings &bindings);
    /** @short How long an incomplete sequence waits for its next key; 0 waits until Escape */
    void setTimeout(int milliseconds);
    void setHints(bool enabled, int delay);

    /** @short The keys typed so far of an incomplete sequence */
    QString pending() const;
    /** @short Drop the keys typed so far */
    void cancel();

    /** @short Send a key press and release to @arg target which Vim mode does not handle itself */
    void sendKey(QWidget *target, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    /** @short Whether keys typed into @arg widget are text, such as in a line edit or an editable combo box */
    static bool takesText(const QWidget *widget);

signals:
    void commandTriggered(const QString &command, UiUtils::Keymap::Context context);
    void pendingChanged(const QString &keys);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::optional<UiUtils::Keymap::Context> contextOf(QObject *watched) const;
    void showHints();
    void hideHints();

    QPointer<QWidget> m_window;
    ContextResolver m_resolver;
    UiUtils::Keymap::Matcher m_matcher;
    std::optional<UiUtils::Keymap::Context> m_pendingContext;
    QTimer *m_timeout;
    QTimer *m_hintTimer;
    QLabel *m_hints;
    bool m_enabled = false;
    bool m_hintsEnabled = true;
    int m_timeoutMs = 1000;
    bool m_synthesizing = false;
};

}
