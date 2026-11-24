/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/ContactPanel.h"
#include "Gui/EmbeddedPanelsMenu.h"
#include "Gui/MessageDetailsPanel.h"
#include "Gui/RelatedMessagesPanel.h"
#include "Imap/Model/ItemRoles.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QVariantList addresses(const QStringList &mailboxes)
{
    QVariantList out;
    for (const auto &mailbox : mailboxes) {
        const auto parts = mailbox.split(QLatin1Char('@'));
        out << QStringList{parts.at(0).toUpper(), QString(), parts.at(0), parts.value(1)};
    }
    return out;
}

/** A fetched message as Imap::Mailbox::Model reports it */
QStandardItem *messageItem(const QString &subject, const QString &from, bool fetched = true)
{
    auto *item = new QStandardItem(subject);
    item->setData(fetched, Imap::Mailbox::RoleIsFetched);
    item->setData(subject, Imap::Mailbox::RoleMessageSubject);
    item->setData(addresses({from}), Imap::Mailbox::RoleMessageFrom);
    return item;
}

/** Hosts panels in a shown window, like the reader column does */
struct Host {
    QWidget window;
    QVBoxLayout *layout;
    Host()
        : layout(new QVBoxLayout(&window))
    {
        layout->addWidget(new QWidget(&window), 1);
        window.resize(500, 600);
        window.show();
    }
    template<typename T>
    T *add(T *panel)
    {
        layout->addWidget(panel);
        return panel;
    }
};

template<typename T>
T *child(QWidget *panel, const QString &name)
{
    return panel->findChild<T *>(name);
}

QString placeholder(QWidget *panel)
{
    return child<QLabel>(panel, panel->objectName() + QLatin1String("Placeholder"))->text();
}

}

class EmbeddedPanelsTest : public QObject
{
    Q_OBJECT
private slots:
    void descriptorsAndDefaults()
    {
        const auto details = Gui::MessageDetailsPanel::panelDescriptor();
        const auto contacts = Gui::ContactPanel::panelDescriptor();
        const auto related = Gui::RelatedMessagesPanel::panelDescriptor();
        QCOMPARE(details.configKey, QStringLiteral("MessageDetailsPanel"));
        QCOMPARE(contacts.configKey, QStringLiteral("ContactPanel"));
        QCOMPARE(related.configKey, QStringLiteral("RelatedMessagesPanel"));
        // Off by default so that the shell keeps the classic look
        QVERIFY(!details.defaultEnabled);
        QVERIFY(!contacts.defaultEnabled);
        QVERIFY(!related.defaultEnabled);
        QCOMPARE(details.defaultPlacement, Gui::EmbeddedPanel::Placement::RightColumn);

        Gui::MessageDetailsPanel panel;
        QCOMPARE(panel.objectName(), QStringLiteral("messageDetailsPanel"));
        QVERIFY(!panel.isPanelEnabled());
        QVERIFY(!panel.shouldBeShown());
        QCOMPARE(placeholder(&panel), i18nc("@info", "No message selected"));

        for (const auto placement : Gui::EmbeddedPanel::placements()) {
            const QString id = Gui::EmbeddedPanel::placementId(placement);
            QCOMPARE(Gui::EmbeddedPanel::placementFromId(id, Gui::EmbeddedPanel::Placement::Footer), placement);
            QVERIFY(!Gui::EmbeddedPanel::placementTitle(placement).isEmpty());
        }
        QCOMPARE(Gui::EmbeddedPanel::placementFromId(QStringLiteral("nowhere"), Gui::EmbeddedPanel::Placement::Footer),
                 Gui::EmbeddedPanel::Placement::Footer);

        QSignalSpy placementSpy(&panel, &Gui::EmbeddedPanel::placementChanged);
        panel.setPlacement(Gui::EmbeddedPanel::Placement::LeftColumn);
        panel.setPlacement(Gui::EmbeddedPanel::Placement::LeftColumn);
        QCOMPARE(placementSpy.count(), 1);
        QCOMPARE(panel.placement(), Gui::EmbeddedPanel::Placement::LeftColumn);
    }

    void detailsFollowTheMessage()
    {
        QStandardItemModel model;
        auto *item = messageItem(QStringLiteral("Quarterly report"), QStringLiteral("ann@example.org"), false);
        item->setData(addresses({QStringLiteral("bob@example.org"), QStringLiteral("carol@example.org")}), Imap::Mailbox::RoleMessageTo);
        item->setData(QStringLiteral("INBOX"), Imap::Mailbox::RoleMailboxName);
        item->setData(true, Imap::Mailbox::RoleMessageIsMarkedFlagged);
        item->setData(quint64(4096), Imap::Mailbox::RoleMessageSize);
        auto *root = new QStandardItem(QStringLiteral("root"));
        root->setData(QStringLiteral("multipart/signed"), Imap::Mailbox::RolePartMimeType);
        item->appendRow(root);
        model.appendRow(item);

        Host host;
        auto *panel = host.add(new Gui::MessageDetailsPanel);
        QSignalSpy relevance(panel, &Gui::EmbeddedPanel::relevanceChanged);
        panel->setPanelEnabled(true);
        panel->setAutoHide(true);
        QVERIFY(panel->isHidden());

        panel->setMessage(item->index());
        QVERIFY(!panel->hasContent());
        QVERIFY(panel->isHidden());
        QCOMPARE(placeholder(panel), i18nc("@info", "Loading message…"));

        // The envelope arrives later and is announced through dataChanged
        item->setData(true, Imap::Mailbox::RoleIsFetched);
        QTRY_VERIFY(panel->hasContent());
        QVERIFY(panel->isVisible());
        QCOMPARE(relevance.last().constFirst().toBool(), true);
        QCOMPARE(panel->fieldText(QStringLiteral("Subject")), QStringLiteral("Quarterly report"));
        QVERIFY(panel->fieldText(QStringLiteral("From")).contains(QStringLiteral("ANN")));
        QVERIFY(panel->fieldText(QStringLiteral("To")).contains(QStringLiteral("CAROL")));
        QVERIFY(panel->fieldText(QStringLiteral("Cc")).isEmpty());
        QCOMPARE(panel->fieldText(QStringLiteral("Folder")), QStringLiteral("INBOX"));
        QVERIFY(!panel->fieldText(QStringLiteral("Size")).isEmpty());
        QCOMPARE(panel->fieldText(QStringLiteral("Status")),
                 i18nc("@info message status", "Unread") + i18nc("@info separator of message status", ", ")
                     + i18nc("@info message status", "Flagged"));
        QCOMPARE(panel->fieldText(QStringLiteral("Security")), i18nc("@info message security", "Signed"));

        item->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
        QTRY_VERIFY(panel->fieldText(QStringLiteral("Status")).startsWith(i18nc("@info message status", "Read")));

        panel->setMessage(QModelIndex());
        QVERIFY(panel->isHidden());
        QCOMPARE(placeholder(panel), i18nc("@info", "No message selected"));

        // Without auto-hide, the panel stays and explains why it is empty
        panel->setAutoHide(false);
        QVERIFY(panel->isVisible());
        panel->setPanelEnabled(false);
        QVERIFY(panel->isHidden());
    }

    void sizeHintIgnoresContent()
    {
        // Splitters size a panel when it is placed, usually before it shows a message
        QStandardItemModel model;
        auto *item = messageItem(QStringLiteral("Plans"), QStringLiteral("ann@example.org"));
        item->setData(addresses({QStringLiteral("bob@example.org"), QStringLiteral("carol@example.org"),
                                 QStringLiteral("dave@example.org"), QStringLiteral("erin@example.org")}),
                      Imap::Mailbox::RoleMessageTo);
        model.appendRow(item);

        Host host;
        auto *details = host.add(new Gui::MessageDetailsPanel);
        auto *contacts = host.add(new Gui::ContactPanel);
        for (Gui::MessageContextPanel *panel : std::initializer_list<Gui::MessageContextPanel *>{details, contacts}) {
            panel->setPanelEnabled(true);
            const QSize empty = panel->sizeHint();
            QVERIFY(empty.height() > 0);
            panel->setMessage(item->index());
            QVERIFY(panel->hasContent());
            QCOMPARE(panel->sizeHint(), empty);
            panel->setMessage(QModelIndex());
            QCOMPARE(panel->sizeHint(), empty);
        }
        // Details ask for more rows than contacts
        QVERIFY(details->sizeHint().height() > contacts->sizeHint().height());
    }

    void detailsForgetDestroyedModel()
    {
        auto *model = new QStandardItemModel;
        auto *item = messageItem(QStringLiteral("Gone soon"), QStringLiteral("ann@example.org"));
        model->appendRow(item);
        Gui::MessageDetailsPanel panel;
        panel.setPanelEnabled(true);
        panel.setMessage(item->index());
        QVERIFY(panel.hasContent());
        delete model;
        QTRY_VERIFY(!panel.hasContent());
        QVERIFY(!panel.message().isValid());
    }

    void distinctReplyToIsListed()
    {
        QStandardItemModel model;
        auto *item = messageItem(QStringLiteral("Plans"), QStringLiteral("ann@example.org"));
        item->setData(addresses({QStringLiteral("ann@example.org"), QStringLiteral("team@example.org")}),
                      Imap::Mailbox::RoleMessageReplyTo);
        model.appendRow(item);

        Host host;
        auto *panel = host.add(new Gui::ContactPanel);
        panel->setPanelEnabled(true);
        panel->setMessage(item->index());
        QCOMPARE(panel->contactCount(), 2);
        auto *list = child<QListWidget>(panel, QStringLiteral("contactPanelList"));
        QVERIFY(list);
        QVERIFY(list->item(0)->text().contains(i18nc("@info contact role", "Reply-To")));
        QVERIFY(panel->contactText(1).contains(QStringLiteral("team@example.org")));
        QVERIFY(list->item(1)->text().contains(i18nc("@info contact role", "Reply-To")));
    }

    void contactsAreListedOnce()
    {
        QStandardItemModel model;
        auto *item = messageItem(QStringLiteral("Plans"), QStringLiteral("ann@example.org"));
        item->setData(addresses({QStringLiteral("ANN@example.org")}), Imap::Mailbox::RoleMessageReplyTo);
        item->setData(addresses({QStringLiteral("bob@example.org")}), Imap::Mailbox::RoleMessageTo);
        QVariantList cc = addresses({QStringLiteral("ann@example.org")});
        cc << QStringList{QString(), QString(), QStringLiteral("undisclosed-recipients"), QString()};
        item->setData(cc, Imap::Mailbox::RoleMessageCc);
        model.appendRow(item);

        Host host;
        auto *panel = host.add(new Gui::ContactPanel);
        panel->setPanelEnabled(true);
        auto *write = child<QToolButton>(panel, QStringLiteral("contactPanelWrite"));
        auto *copy = child<QToolButton>(panel, QStringLiteral("contactPanelCopy"));
        auto *addressBook = child<QToolButton>(panel, QStringLiteral("contactPanelAddressBook"));
        QVERIFY(write && copy && addressBook);
        QVERIFY(!write->isEnabled());

        panel->setMessage(item->index());
        QCOMPARE(panel->contactCount(), 2);
        QVERIFY(panel->contactText(0).contains(QStringLiteral("ann@example.org")));
        QVERIFY(panel->contactText(1).contains(QStringLiteral("bob@example.org")));
        auto *list = child<QListWidget>(panel, QStringLiteral("contactPanelList"));
        QVERIFY(list);
        // A Reply-To that repeats the sender is what servers report when the header is missing.
        QVERIFY(!list->item(0)->text().contains(i18nc("@info contact role", "Reply-To")));
        QVERIFY(list->item(0)->text().contains(i18nc("@info contact role", "Cc")));
        QCOMPARE(list->currentRow(), 0);
        QVERIFY(write->isEnabled());
        QVERIFY(copy->isEnabled());
        QVERIFY(!addressBook->isEnabled());

        QSignalSpy compose(panel, &Gui::ContactPanel::composeRequested);
        QSignalSpy manage(panel, &Gui::ContactPanel::manageContactRequested);
        list->setCurrentRow(1);
        write->click();
        QCOMPARE(compose.count(), 1);
        const QUrl mailto = compose.constFirst().constFirst().toUrl();
        QCOMPARE(mailto.scheme(), QStringLiteral("mailto"));
        QVERIFY(mailto.toString().contains(QStringLiteral("bob@example.org")));

        panel->openCurrentInAddressBook();
        QCOMPARE(manage.count(), 0);
        panel->setAddressBookAvailable(true);
        QVERIFY(addressBook->isEnabled());
        addressBook->click();
        QCOMPARE(manage.count(), 1);
        QCOMPARE(manage.constFirst().constFirst().toUrl().scheme(), QStringLiteral("x-trojita-manage-contact"));

        // The selection survives a refresh
        item->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
        QTest::qWait(10);
        QCOMPARE(list->currentRow(), 1);

        auto *empty = messageItem(QStringLiteral("Nobody"), QString());
        empty->setData(QVariant(), Imap::Mailbox::RoleMessageFrom);
        model.appendRow(empty);
        panel->setAutoHide(true);
        panel->setMessage(empty->index());
        QCOMPARE(panel->contactCount(), 0);
        QVERIFY(!panel->hasContent());
        QVERIFY(panel->isHidden());
    }

    void relatedMessagesAreGrouped()
    {
        QStandardItemModel list;
        list.appendRow(messageItem(QStringLiteral("Plans"), QStringLiteral("bob@example.org")));
        list.appendRow(messageItem(QStringLiteral("Invoice"), QStringLiteral("ann@example.org")));
        list.appendRow(messageItem(QStringLiteral("Other"), QStringLiteral("carol@example.org")));
        auto *current = messageItem(QStringLiteral("Re: Plans"), QStringLiteral("ann@example.org"));
        list.appendRow(current);

        Host host;
        auto *panel = host.add(new Gui::RelatedMessagesPanel);
        panel->setMessageList(&list);
        panel->setMessage(current->index());
        // A turned-off panel does not scan the list
        QVERIFY(panel->conversation().isEmpty());

        panel->setPanelEnabled(true);
        QCOMPARE(panel->conversation().size(), 1);
        QCOMPARE(panel->sameSender().size(), 1);
        QCOMPARE(panel->conversation().constFirst().row(), 0);
        QCOMPARE(panel->sameSender().constFirst().row(), 1);
        auto *tree = child<QTreeWidget>(panel, QStringLiteral("relatedMessagesPanelList"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        QVERIFY(tree->topLevelItem(0)->child(0)->text(0).startsWith(QStringLiteral("Plans")));

        QSignalSpy activated(panel, &Gui::RelatedMessagesPanel::messageActivated);
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        panel->activateCurrent();
        QCOMPARE(activated.count(), 1);
        QCOMPARE(activated.constFirst().constFirst().value<QModelIndex>().row(), 1);
        // Group headers are not messages
        tree->setCurrentItem(tree->topLevelItem(0));
        panel->activateCurrent();
        QCOMPARE(activated.count(), 1);

        // New arrivals are picked up after a short delay
        list.appendRow(messageItem(QStringLiteral("Fwd: plans"), QStringLiteral("dave@example.org")));
        QTRY_COMPARE(panel->conversation().size(), 2);

        panel->setAutoHide(true);
        QVERIFY(panel->isVisible());
        panel->setMessage(list.index(2, 0));
        QVERIFY(!panel->hasContent());
        QVERIFY(panel->isHidden());
        QCOMPARE(placeholder(panel), i18nc("@info", "No related messages in this folder"));
    }

    void menuControlsEveryPanel()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("plasma-impresariorc"));
        auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);

        Host host;
        auto *details = host.add(new Gui::MessageDetailsPanel);
        auto *contacts = host.add(new Gui::ContactPanel);
        Gui::EmbeddedPanelsMenu menu(config);
        menu.addPanel(details);
        menu.addPanel(contacts);
        QCOMPARE(menu.objectName(), QStringLiteral("embeddedPanelsMenu"));
        QCOMPARE(menu.actions().size(), 2);
        QVERIFY(menu.findChild<QMenu *>(QStringLiteral("embeddedPanelMessageDetailsMenu")));
        QVERIFY(menu.findChild<QMenu *>(QStringLiteral("embeddedPanelContactMenu")));
        QCOMPARE(menu.visibleAction(details)->objectName(), QStringLiteral("embeddedPanelMessageDetails"));
        QCOMPARE(menu.placementAction(contacts, Gui::EmbeddedPanel::Placement::Footer)->objectName(),
                 QStringLiteral("embeddedPanelContactPlacementFooter"));
        QVERIFY(!menu.visibleAction(details)->isChecked());
        QVERIFY(!menu.autoHideAction(details)->isEnabled());
        QVERIFY(menu.placementAction(details, Gui::EmbeddedPanel::Placement::RightColumn)->isChecked());
        QVERIFY(details->isHidden());

        menu.visibleAction(details)->trigger();
        QVERIFY(details->isPanelEnabled());
        QVERIFY(details->isVisible());
        QVERIFY(menu.autoHideAction(details)->isEnabled());
        menu.autoHideAction(details)->trigger();
        QVERIFY(details->isHidden());
        menu.placementAction(details, Gui::EmbeddedPanel::Placement::LeftColumn)->trigger();
        QCOMPARE(details->placement(), Gui::EmbeddedPanel::Placement::LeftColumn);
        QVERIFY(!menu.placementAction(details, Gui::EmbeddedPanel::Placement::RightColumn)->isChecked());
        QCOMPARE(contacts->placement(), Gui::EmbeddedPanel::Placement::RightColumn);

        auto reread = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
        reread->reparseConfiguration();
        const KConfigGroup appearance(reread, QStringLiteral("Appearance"));
        QCOMPARE(appearance.readEntry("MessageDetailsPanelVisible", false), true);
        QCOMPARE(appearance.readEntry("MessageDetailsPanelAutoHide", false), true);
        QCOMPARE(appearance.readEntry("MessageDetailsPanelPlacement", QString()), QStringLiteral("left"));
        QVERIFY(!appearance.hasKey("ContactPanelVisible"));

        Gui::MessageDetailsPanel restored;
        Gui::EmbeddedPanelsMenu restoredMenu(reread);
        restoredMenu.addPanel(&restored);
        QVERIFY(restored.isPanelEnabled());
        QVERIFY(restored.autoHide());
        QCOMPARE(restored.placement(), Gui::EmbeddedPanel::Placement::LeftColumn);
        QVERIFY(restoredMenu.placementAction(&restored, Gui::EmbeddedPanel::Placement::LeftColumn)->isChecked());
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    EmbeddedPanelsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_EmbeddedPanels.moc"
