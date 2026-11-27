/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>
#include <QList>
#include <functional>

class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QToolButton;

namespace Gui {

/** @short Non-modal, inline message strip with optional action buttons

Used for feedback after operations which change messages on the server, such as "Moved 3 messages to Trash" with an
Undo button, or for reporting an error message from the server. Information disappears on its own after
autoHideTimeout() unless the pointer or the keyboard focus is inside the bar; errors and warnings stay until closed.
While a longer operation runs, showBusy() shows its description with an indeterminate progress indicator until
the result replaces it.
*/
class InlineNotificationBar : public QFrame
{
    Q_OBJECT
public:
    enum class Kind {
        Info,
        Error,
        /** Something the user should know before going on, such as a message marked as junk; stays until closed */
        Warning,
        /** An operation is still running; replaced by its result */
        Busy,
    };

    struct Action {
        QString text;
        QString iconName;
        QString objectName;
        std::function<void()> callback;
    };

    explicit InlineNotificationBar(QWidget *parent = nullptr);

    /** @short Show @arg text, replacing whatever was shown before */
    void showMessage(Kind kind, const QString &text, const QList<Action> &actions = {});
    void showInfo(const QString &text, const QList<Action> &actions = {});
    void showError(const QString &text, const QList<Action> &actions = {});
    void showWarning(const QString &text, const QList<Action> &actions = {});
    /** @short Show @arg text with a progress indicator and without a timeout; returns an id for dismissBusy() */
    quint64 showBusy(const QString &text);
    /** @short Hide the bar if it still shows the busy message @arg id rather than something newer */
    void dismissBusy(quint64 id);
    bool isBusy() const;

    Kind kind() const;
    QString text() const;
    QList<QPushButton *> actionButtons() const;
    QToolButton *closeButton() const;
    QProgressBar *progressBar() const;

    /** @short Delay in milliseconds before information is hidden; zero or less keeps it visible */
    void setAutoHideTimeout(int msec);
    int autoHideTimeout() const;
    bool isAutoHideActive() const;

public slots:
    /** @short Hide the bar and emit dismissed() */
    void dismiss();

signals:
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    void slotAutoHideTimeout();
    bool isInUse() const;

    QLabel *m_icon;
    QLabel *m_text;
    QProgressBar *m_progress;
    QHBoxLayout *m_buttonLayout;
    QToolButton *m_close;
    QTimer *m_autoHide;
    QList<QPushButton *> m_buttons;
    Kind m_kind = Kind::Info;
    int m_timeout = 8000;
    quint64 m_serial = 0;
};

}
