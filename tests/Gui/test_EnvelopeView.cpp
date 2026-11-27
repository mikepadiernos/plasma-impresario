/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <KLocalizedString>

#include "Gui/EnvelopeView.h"
#include "Imap/Model/ItemRoles.h"

using ListAction = UiUtils::MailingLists::Action;

class EnvelopeViewTest : public QObject
{
    Q_OBJECT

private:
    static QStandardItem *message(const QVariantMap &listHeaders, const QString &listPost = QString())
    {
        auto *item = new QStandardItem(QStringLiteral("message"));
        item->setData(1u, Imap::Mailbox::RoleMessageUid);
        item->setData(true, Imap::Mailbox::RoleIsFetched);
        item->setData(listHeaders, Imap::Mailbox::RoleMessageHeaderList);
        QVariantList post;
        if (!listPost.isEmpty())
            post << QUrl(listPost);
        item->setData(post, Imap::Mailbox::RoleMessageHeaderListPost);
        item->setData(false, Imap::Mailbox::RoleMessageHeaderListPostNo);
        return item;
    }

    static void show(Gui::EnvelopeView &view, const QModelIndex &index)
    {
        view.setMessage(index);
        // The rows of the previous message go away later
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    static int position(const Gui::EnvelopeView &view, ListAction action)
    {
        const auto actions = view.listActions();
        for (qsizetype i = 0; i < actions.size(); ++i) {
            if (actions.at(i).action == action)
                return int(i);
        }
        return -1;
    }

    static QMessageBox *confirmation(Gui::EnvelopeView &view)
    {
        return view.findChild<QMessageBox *>(QStringLiteral("listActionConfirmation"));
    }

    static void closeConfirmation()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

private slots:
    void namesTheListAndAsksBeforeActions()
    {
        QStandardItemModel model;
        model.appendRow(message({{QStringLiteral("list-id"), QStringLiteral("Release Team <team.lists.example.org>")},
                                 {QStringLiteral("list-unsubscribe"),
                                  QStringLiteral("<mailto:team-leave@lists.example.org>, <https://lists.example.org/leave>")},
                                 {QStringLiteral("list-archive"), QStringLiteral("<https://lists.example.org/archive>")},
                                 {QStringLiteral("list-help"), QStringLiteral("<javascript:alert(1)>")}},
                                QStringLiteral("mailto:team@lists.example.org")));
        Gui::EnvelopeView view(nullptr, nullptr);
        QList<QUrl> opened;
        view.setUrlOpener([&opened](const QUrl &url) { opened << url; });
        show(view, model.index(0, 0));

        auto *row = view.findChild<QLabel *>(QStringLiteral("envelopeMailingList"));
        QVERIFY(row);
        QVERIFY(row->text().contains(QStringLiteral("Release Team (team.lists.example.org)")));
        QVERIFY(!row->openExternalLinks());
        // Only links that a mail client or a browser can follow are offered
        QCOMPARE(view.listActions().size(), 3);
        QVERIFY(position(view, ListAction::Help) < 0);
        const int post = position(view, ListAction::Post);
        const int unsubscribe = position(view, ListAction::Unsubscribe);
        const int archive = position(view, ListAction::Archive);
        QVERIFY(post >= 0 && unsubscribe >= 0 && archive >= 0);
        QCOMPARE(view.listActions().at(unsubscribe).url, QUrl(QStringLiteral("mailto:team-leave@lists.example.org")));

        // Clicking a link only asks; cancelling does nothing
        emit row->linkActivated(QStringLiteral("list-action:%1").arg(unsubscribe));
        QMessageBox *box = confirmation(view);
        QVERIFY(box);
        QVERIFY(box->text().contains(QStringLiteral("Unsubscribe from Release Team?")));
        QVERIFY(box->text().contains(QStringLiteral("team-leave@lists.example.org")));
        QCOMPARE(box->defaultButton(), box->button(QMessageBox::Cancel));
        QVERIFY(opened.isEmpty());
        box->button(QMessageBox::Cancel)->click();
        QVERIFY(opened.isEmpty());
        closeConfirmation();
        QVERIFY(!confirmation(view));

        // Hovering shows the whole target, with an internationalized host in its punycode form
        emit row->linkHovered(QStringLiteral("list-action:%1").arg(archive));
        QVERIFY2(row->toolTip().remove(QChar(0x200B)).contains(QStringLiteral("https://lists.example.org/archive")),
                 qPrintable(row->toolTip()));
        emit row->linkHovered(QString());
        QVERIFY(row->toolTip().isEmpty());

        // Confirming follows the link
        emit row->linkActivated(QStringLiteral("list-action:%1").arg(archive));
        box = confirmation(view);
        QVERIFY(box);
        QVERIFY(box->text().contains(QStringLiteral("https://lists.example.org/archive")));
        auto *follow = box->findChild<QPushButton *>(QStringLiteral("followListAction"));
        QVERIFY(follow);
        follow->click();
        QCOMPARE(opened, QList<QUrl>{QUrl(QStringLiteral("https://lists.example.org/archive"))});
        closeConfirmation();

        // Links which are not list actions, or out of range, do nothing
        emit row->linkActivated(QStringLiteral("list-action:9"));
        emit row->linkActivated(QStringLiteral("https://example.org/"));
        QVERIFY(!confirmation(view));
        view.requestListAction(-1);
        QVERIFY(!confirmation(view));
        QCOMPARE(opened.size(), 1);
    }

    void handlesMessagesWithoutAList()
    {
        QStandardItemModel model;
        model.appendRow(message({}));
        model.appendRow(message({{QStringLiteral("list-unsubscribe"), QStringLiteral("<https://news.example.org/leave>")}}));
        auto *unfetched = message({{QStringLiteral("list-id"), QStringLiteral("<team.lists.example.org>")}});
        unfetched->setData(false, Imap::Mailbox::RoleIsFetched);
        model.appendRow(unfetched);
        Gui::EnvelopeView view(nullptr, nullptr);
        QList<QUrl> opened;
        view.setUrlOpener([&opened](const QUrl &url) { opened << url; });

        show(view, model.index(0, 0));
        QVERIFY(!view.findChild<QLabel *>(QStringLiteral("envelopeMailingList")));
        QVERIFY(view.listActions().isEmpty());

        // A newsletter may offer unsubscribing without naming its list
        show(view, model.index(1, 0));
        QVERIFY(view.findChild<QLabel *>(QStringLiteral("envelopeMailingList")));
        QCOMPARE(view.listActions().size(), 1);
        view.requestListAction(0);
        QMessageBox *box = confirmation(view);
        QVERIFY(box);
        QVERIFY(box->text().contains(QStringLiteral("Unsubscribe from this mailing list?")));
        box->button(QMessageBox::Cancel)->click();
        closeConfirmation();

        // Until its headers arrive, a message shows no list
        show(view, model.index(2, 0));
        QVERIFY(!view.findChild<QLabel *>(QStringLiteral("envelopeMailingList")));
        QVERIFY(view.listActions().isEmpty());
        show(view, QModelIndex());
        QVERIFY(view.listActions().isEmpty());
        QVERIFY(opened.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_CACHE_HOME", home.filePath(QStringLiteral("cache")).toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    EnvelopeViewTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_EnvelopeView.moc"
