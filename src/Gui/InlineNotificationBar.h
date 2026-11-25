/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>
#include <QList>
#include <functional>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QTimer;
class QToolButton;

namespace Gui {

/** @short Non-modal, inline message strip with optional action buttons

Used for feedback after operations which change messages on the server, such as "Moved 3 messages to Trash" with an
Undo button, or for reporting an error message from the server. Information disappears on its own after
autoHideTimeout() unless the pointer or the keyboard focus is inside the bar; errors stay until closed.
*/
class InlineNotificationBar : public QFrame
{
    Q_OBJECT
public:
    enum class Kind {
        Info,
        Error,
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

    Kind kind() const;
    QString text() const;
    QList<QPushButton *> actionButtons() const;
    QToolButton *closeButton() const;

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
    QHBoxLayout *m_buttonLayout;
    QToolButton *m_close;
    QTimer *m_autoHide;
    QList<QPushButton *> m_buttons;
    Kind m_kind = Kind::Info;
    int m_timeout = 8000;
};

}
