/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>
   Copyright (C) 2015 Erik Quaeghebeur <trojita@equaeghe.nospammail.net>
   Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

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

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <KLocalizedString>

#include "TagListWidget.h"
#include "FlowLayout.h"
#include "TagAddDialog.h"
#include "TagWidget.h"
#include "Gui/Util.h"
#include "Imap/Model/SpecialFlagNames.h"

namespace Gui
{

TagListWidget::TagListWidget(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTags) :
    QWidget(parent)
    , m_favoriteTags(m_favoriteTags)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    parentLayout = new FlowLayout(this, 0);
    setLayout(parentLayout);

    addButton = TagWidget::addingWidget();
    connect(addButton, &TagWidget::addingClicked, this, &TagListWidget::newTagsRequested);

    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::mdnsent.toLower());
    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::submitted.toLower());
    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::submitpending.toLower());
}

void TagListWidget::setTagList(QStringList list)
{
    m_tags = list;
    rebuild();
}

void TagListWidget::setLabelList(const QStringList &labels)
{
    if (labels == m_labels)
        return;
    m_labels = labels;
    rebuild();
}

QString TagListWidget::gmailLabelDisplayName(const QString &label)
{
    if (!label.startsWith(QLatin1Char('\\')))
        return label;
    const QString name = label.mid(1).toLower();
    // Starred and Draft already show as the message's flag and folder
    if (name == QLatin1String("starred") || name == QLatin1String("draft") || name.isEmpty())
        return QString();
    if (name == QLatin1String("inbox"))
        return i18nc("@label Gmail system label", "Inbox");
    if (name == QLatin1String("important"))
        return i18nc("@label Gmail system label", "Important");
    if (name == QLatin1String("sent"))
        return i18nc("@label Gmail system label", "Sent");
    if (name == QLatin1String("trash"))
        return i18nc("@label Gmail system label", "Trash");
    if (name == QLatin1String("spam"))
        return i18nc("@label Gmail system label", "Spam");
    return label.mid(1);
}

void TagListWidget::rebuild()
{
    empty();
    parentLayout->removeWidget(addButton);

    for (const QString &label : std::as_const(m_labels)) {
        const QString displayName = gmailLabelDisplayName(label);
        if (displayName.isEmpty())
            continue;
        TagWidget *lbl = TagWidget::serverLabel(label, displayName);
        lbl->setToolTip(i18nc("@info:tooltip", "Gmail label %1", displayName));
        parentLayout->addWidget(lbl);
        children << lbl;
    }

    Q_FOREACH(const QString &tagName, m_tags) {
        QString tagNameLowerCase = tagName.toLower();
        if (Imap::Mailbox::FlagNames::toCanonical.contains(tagNameLowerCase)) {
            if (unsupportedReservedKeywords.contains(tagNameLowerCase)) {
                TagWidget *lbl = TagWidget::systemFlag(tagName);
                parentLayout->addWidget(lbl);
                children << lbl;
            } else {
                continue;
            }
        } else {
            TagWidget *lbl = TagWidget::userKeyword(tagName, m_favoriteTags);
            parentLayout->addWidget(lbl);
            connect(lbl, &TagWidget::removeClicked, this, &TagListWidget::tagRemoved);
            children << lbl;
        }
    }

    parentLayout->addWidget(addButton);
}

void TagListWidget::empty()
{
    qDeleteAll(children.begin(), children.end());
    children.clear();
}

void TagListWidget::newTagsRequested()
{
    QStringList tagList = TagAddDialog::getTags(this, m_favoriteTags);

    // Check whether any text has been entered
    if (tagList.isEmpty()) {
        return;
    }

    // Check whether reserved keywords have been entered
    QStringList reservedTagsList = QStringList();
    for (QStringList::const_iterator it = tagList.constBegin(); it != tagList.constEnd(); ++it) {
        if (Imap::Mailbox::FlagNames::toCanonical.contains(it->toLower())) {
            reservedTagsList << *it;
        }
    }
    if (!reservedTagsList.isEmpty()) {
        Gui::Util::messageBoxWarning(this, tr("Disallowed tag value"),
                                     tr("No tags were set because the following given tag(s) are reserved "
                                        "and have been disallowed from being set in this way: %1.")
                                     .arg(reservedTagsList.join(QStringLiteral(", "))));
        return;
    }

    emit tagAdded(tagList.join(QLatin1Char(' ')));
}

}
