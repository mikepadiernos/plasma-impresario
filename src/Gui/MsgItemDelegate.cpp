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

#include "MsgItemDelegate.h"
#include <QTextLayout>
#include <QTreeView>
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/SearchSnippets.h"
#include "Imap/Model/SpecialFolder.h"
#include "Imap/Model/Utils.h"
#include "MsgListView.h"
#include "UiUtils/Color.h"
#include "UiUtils/IconLoader.h"
#include <KLocalizedString>
#include <QApplication>
#include <QRegularExpression>
#include <QStyle>

namespace {

using Imap::Mailbox::MsgListModel;

int lineHeight(const QStyleOptionViewItem &option)
{
    QFont bold(option.font);
    bold.setBold(true);
    return qMax(option.fontMetrics.height(), QFontMetrics(bold).height());
}

QStyle *styleOf(const QStyleOptionViewItem &option)
{
    return option.widget ? option.widget->style() : QApplication::style();
}

QColor blend(const QColor &a, const QColor &b, qreal ratio)
{
    return QColor::fromRgbF(a.redF() * (1 - ratio) + b.redF() * ratio, a.greenF() * (1 - ratio) + b.greenF() * ratio,
                            a.blueF() * (1 - ratio) + b.blueF() * ratio);
}

}

namespace Gui
{

MsgItemDelegate::MsgItemDelegate(QObject *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTagsModel) :
    ColoredItemDelegate(parent), m_favoriteTagsModel(m_favoriteTagsModel)
{
}

QColor MsgItemDelegate::itemColor(const QModelIndex &index) const
{
    auto view = static_cast<MsgListView*>(parent());

    Q_ASSERT(index.isValid());
    QModelIndex sindex = index.sibling(index.row(), Imap::Mailbox::MsgListModel::SUBJECT);
    QVariant flags;
    if (!sindex.model()->hasChildren(sindex) || view->isExpanded(sindex)) {
        flags = index.data(Imap::Mailbox::RoleMessageFlags);
    } else {
        flags = index.data(Imap::Mailbox::RoleThreadAggregatedFlags);
    }

    return m_favoriteTagsModel->findBestColorForTags(flags.toStringList());
}

QFont MsgItemDelegate::itemFont(const QModelIndex &index) const
{
    QFont font;

    Q_ASSERT(index.isValid());

    if (index.data(Imap::Mailbox::RoleIsGroupHeader).toBool()) {
        font.setBold(true);
        return font;
    }

    // We will need the data, but asking for Flags or IsMarkedXYZ doesn't cause a fetch
    index.data(Imap::Mailbox::RoleMessageSubject);

    // These items should definitely *not* be rendered in bold
    if (!index.data(Imap::Mailbox::RoleIsFetched).toBool())
        return font;

    if (index.data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool())
        font.setStrikeOut(true);

    if (! index.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool()) {
        // If any message is marked as unread, show it in bold and be done with it
        font.setBold(true);
    } else if (index.model()->hasChildren(index.sibling(index.row(), 0)) &&
               index.data(Imap::Mailbox::RoleThreadRootWithUnreadMessages).toBool()) {
        // If this node is not marked as read, is a root of some thread and that thread
        // contains unread messages, display the thread's root underlined
        font.setUnderline(true);
    }

    if (index.column() == Imap::Mailbox::MsgListModel::SUBJECT && index.data(Imap::Mailbox::RoleMessageSubject).toString().isEmpty()) {
        font.setItalic(true);
    }

    return font;
}

void MsgItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem viewOption(option);
    auto foregroundColor = itemColor(index);
    if (foregroundColor.isValid())
        viewOption.palette.setColor(QPalette::Text, foregroundColor);
    viewOption.font = itemFont(index);
    ColoredItemDelegate::paintWithForeground(painter, viewOption, index, foregroundColor);
}

QSize MsgItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = ColoredItemDelegate::sizeHint(option, index);
    if (m_multiLine) {
        const int line = lineHeight(option);
        const int lines = m_snippetLine ? 3 : 2;
        size.setHeight(lines * line + (isComfortable() ? qMax(8, line) : qMax(4, line / 2)));
    }
    return size;
}

void MsgItemDelegate::setMultiLine(bool multiLine)
{
    m_multiLine = multiLine;
}

void MsgItemDelegate::setSenderInitials(bool shown)
{
    m_senderInitials = shown;
}

void MsgItemDelegate::setSnippetLine(bool shown)
{
    m_snippetLine = shown;
}

QString MsgItemDelegate::senderInitials(const QString &sender)
{
    static const QRegularExpression separators(QStringLiteral("[\\s,;]+"));
    QStringList letters;
    for (const QString &word : sender.split(separators, Qt::SkipEmptyParts)) {
        const QList<uint> codePoints = word.toUcs4();
        for (const uint codePoint : codePoints) {
            if (QChar::isLetterOrNumber(codePoint)) {
                const char32_t character = codePoint;
                letters << QString::fromUcs4(&character, 1);
                break;
            }
        }
    }
    if (letters.isEmpty())
        return QString();
    return (letters.size() == 1 ? letters.constFirst() : letters.constFirst() + letters.constLast()).toUpper();
}

QString MsgItemDelegate::correspondent(const QModelIndex &index)
{
    using Imap::Mailbox::SpecialUse;
    const QModelIndex mailbox = Imap::deproxifiedIndex(index).parent().parent();
    const auto use =
        mailbox.isValid() ? static_cast<SpecialUse>(mailbox.data(Imap::Mailbox::RoleMailboxSpecialUse).toInt()) : SpecialUse::None;
    if (use == SpecialUse::Sent || use == SpecialUse::Drafts) {
        const QString recipients = index.sibling(index.row(), MsgListModel::TO).data(Qt::DisplayRole).toString();
        if (!recipients.isEmpty())
            return i18nc("@label recipients of a sent message in the message list", "To: %1", recipients);
    }
    return index.sibling(index.row(), MsgListModel::FROM).data(Qt::DisplayRole).toString();
}

MsgItemDelegate::RowLayout MsgItemDelegate::multiLineLayout(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    RowLayout layout;
    const QRect rect = option.rect;
    QStyle *style = styleOf(option);
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 2;
    const int line = lineHeight(option);
    const int iconSize = qMin(line, style->pixelMetric(QStyle::PM_SmallIconSize, &option, option.widget));
    const int lines = m_snippetLine ? 3 : 2;
    const int top = rect.top() + qMax(0, (rect.height() - lines * line) / 2);

    int left = rect.left() + margin;
    const int right = rect.right() - margin;
    const int markerWidth = qMax(8, line / 2 + 2);
    layout.readState = QRect(left, rect.top(), markerWidth, rect.height());
    left += markerWidth + margin / 2;
    if (m_senderInitials) {
        const int side = qMax(0, qMin(2 * line, rect.height() - 4));
        layout.avatar = QRect(left, rect.top() + (rect.height() - side) / 2, side, side);
        left += side + margin;
    }

    QRect first(left, top, qMax(0, right - left + 1), line);
    QRect second(left, top + line, qMax(0, right - left + 1), line);
    if (m_snippetLine)
        layout.snippet = QRect(left, top + 2 * line, qMax(0, right - left + 1), line);

    const QString date = index.sibling(index.row(), MsgListModel::DATE).data(Qt::DisplayRole).toString();
    const int dateWidth = option.fontMetrics.horizontalAdvance(date);
    layout.date = QRect(first.right() - dateWidth + 1, first.top(), dateWidth, line);
    const int iconTop = first.top() + (line - iconSize) / 2;
    layout.flag = QRect(layout.date.left() - margin - iconSize, iconTop, iconSize, iconSize);
    int textRight = layout.flag.left() - margin / 2;
    if (index.data(Imap::Mailbox::RoleMessageHasAttachments).toBool()) {
        layout.attachment = QRect(textRight - iconSize, iconTop, iconSize, iconSize);
        textRight = layout.attachment.left() - margin / 2;
    }
    first.setRight(qMax(first.left(), textRight - margin / 2));

    if (index.data(Imap::Mailbox::RoleMessageIsMarkedReplied).toBool() ||
        index.data(Imap::Mailbox::RoleMessageIsMarkedForwarded).toBool()) {
        layout.status = QRect(second.left(), second.top() + (line - iconSize) / 2, iconSize, iconSize);
        second.setLeft(layout.status.right() + 1 + margin / 2);
    }
    // Only lists of several accounts name the account, and search results the folder too; at most a third of the line
    const QString account = index.data(Imap::Mailbox::RoleMessageRowLabel).toString();
    if (!account.isEmpty()) {
        const int accountWidth = qMin(option.fontMetrics.horizontalAdvance(account), second.width() / 3);
        layout.source = QRect(second.right() - accountWidth + 1, second.top(), accountWidth, line);
        second.setRight(qMax(second.left(), layout.source.left() - margin));
    }
    layout.firstLine = first;
    layout.secondLine = second;

    // Mirror the layout in right-to-left languages
    for (QRect *part : {&layout.readState, &layout.avatar, &layout.firstLine, &layout.date, &layout.flag, &layout.attachment,
                        &layout.status, &layout.secondLine, &layout.source, &layout.snippet}) {
        if (part->isValid())
            *part = QStyle::visualRect(option.direction, rect, *part);
    }
    return layout;
}

QIcon MsgItemDelegate::indicatorIcon(const QString &name) const
{
    auto it = m_icons.constFind(name);
    if (it == m_icons.constEnd())
        it = m_icons.insert(name, UiUtils::SymbolicIcon::fromTheme(name));
    return *it;
}

void MsgItemDelegate::paintMultiLineRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    using namespace Imap::Mailbox;
    QStyleOptionViewItem opt(option);
    const QColor tagColor = itemColor(index);
    applyForegroundToSelection(opt, tagColor);

    // Background, selection and focus come from the style, as for any other item
    opt.text.clear();
    opt.icon = QIcon();
    opt.features = QStyleOptionViewItem::None;
    opt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
    styleOf(opt)->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    // Asking for the subject fetches the envelope; the flags and the date come with it
    const QFont subjectFont = itemFont(index);
    const bool fetched = index.data(RoleIsFetched).toBool();
    const RowLayout layout = multiLineLayout(opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
                                       : (opt.state & QStyle::State_Active) ? QPalette::Normal
                                                                            : QPalette::Inactive;
    const QColor text = selected             ? opt.palette.color(group, QPalette::HighlightedText)
                        : tagColor.isValid() ? tagColor
                                             : opt.palette.color(group, QPalette::Text);
    const QColor secondary =
        selected ? text : blend(opt.palette.color(group, QPalette::Text), opt.palette.color(group, QPalette::Base), 0.35);
    const QIcon::Mode iconMode = selected ? QIcon::Selected : QIcon::Normal;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(opt.rect);

    const bool unread = fetched && !index.data(RoleMessageIsMarkedRead).toBool();
    if (unread || (fetched && hovered)) {
        // Unread is a filled dot in the accent color, distinct from the selection; hovering offers the toggle
        const int dot = qMax(6, lineHeight(opt) / 3);
        QRect dotRect(0, 0, dot, dot);
        dotRect.moveCenter(QPoint(layout.readState.center().x(), layout.firstLine.center().y()));
        if (unread) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? text : opt.palette.color(group, QPalette::Highlight));
        } else {
            painter->setPen(QPen(secondary, 1));
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawEllipse(dotRect);
    }

    const QString who = fetched ? correspondent(index) : QString();
    if (layout.avatar.isValid() && !who.isEmpty()) {
        const QString initials = senderInitials(who);
        const bool darkBase = opt.palette.color(QPalette::Base).lightnessF() < 0.5;
        const QColor fill = QColor::fromHslF((qHash(who) % 360) / 360.0, 0.45, darkBase ? 0.38 : 0.68);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawEllipse(layout.avatar);
        QFont badgeFont(opt.font);
        badgeFont.setBold(true);
        badgeFont.setPixelSize(qMax(6, layout.avatar.height() * 2 / 5));
        painter->setFont(badgeFont);
        painter->setPen(UiUtils::readableTextColor(fill));
        painter->drawText(layout.avatar, Qt::AlignCenter, initials);
    }

    const Qt::Alignment leading = QStyle::visualAlignment(opt.direction, Qt::AlignLeft | Qt::AlignVCenter);
    const Qt::Alignment trailing = QStyle::visualAlignment(opt.direction, Qt::AlignRight | Qt::AlignVCenter);

    QFont senderFont(subjectFont);
    senderFont.setItalic(false);
    painter->setFont(senderFont);
    painter->setPen(text);
    painter->drawText(layout.firstLine, leading, QFontMetrics(senderFont).elidedText(who, Qt::ElideRight, layout.firstLine.width()));

    painter->setFont(opt.font);
    painter->setPen(secondary);
    painter->drawText(layout.date, trailing, index.sibling(index.row(), MsgListModel::DATE).data(Qt::DisplayRole).toString());

    if (fetched && index.data(RoleMessageIsMarkedFlagged).toBool()) {
        indicatorIcon(QStringLiteral("mail-flagged")).paint(painter, layout.flag, Qt::AlignCenter, iconMode);
    } else if (fetched && hovered) {
        painter->setOpacity(0.5);
        indicatorIcon(QStringLiteral("mail-unflagged")).paint(painter, layout.flag, Qt::AlignCenter, iconMode);
        painter->setOpacity(1);
    }
    if (layout.attachment.isValid())
        indicatorIcon(QStringLiteral("mail-attachment")).paint(painter, layout.attachment, Qt::AlignCenter, iconMode);
    if (layout.status.isValid()) {
        const bool replied = index.data(RoleMessageIsMarkedReplied).toBool();
        const bool forwarded = index.data(RoleMessageIsMarkedForwarded).toBool();
        const QString name = replied && forwarded ? QStringLiteral("mail-forwarded-replied")
                             : replied            ? QStringLiteral("mail-replied")
                                                  : QStringLiteral("mail-forwarded");
        indicatorIcon(name).paint(painter, layout.status, Qt::AlignCenter, iconMode);
    }

    painter->setFont(subjectFont);
    painter->setPen(text);
    const QString subject = index.sibling(index.row(), MsgListModel::SUBJECT).data(Qt::DisplayRole).toString();
    painter->drawText(layout.secondLine, leading, QFontMetrics(subjectFont).elidedText(subject, Qt::ElideRight, layout.secondLine.width()));
    if (layout.source.isValid()) {
        painter->setFont(opt.font);
        painter->setPen(secondary);
        const QString account = index.data(RoleMessageRowLabel).toString();
        painter->drawText(layout.source, trailing, opt.fontMetrics.elidedText(account, Qt::ElideRight, layout.source.width()));
    }
    if (layout.snippet.isValid()) {
        const auto snippet = index.data(RoleMessageSearchSnippet).value<SearchSnippet>();
        if (!snippet.text.isEmpty()) {
            // Elided as if all of it were bold, so that the marked matches still fit
            QFont bold(opt.font);
            bold.setBold(true);
            const QString shown = QFontMetrics(bold).elidedText(snippet.text, Qt::ElideRight, layout.snippet.width());
            const int kept = shown == snippet.text ? int(shown.size()) : int(shown.size()) - 1;
            QList<QTextLayout::FormatRange> marks;
            for (const auto &match : snippet.matches) {
                const int length = qMin(match.second, kept - match.first);
                if (length <= 0)
                    break;
                QTextLayout::FormatRange mark;
                mark.start = match.first;
                mark.length = length;
                mark.format.setFontWeight(QFont::Bold);
                mark.format.setForeground(text);
                marks << mark;
            }
            QTextLayout textLayout(shown, opt.font);
            QTextOption textOption(QStyle::visualAlignment(opt.direction, Qt::AlignLeft));
            textOption.setTextDirection(opt.direction);
            textOption.setWrapMode(QTextOption::NoWrap);
            textLayout.setTextOption(textOption);
            textLayout.setFormats(marks);
            textLayout.beginLayout();
            QTextLine textLine = textLayout.createLine();
            if (textLine.isValid()) {
                textLine.setLineWidth(layout.snippet.width());
                textLine.setPosition(QPointF(0, 0));
            }
            textLayout.endLayout();
            painter->setPen(secondary);
            const qreal y = layout.snippet.top() + (layout.snippet.height() - (textLine.isValid() ? textLine.height() : 0)) / 2;
            textLayout.draw(painter, QPointF(layout.snippet.left(), y));
        }
    }
    painter->restore();
}

MsgItemDelegate::RowHit MsgItemDelegate::multiLineHit(const QStyleOptionViewItem &option, const QModelIndex &index, const QPoint &pos) const
{
    if (!index.data(Imap::Mailbox::RoleIsFetched).toBool())
        return RowHit::None;
    const RowLayout layout = multiLineLayout(option, index);
    if (layout.flag.adjusted(-2, -2, 2, 2).contains(pos))
        return RowHit::Flag;
    if (layout.readState.contains(pos))
        return RowHit::ReadState;
    return RowHit::None;
}

}
