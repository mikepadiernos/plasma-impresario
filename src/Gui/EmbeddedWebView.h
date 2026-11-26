/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QSettings>
#include <QWebEngineView>

namespace Gui
{

class WebEngineMessagePage;
class WebEngineMessageProfile;

/** @short Whether an http or https link from a message may be opened

A link without warning signs is allowed at once. Otherwise a dialog shows the
real destination and the reasons for caution, and returns whether the user chose
to open it anyway. Cancel is the default.
@param url The link target.
@param text The link's text, used to spot text which shows another address.
*/
bool confirmExternalLink(QWidget *parent, const QUrl &url, const QString &text);

/** An embeddable Qt WebEngine view for message content.

  The view uses a per-message profile and sizes itself to the rendered content
  so the containing message scroll area remains the primary vertical scroller.
*/
class EmbeddedWebView : public QWebEngineView
{
    Q_OBJECT
public:
    enum class ColorScheme {
        /** @short System's color scheme, but let the content override this */
        System,
        /** @short System's color scheme adjusted towards a compromise for reasonable contrats on funny backgrounds */
        AdjustedSystem,
        /** @short Use boring black-text-on-white-background */
        BlackOnWhite,
    };

    /** Creates a view bound to a single message profile.
     * @param parent Parent widget.
     * @param profile Per-message WebEngine profile.
     * @param profileSettings Message-view settings.
     */
    EmbeddedWebView(QWidget *parent, WebEngineMessageProfile *profile, QSettings *profileSettings);
    QSize sizeHint() const;
    QWidget *scrollParent() const;
    /** @returns The page that loads IMAP message parts. */
    WebEngineMessagePage *messagePage() const;
    /** Sets a fixed viewport width for content such as large plain-text parts.
     * @param staticWidth Width in logical pixels; zero restores adaptive width.
     */
    void setStaticWidth(int staticWidth);
    int staticWidth() const;
    static std::map<ColorScheme, QString> supportedColorSchemes();
public slots:
    void changeColorScheme(const ColorScheme colorScheme);
protected:
    void changeEvent(QEvent *e);
    bool eventFilter(QObject *o, QEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void showEvent(QShowEvent *se);
    void addCustomStylesheet(const QString &css);
    void constrainSize();
private:
    void findScrollParent();
    void loadColorScheme();
    void setColorScheme(const ColorScheme colorScheme);
private slots:
    void autoScroll();
    void slotLinkClicked(const QUrl &url, const QString &text);
    void handlePageLoadFinished(bool success);
private:
    WebEngineMessagePage *m_page;
    QWidget *m_scrollParent;
    int m_scrollParentPadding;
    int m_resizeInProgress;
    QTimer *m_autoScrollTimer;
    QTimer *m_sizeContrainTimer;
    int m_autoScrollPixels;
    int m_staticWidth;
    QString m_customCss;
    QSettings *m_settings;
protected:
    ColorScheme m_colorScheme;
};

}

Q_DECLARE_METATYPE(Gui::EmbeddedWebView::ColorScheme)

#endif // EMBEDDEDWEBVIEW_H
