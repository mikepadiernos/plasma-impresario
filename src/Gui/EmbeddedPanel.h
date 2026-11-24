/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>
#include <QIcon>
#include <QList>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QPointer>

class QAbstractItemModel;
class QHBoxLayout;
class QLabel;
class QStackedWidget;
class QTimer;
class QToolButton;

namespace Gui {

/** @short Base for the context-aware panels embedded in the mail shell

A panel has a bold title row with optional buttons, and either a placeholder text
or its content below. Whether it is shown depends on two user preferences: the panel
can be turned off, and it can hide itself while it has nothing to show. Each panel
also remembers where the shell should place it.
*/
class EmbeddedPanel : public QFrame
{
    Q_OBJECT
public:
    /** @short Where the shell places a panel */
    enum class Placement {
        BelowReader, /**< In the reader column, under the message */
        LeftColumn, /**< In the shell's left column */
        RightColumn, /**< In the shell's right column */
        Footer, /**< In the footer above the status bar */
    };
    Q_ENUM(Placement)

    /** @short Static description of a panel, used for its menu entries and preferences */
    struct Descriptor {
        /** Suffix of the menu action object names, e.g. "Attachments" gives embeddedPanelAttachments */
        QString id;
        /** Prefix of the preferences in the Appearance group, e.g. "AttachmentPanel" gives AttachmentPanelVisible */
        QString configKey;
        QString title;
        QString autoHideToolTip;
        bool defaultEnabled = false;
        Placement defaultPlacement = Placement::RightColumn;
    };

    static QList<Placement> placements();
    static QString placementId(Placement placement);
    static Placement placementFromId(const QString &id, Placement fallback);
    static QString placementTitle(Placement placement);

    const Descriptor &descriptor() const;

    /** Whether the user wants the panel at all */
    bool isPanelEnabled() const;
    /** Whether the panel hides itself while it has nothing to show (default: false) */
    bool autoHide() const;
    /** Whether the panel should currently be shown, given the preferences and its content */
    bool shouldBeShown() const;
    Placement placement() const;

    /** Whether the panel currently has something to show for the current context */
    virtual bool hasContent() const = 0;
    /** The widget which receives keyboard focus when the panel is entered */
    virtual QWidget *focusTarget() const = 0;

public slots:
    void setPanelEnabled(bool enabled);
    void setAutoHide(bool autoHide);
    void setPlacement(Gui::EmbeddedPanel::Placement placement);

signals:
    /** Emitted when the panel is shown or hidden because of the preferences or its content */
    void relevanceChanged(bool shown);
    void placementChanged(Gui::EmbeddedPanel::Placement placement);

protected:
    /** Child widgets are named after @arg objectName, for example attachmentPanelTitle */
    EmbeddedPanel(const Descriptor &descriptor, const QString &objectName, QWidget *parent);

    void setContentWidget(QWidget *content);
    QToolButton *addHeaderButton(const QString &objectNameSuffix, const QString &iconName, const QString &text);
    void setTitle(const QString &title);
    void showPlaceholder(const QString &text);
    void showContent();
    /** Re-evaluate shouldBeShown(); call whenever hasContent() may have changed */
    void updateVisibility();
    /** Called after the user turned the panel on or off, before its visibility is updated */
    virtual void panelEnabledChanged() {}
    /** Height of everything except the content area */
    int chromeHeight() const;

    static QIcon themedIcon(const QString &name);

    QLabel *m_title;
    QStackedWidget *m_stack;
    QLabel *m_placeholder;
    QHBoxLayout *m_header;

private:
    Descriptor m_descriptor;
    QWidget *m_content = nullptr;
    bool m_enabled;
    bool m_autoHide = false;
    Placement m_placement;
    int m_appliedVisibility = -1;
};

/** @short An embedded panel which follows the message shown in the reader */
class MessageContextPanel : public EmbeddedPanel
{
    Q_OBJECT
public:
    QModelIndex message() const;

public slots:
    /** Follow a message of the IMAP model; an invalid index means no message */
    void setMessage(const QModelIndex &message);

protected:
    MessageContextPanel(const Descriptor &descriptor, const QString &objectName, QWidget *parent);

    /** Rebuild the panel from message(); called after the message or its data changed */
    virtual void refresh() = 0;
    void scheduleRefresh();
    /** Whether the envelope of the message is available without triggering a fetch */
    bool messageIsFetched() const;

private:
    QPersistentModelIndex m_message;
    QList<QMetaObject::Connection> m_connections;
    QTimer *m_refreshTimer;
};

}
