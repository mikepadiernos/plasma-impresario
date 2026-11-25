/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/AttachmentPanel.h"
#include "Gui/EmbeddedPanelsMenu.h"
#include "Imap/Model/ItemRoles.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QStandardItem *part(const char *mimeType, const QString &fileName = QString(), const char *disposition = "")
{
    auto *item = new QStandardItem(QString::fromLatin1(mimeType));
    item->setData(QByteArray(mimeType), Imap::Mailbox::RolePartMimeType);
    item->setData(fileName, Imap::Mailbox::RolePartFileName);
    item->setData(QByteArray(disposition), Imap::Mailbox::RolePartBodyDisposition);
    item->setData(quint64(2048), Imap::Mailbox::RolePartOctets);
    return item;
}

/** Shaped like Cryptography::MessageModel: index(0, 0) is the message and its child is the body */
QStandardItem *message(QStandardItemModel &model, QStandardItem *body)
{
    auto *root = new QStandardItem(QStringLiteral("message"));
    root->appendRow(body);
    model.appendRow(root);
    return body;
}

QStandardItem *mixedWithTwoFiles(QStandardItemModel &model)
{
    auto *mixed = part("multipart/mixed");
    mixed->appendRow(part("text/plain"));
    mixed->appendRow(part("application/pdf", QStringLiteral("report.pdf"), "attachment"));
    mixed->appendRow(part("image/png", QStringLiteral("photo.png"), "inline"));
    return message(model, mixed);
}

struct Host {
    QWidget window;
    Gui::AttachmentPanel *panel;
    Host()
        : panel(new Gui::AttachmentPanel(&window))
    {
        auto *layout = new QVBoxLayout(&window);
        layout->addWidget(new QWidget(&window), 1);
        layout->addWidget(panel);
        window.resize(400, 300);
        window.show();
    }
};

template<typename T>
T *child(Gui::AttachmentPanel *panel, const char *name)
{
    return panel->findChild<T *>(QString::fromLatin1(name));
}

}

class AttachmentPanelTest : public QObject
{
    Q_OBJECT
private slots:
    void shownWithPlaceholderByDefault()
    {
        Host host;
        QVERIFY(host.panel->isPanelEnabled());
        QVERIFY(!host.panel->autoHide());
        QVERIFY(host.panel->isVisible());
        QCOMPARE(host.panel->attachmentCount(), 0);
        auto *stack = qobject_cast<QStackedWidget *>(child<QLabel>(host.panel, "attachmentPanelPlaceholder")->parentWidget());
        QVERIFY(stack);
        QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("attachmentPanelPlaceholder"));
        QCOMPARE(child<QLabel>(host.panel, "attachmentPanelPlaceholder")->text(), i18nc("@info", "No message selected"));
        QCOMPARE(child<QLabel>(host.panel, "attachmentPanelTitle")->text(), i18nc("@title embedded panel", "Attachments"));
        QVERIFY(!child<QToolButton>(host.panel, "attachmentPanelOpen")->isEnabled());
        QVERIFY(!child<QToolButton>(host.panel, "attachmentPanelSave")->isEnabled());

        QStandardItemModel plain;
        message(plain, part("text/plain"));
        host.panel->setMessageModel(&plain);
        QVERIFY(host.panel->isVisible());
        QCOMPARE(child<QLabel>(host.panel, "attachmentPanelPlaceholder")->text(), i18nc("@info", "This message has no attachments"));
    }

    void listsAttachmentsAndFollowsLateParts()
    {
        Host host;
        QStandardItemModel model;
        auto *mixed = mixedWithTwoFiles(model);
        host.panel->setMessageModel(&model);
        QCOMPARE(host.panel->attachmentCount(), 2);
        auto *list = child<QListWidget>(host.panel, "attachmentPanelList");
        QVERIFY(list);
        QVERIFY(list->isVisible());
        QCOMPARE(list->count(), 2);
        QVERIFY(list->item(0)->text().startsWith(QStringLiteral("report.pdf")));
        QVERIFY(list->item(1)->text().startsWith(QStringLiteral("photo.png")));
        QCOMPARE(list->currentRow(), 0);
        QCOMPARE(child<QLabel>(host.panel, "attachmentPanelTitle")->text(),
                 i18ncp("@title embedded panel", "%1 Attachment", "%1 Attachments", 2));

        list->setCurrentRow(1);
        mixed->appendRow(part("application/zip", QStringLiteral("archive.zip"), "attachment"));
        QTRY_COMPARE(host.panel->attachmentCount(), 3);
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->currentRow(), 1);
        QVERIFY(list->item(2)->text().startsWith(QStringLiteral("archive.zip")));
    }

    void autoHideFollowsTheCurrentMessage()
    {
        Host host;
        QSignalSpy relevance(host.panel, &Gui::AttachmentPanel::relevanceChanged);
        host.panel->setAutoHide(true);
        QVERIFY(host.panel->isHidden());
        QCOMPARE(relevance.size(), 1);
        QCOMPARE(relevance.takeFirst().at(0).toBool(), false);

        QStandardItemModel plain;
        message(plain, part("text/plain"));
        host.panel->setMessageModel(&plain);
        QVERIFY(host.panel->isHidden());
        QVERIFY(relevance.isEmpty());

        QStandardItemModel withFiles;
        mixedWithTwoFiles(withFiles);
        host.panel->setMessageModel(&withFiles);
        QVERIFY(host.panel->isVisible());
        QCOMPARE(relevance.size(), 1);
        QCOMPARE(relevance.takeFirst().at(0).toBool(), true);

        host.panel->setMessageModel(nullptr);
        QVERIFY(host.panel->isHidden());
        host.panel->setAutoHide(false);
        QVERIFY(host.panel->isVisible());
    }

    void disablingHidesThePanel()
    {
        Host host;
        QStandardItemModel model;
        mixedWithTwoFiles(model);
        host.panel->setMessageModel(&model);
        host.panel->setPanelEnabled(false);
        QVERIFY(host.panel->isHidden());
        QVERIFY(!host.panel->shouldBeShown());
        host.panel->setPanelEnabled(true);
        QVERIFY(host.panel->isVisible());
    }

    void clearsWhenTheModelIsDestroyed()
    {
        Host host;
        auto *model = new QStandardItemModel;
        mixedWithTwoFiles(*model);
        host.panel->setMessageModel(model);
        QCOMPARE(host.panel->attachmentCount(), 2);
        delete model;
        QCOMPARE(host.panel->attachmentCount(), 0);
        QCOMPARE(child<QListWidget>(host.panel, "attachmentPanelList")->count(), 0);
        QCOMPARE(child<QLabel>(host.panel, "attachmentPanelPlaceholder")->text(), i18nc("@info", "No message selected"));
    }

    void menuPersistsPreferences()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("plasma-impresariorc"));
        auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);

        Host host;
        Gui::EmbeddedPanelsMenu menu(config);
        menu.addPanel(host.panel);
        QAction *visible = menu.visibleAction(host.panel);
        QAction *autoHide = menu.autoHideAction(host.panel);
        QVERIFY(visible->isChecked());
        QVERIFY(!autoHide->isChecked());
        QVERIFY(autoHide->isEnabled());
        QCOMPARE(visible->objectName(), QStringLiteral("embeddedPanelAttachments"));
        QCOMPARE(autoHide->objectName(), QStringLiteral("embeddedPanelAttachmentsAutoHide"));
        QVERIFY(menu.placementAction(host.panel, Gui::EmbeddedPanel::Placement::BelowReader)->isChecked());
        QCOMPARE(host.panel->placement(), Gui::EmbeddedPanel::Placement::BelowReader);
        QVERIFY(host.panel->isVisible());

        autoHide->trigger();
        QVERIFY(host.panel->autoHide());
        QVERIFY(host.panel->isHidden());
        visible->trigger();
        QVERIFY(!host.panel->isPanelEnabled());
        QVERIFY(!autoHide->isEnabled());
        menu.placementAction(host.panel, Gui::EmbeddedPanel::Placement::Footer)->trigger();
        QCOMPARE(host.panel->placement(), Gui::EmbeddedPanel::Placement::Footer);

        auto reread = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
        reread->reparseConfiguration();
        const KConfigGroup appearance(reread, QStringLiteral("Appearance"));
        QCOMPARE(appearance.readEntry("AttachmentPanelVisible", true), false);
        QCOMPARE(appearance.readEntry("AttachmentPanelAutoHide", false), true);
        QCOMPARE(appearance.readEntry("AttachmentPanelPlacement", QString()), QStringLiteral("footer"));

        Host restored;
        Gui::EmbeddedPanelsMenu restoredMenu(reread);
        restoredMenu.addPanel(restored.panel);
        QVERIFY(!restoredMenu.visibleAction(restored.panel)->isChecked());
        QVERIFY(restoredMenu.autoHideAction(restored.panel)->isChecked());
        QVERIFY(!restoredMenu.autoHideAction(restored.panel)->isEnabled());
        QVERIFY(restoredMenu.placementAction(restored.panel, Gui::EmbeddedPanel::Placement::Footer)->isChecked());
        QVERIFY(!restored.panel->isPanelEnabled());
        QVERIFY(restored.panel->autoHide());
        QCOMPARE(restored.panel->placement(), Gui::EmbeddedPanel::Placement::Footer);
        QVERIFY(restored.panel->isHidden());
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    AttachmentPanelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AttachmentPanel.moc"
