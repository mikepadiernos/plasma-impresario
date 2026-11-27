/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#ifndef MSGITEMDELEGATE_H
#define MSGITEMDELEGATE_H

#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>
#include "Imap/Model/FavoriteTagsModel.h"
#include "ColoredItemDelegate.h"

namespace Gui {

/** @short Painting colorized messages in MsgListView
*/
class MsgItemDelegate : public ColoredItemDelegate
{
    Q_OBJECT
public:
    /** @short Indicator of a multi-line row that acts on the message when clicked */
    enum class RowHit { None, ReadState, Flag };
    explicit MsgItemDelegate(QObject* parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /** @short Size rows for two lines: the sender, indicators and date above the subject */
    void setMultiLine(bool multiLine);
    bool isMultiLine() const { return m_multiLine; }
    /** @short Show a badge with the initials of the sender in multi-line rows; nothing is fetched for it */
    void setSenderInitials(bool shown);
    bool showsSenderInitials() const { return m_senderInitials; }
    /** @short Add a third line to multi-line rows for the snippet of a search, with the matches in bold */
    void setSnippetLine(bool shown);
    bool showsSnippetLine() const { return m_snippetLine; }
    /** @short Show multi-line rows as cards with a margin, a frame and the account or folder colour */
    void setCards(bool cards);
    bool showsCards() const { return m_cards; }
    /** @short The account the list shows, for folder colours; unified lists name the account of each message */
    void setAccountId(const QString &accountId);

    /** @short The account or folder colour of the message @arg index, or an invalid colour */
    QColor sourceColor(const QModelIndex &index) const;

    /** @short Paint the whole multi-line row of the message @arg index into option.rect */
    void paintMultiLineRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    /** @short Indicator at @arg pos in the multi-line row of @arg index laid out in option.rect */
    RowHit multiLineHit(const QStyleOptionViewItem &option, const QModelIndex &index, const QPoint &pos) const;

    /** @short Initials of a display name or address, such as "JD" for "Jane Doe" */
    static QString senderInitials(const QString &sender);
    /** @short Sender, or the recipients in Sent and Drafts, as shown in a multi-line row */
    static QString correspondent(const QModelIndex &index);

private:
    struct RowLayout
    {
        QRect readState;
        QRect avatar;
        QRect firstLine;
        QRect date;
        QRect flag;
        QRect attachment;
        /** Shown while the message is marked as junk */
        QRect junk;
        QRect status;
        QRect secondLine;
        /** The account of a message in a list which shows several accounts */
        QRect source;
        /** The snippet of a search, when the rows show it */
        QRect snippet;
    };
    RowLayout multiLineLayout(const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QColor itemColor(const QModelIndex &index) const;
    /** @short The colour of the text of a row which is not selected: a tag's, else a custom flagged or unread
     * colour, else invalid */
    QColor stateColor(const QModelIndex &index, const QColor &tagColor) const;
    int cardGap(const QStyleOptionViewItem &option) const;
    QRect cardContents(const QStyleOptionViewItem &option) const;
    void paintCard(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paintTinted(QPainter *painter, const QIcon &icon, const QRect &rect, QIcon::Mode mode, const QColor &color) const;
    QFont itemFont(const QModelIndex &index) const;
    QIcon indicatorIcon(const QString &name) const;

    Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel;
    bool m_multiLine = false;
    bool m_senderInitials = true;
    bool m_snippetLine = false;
    bool m_cards = false;
    QString m_accountId;
    mutable QHash<QString, QIcon> m_icons;
};

}

#endif	/* MSGITEMDELEGATE_H */
