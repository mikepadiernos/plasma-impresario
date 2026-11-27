/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QFont>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QPointer>
#include <QStringList>

#include "UiUtils/FolderColors.h"
#include "UiUtils/Theme.h"

class KConfigBase;
class QWidget;

namespace Gui {

/** @short Applies the appearance options of the Theme group to the application

There is one instance, which every window, delegate and the widget style ask for
the appearance in effect. Applying never changes the desktop: the palette, fonts
and stylesheet are set on this application only, and window corners and
decorations stay with KWin.

With every option at its default nothing is touched, so that the application
looks exactly as Plasma draws it.
*/
class AppearanceManager : public QObject {
    Q_OBJECT
public:
    static AppearanceManager *instance();

    /** @short Where a widget's font comes from */
    enum class FontTarget {
        MessageList,
        FolderTree,
        MessageHeaders,
        Tabs,
        StatusBar,
    };

    /** @short Read the global options, the theme profiles, the colours and the
    workspace of @arg activityId or @arg accountId from @arg config, and apply
    them

    Problems, such as a missing profile, are kept in problems().
    */
    void load(const KConfigBase &config, const QString &activityId = QString(), const QString &accountId = QString());
    /** @short Apply @arg values, already resolved and validated, and @arg colors
     */
    void apply(const UiUtils::Theme::Values &values, const UiUtils::FolderColors::Colors &colors);
    /** @short Ignore the appearance of workspaces, as asked for by
     * --no-workspace-appearance */
    void setWorkspaceAppearanceDisabled(bool disabled);
    bool workspaceAppearanceDisabled() const;

    const UiUtils::Theme::Values &values() const;
    QVariant value(const QString &entry) const;
    /** @short Problems found while loading, in sentences */
    QStringList problems() const;
    /** @short Whether the colour scheme in effect is a high-contrast one */
    bool highContrast() const;
    /** @short Whether blur, translucency, tints and radii are suspended for a
     * high-contrast scheme */
    bool effectsSuspended() const;
    /** @short Whether KWin can blur behind windows now */
    static bool blurAvailable();

    QColor accent() const;
    /** @short The colour of unread or flagged messages, or an invalid colour
     * for the usual look */
    QColor unreadColor() const;
    QColor flaggedColor() const;
    const UiUtils::FolderColors::Colors &folderColors() const;
    QColor accountColor(const QString &accountId) const;
    QColor folderColor(const QString &accountId, const QString &folder, QChar separator) const;

    /** @short The corner radius in device-independent pixels, or -1 for the
     * widget style's */
    int cornerRadius() const;
    int spacingScale() const;
    /** @short How long a transition of @arg baseMs lasts now */
    int animationDuration(int baseMs) const;
    /** @short The font of plain-text messages: the chosen one, else the
     * system's fixed font */
    QFont plainTextFont() const;

    /** @short Apply opacity and blur to @arg window, a top-level window

    Translucency needs the window to be created translucent; when it was not,
    the change applies when the window next opens.
    */
    void registerWindow(QWidget *window);
    /** @short Tint @arg pane, a list, a tree or the reader, as PaneTint asks */
    void registerPane(QWidget *pane);
    /** @short Give @arg widget the font chosen for @arg target */
    void registerFont(QWidget *widget, FontTarget target);

    /** @short Installed KDE colour schemes, by the file's base name, with their
     * display names */
    static QList<std::pair<QString, QString>> installedColorSchemes();
    /** @short The file of the installed scheme @arg name, or an empty string */
    static QString colorSchemePath(const QString &name);
    /** @short The palette of @arg scheme: a preset, an installed scheme, or
     * empty for Plasma's */
    static QPalette schemePalette(const QString &scheme, bool *found = nullptr);
    /** @short Plasma's animation speed factor, 1 for normal */
    static qreal plasmaAnimationFactor();

signals:
    /** @short The appearance in effect changed; delegates and views repaint */
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit AppearanceManager(QObject *parent);
    void applyPalette();
    void applyFonts();
    void applyStyleSheet();
    void applyWindow(QWidget *window);
    void updatePaneTint();
    void repolish();
    QPalette paneBasePalette(QWidget *pane) const;

    UiUtils::Theme::Values m_values;
    UiUtils::FolderColors::Colors m_colors;
    UiUtils::Theme::Colors m_customColors;
    QStringList m_problems;
    bool m_workspaceAppearanceDisabled = false;
    bool m_highContrast = false;
    bool m_suspended = false;
    /** The palette before this application changed it, to return to */
    QPalette m_systemPalette;
    bool m_paletteChanged = false;
    bool m_styleSheetChanged = false;
    bool m_applying = false;
    QList<QPointer<QWidget>> m_windows;
    QList<QPointer<QWidget>> m_panes;
    QHash<QWidget *, FontTarget> m_fontTargets;
    QPointer<QWidget> m_activePane;
};

} // namespace Gui
