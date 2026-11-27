/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/ContactPanel.h"
#include "Imap/Model/ItemRoles.h"
#include "Plugins/AbookAddressbook/AbookAddressbook.h"

#include <KLocalizedString>
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
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

QStandardItem *messageItem(const QString &from, const QStringList &to)
{
    auto *item = new QStandardItem(from);
    item->setData(true, Imap::Mailbox::RoleIsFetched);
    item->setData(addresses({from}), Imap::Mailbox::RoleMessageFrom);
    item->setData(addresses(to), Imap::Mailbox::RoleMessageTo);
    return item;
}

QByteArray pngPhoto()
{
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

class FakeBook;

class FakeContactJob : public Plugins::AddressbookContactJob
{
public:
    FakeContactJob(const QString &email, FakeBook *book);

    void doStart() override;
    void doStop() override;

    void deliver(const Plugins::ContactDetails &details)
    {
        emit contactDetailsAvailable(details);
        finished();
    }

    QString m_email;
    FakeBook *m_book;
};

/** An address book which answers from a table, optionally holding the answers back */
class FakeBook : public Plugins::AddressbookPlugin
{
public:
    explicit FakeBook(Features features)
        : AddressbookPlugin(nullptr)
        , m_features(features)
    {
    }

    Features features() const override
    {
        return m_features;
    }
    Plugins::AddressbookCompletionJob *requestCompletion(const QString &, const QStringList &, int) override
    {
        return nullptr;
    }
    Plugins::AddressbookNamesJob *requestPrettyNamesForAddress(const QString &) override
    {
        return nullptr;
    }
    void openAddressbookWindow() override
    {
    }
    void openContactWindow(const QString &, const QString &) override
    {
    }
    Plugins::AddressbookContactJob *requestContactDetails(const QString &email) override
    {
        ++requests;
        return new FakeContactJob(email, this);
    }

    void changeContacts()
    {
        emit contactsChanged();
    }

    /** Answer the held lookups, all with @arg details */
    void release(const Plugins::ContactDetails &details)
    {
        const auto jobs = held;
        held.clear();
        for (const auto &job : jobs) {
            if (job)
                job->deliver(details);
        }
    }

    Features m_features;
    QHash<QString, Plugins::ContactDetails> contacts;
    bool hold = false;
    QList<QPointer<FakeContactJob>> held;
    int requests = 0;
    int stopped = 0;
};

FakeContactJob::FakeContactJob(const QString &email, FakeBook *book)
    : AddressbookContactJob(book)
    , m_email(email)
    , m_book(book)
{
}

void FakeContactJob::doStart()
{
    if (m_book->hold) {
        m_book->held << this;
        return;
    }
    Plugins::ContactDetails details = m_book->contacts.value(m_email.toLower());
    details.email = m_email;
    deliver(details);
}

void FakeContactJob::doStop()
{
    ++m_book->stopped;
    emit error(AddressbookJob::Stopped);
    finished();
}

constexpr auto allFeatures = Plugins::AddressbookPlugin::Features(
    Plugins::AddressbookPlugin::FeatureContactWindow | Plugins::AddressbookPlugin::FeatureAddContact
    | Plugins::AddressbookPlugin::FeatureEditContact | Plugins::AddressbookPlugin::FeatureContactDetails);

Plugins::ContactDetails known(const QString &name, const QString &organization = QString(), const QByteArray &photo = QByteArray())
{
    Plugins::ContactDetails details;
    details.known = true;
    details.name = name;
    details.organization = organization;
    details.photo = photo;
    return details;
}

struct Host {
    QWidget window;
    Gui::ContactPanel *panel;
    QListWidget *list;
    QToolButton *addressBook;
    Host()
        : panel(new Gui::ContactPanel)
    {
        auto *layout = new QVBoxLayout(&window);
        layout->addWidget(panel);
        panel->setPanelEnabled(true);
        window.resize(500, 400);
        window.show();
        list = panel->findChild<QListWidget *>(QStringLiteral("contactPanelList"));
        addressBook = panel->findChild<QToolButton *>(QStringLiteral("contactPanelAddressBook"));
    }
    QString description(int row) const
    {
        return list->item(row)->data(Qt::AccessibleDescriptionRole).toString();
    }
};

}

class ContactPanelAddressBookTest : public QObject
{
    Q_OBJECT
private slots:
    void knownAndUnknownContacts()
    {
        FakeBook book(allFeatures);
        const QByteArray photo = pngPhoto();
        book.contacts.insert(QStringLiteral("ann@example.org"), known(QStringLiteral("Ann Example"), QStringLiteral("Example Org"), photo));
        QStandardItemModel model;
        auto *message = messageItem(QStringLiteral("ann@example.org"), {QStringLiteral("bob@example.org")});
        model.appendRow(message);

        Host host;
        QVERIFY(host.list && host.addressBook);
        host.panel->setAddressBook(&book);
        host.panel->setMessage(message->index());
        QCOMPARE(host.panel->contactCount(), 2);
        QTRY_VERIFY(!host.panel->lookupsPending());
        QCOMPARE(book.requests, 2);

        // The address book's name and organization replace the header name
        const QString ann = host.list->item(0)->text();
        QVERIFY2(ann.contains(QStringLiteral("Ann Example")) && ann.contains(QStringLiteral("Example Org")), qPrintable(ann));
        QVERIFY(ann.contains(QStringLiteral("ann@example.org")));
        QVERIFY(host.panel->contactIsKnown(0));
        QVERIFY(host.panel->contactText(0).contains(QStringLiteral("Ann Example")));
        QCOMPARE(host.list->item(0)->data(Qt::AccessibleTextRole).toString(), ann);
        QCOMPARE(host.description(0),
                 i18nc("@info accessible description of a contact", "In address book, with photo from address book"));
        QVERIFY(!host.list->item(0)->icon().isNull());

        QVERIFY(!host.panel->contactIsKnown(1));
        QVERIFY(host.list->item(1)->text().contains(QStringLiteral("BOB")));
        QCOMPARE(host.description(1), i18nc("@info accessible description of a contact", "Not in address book"));

        // Known contacts are opened, unknown ones added
        QSignalSpy manage(host.panel, &Gui::ContactPanel::manageContactRequested);
        QSignalSpy add(host.panel, &Gui::ContactPanel::addContactRequested);
        host.list->setCurrentRow(0);
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Open in Address Book"));
        host.addressBook->click();
        QCOMPARE(manage.count(), 1);
        host.panel->addCurrentToAddressBook();
        QCOMPARE(add.count(), 0);

        host.list->setCurrentRow(1);
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Add to Address Book"));
        QVERIFY(host.addressBook->isEnabled());
        host.addressBook->click();
        QCOMPARE(add.count(), 1);
        QCOMPARE(manage.count(), 1);
        const QUrl url = add.constFirst().constFirst().toUrl();
        QCOMPARE(url.scheme(), QStringLiteral("x-trojita-manage-contact"));
        QVERIFY(url.toString().contains(QStringLiteral("bob@example.org")));
        host.panel->openCurrentInAddressBook();
        QCOMPARE(manage.count(), 2);

        // A refresh of the same message reuses the results
        message->setData(true, Imap::Mailbox::RoleMessageIsMarkedRead);
        QTest::qWait(10);
        QCOMPARE(book.requests, 2);
        QVERIFY(host.panel->contactIsKnown(0));
        QCOMPARE(host.list->currentRow(), 1);

        // Changed contacts are looked up again, keeping the old details meanwhile
        book.contacts.insert(QStringLiteral("bob@example.org"), known(QStringLiteral("Bob Builder")));
        book.changeContacts();
        QVERIFY(host.panel->contactIsKnown(0));
        QTRY_VERIFY(host.panel->contactIsKnown(1));
        QCOMPARE(book.requests, 4);
        QVERIFY(host.list->item(1)->text().contains(QStringLiteral("Bob Builder")));
        QCOMPARE(host.description(1), i18nc("@info accessible description of a contact", "In address book"));
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Open in Address Book"));
    }

    void addingNeedsTheFeature()
    {
        FakeBook book(Plugins::AddressbookPlugin::FeatureContactWindow | Plugins::AddressbookPlugin::FeatureContactDetails);
        QStandardItemModel model;
        auto *message = messageItem(QStringLiteral("bob@example.org"), {});
        model.appendRow(message);

        Host host;
        host.panel->setAddressBook(&book);
        host.panel->setMessage(message->index());
        QTRY_VERIFY(!host.panel->lookupsPending());
        QCOMPARE(host.description(0), i18nc("@info accessible description of a contact", "Not in address book"));
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Open in Address Book"));
        QSignalSpy add(host.panel, &Gui::ContactPanel::addContactRequested);
        host.panel->addCurrentToAddressBook();
        QCOMPARE(add.count(), 0);
    }

    void lookupsAreCancelledWithTheMessage()
    {
        FakeBook book(allFeatures);
        book.hold = true;
        QStandardItemModel model;
        auto *first = messageItem(QStringLiteral("ann@example.org"), {QStringLiteral("bob@example.org")});
        auto *second = messageItem(QStringLiteral("ann@example.org"), {});
        model.appendRow(first);
        model.appendRow(second);

        Host host;
        host.panel->setAddressBook(&book);
        host.panel->setMessage(first->index());
        QTRY_COMPARE(book.held.size(), 2);
        QVERIFY(host.panel->lookupsPending());

        host.panel->setMessage(second->index());
        QCOMPARE(book.requests, 3);
        // Answers for the previous message are ignored, even for the same address
        book.release(known(QStringLiteral("Stale Name")));
        QTRY_COMPARE(book.stopped, 2);
        QVERIFY(!host.panel->contactIsKnown(0));
        QVERIFY(!host.list->item(0)->text().contains(QStringLiteral("Stale Name")));
        QVERIFY(host.panel->lookupsPending());

        book.release(known(QStringLiteral("Ann Example")));
        QTRY_VERIFY(host.panel->contactIsKnown(0));
        QVERIFY(!host.panel->lookupsPending());
        QVERIFY(host.list->item(0)->text().contains(QStringLiteral("Ann Example")));

        // Removing the address book drops its details
        host.panel->setAddressBook(nullptr);
        QVERIFY(!host.panel->contactIsKnown(0));
        QVERIFY(host.description(0).isEmpty());
        QVERIFY(!host.addressBook->isEnabled());
    }

    void withoutDetailsTheHeadersAreShown()
    {
        FakeBook book(Plugins::AddressbookPlugin::FeatureContactWindow | Plugins::AddressbookPlugin::FeatureAddContact);
        QStandardItemModel model;
        auto *message = messageItem(QStringLiteral("ann@example.org"), {});
        model.appendRow(message);

        Host host;
        host.panel->setAddressBook(&book);
        host.panel->setMessage(message->index());
        QTest::qWait(10);
        QCOMPARE(book.requests, 0);
        QVERIFY(!host.panel->lookupsPending());
        QVERIFY(host.description(0).isEmpty());
        QVERIFY(host.list->item(0)->text().contains(QStringLiteral("ANN")));
        QVERIFY(host.addressBook->isEnabled());
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Open in Address Book"));
    }

    void abookDescribesContacts()
    {
        const QString abook = QDir::homePath() + QStringLiteral("/.abook");
        QVERIFY(QDir().mkpath(abook));
        const QByteArray photo = pngPhoto();
        QFile annPhoto(abook + QStringLiteral("/ann.png"));
        QVERIFY(annPhoto.open(QIODevice::WriteOnly));
        annPhoto.write(photo);
        annPhoto.close();
        const QString elsewhere = QDir::homePath() + QStringLiteral("/bob.png");
        QFile bobPhoto(elsewhere);
        QVERIFY(bobPhoto.open(QIODevice::WriteOnly));
        bobPhoto.write(photo);
        bobPhoto.close();
        QFile big(abook + QStringLiteral("/big.png"));
        QVERIFY(big.open(QIODevice::WriteOnly));
        QVERIFY(big.resize(AbookAddressbook::maximumPhotoSize + 1));
        big.close();

        QFile db(abook + QStringLiteral("/addressbook"));
        QVERIFY(db.open(QIODevice::WriteOnly));
        db.write("[format]\nprogram=abook\nversion=0.6.1\n\n"
                 "[0]\nname=Ann Example\nemail=ann@example.org,ann@work.example\nphoto=ann.png\n\n"
                 "[1]\nname=Bob Builder\nemail=bob@example.org\nphoto="
                 + elsewhere.toUtf8()
                 + "\n\n"
                   "[2]\nname=Carol\nemail=carol@example.org\nphoto=https://example.org/carol.png\n\n"
                   "[3]\nname=Dave\nemail=dave@example.org\nphoto=big.png\n");
        db.close();

        AbookAddressbook book(nullptr);
        QVERIFY(book.features().testFlag(Plugins::AddressbookPlugin::FeatureContactDetails));

        auto details = book.contactDetails(QStringLiteral("ANN@Example.org"));
        QVERIFY(details.known);
        QCOMPARE(details.name, QStringLiteral("Ann Example"));
        QCOMPARE(details.photo, photo);
        QVERIFY(details.organization.isEmpty());
        QCOMPARE(book.contactDetails(QStringLiteral("ann@work.example")).name, QStringLiteral("Ann Example"));

        details = book.contactDetails(QStringLiteral("bob@example.org"));
        QCOMPARE(details.name, QStringLiteral("Bob Builder"));
        QCOMPARE(details.photo, photo);

        details = book.contactDetails(QStringLiteral("carol@example.org"));
        QVERIFY(details.known);
        QVERIFY(details.photo.isEmpty());
        details = book.contactDetails(QStringLiteral("dave@example.org"));
        QVERIFY(details.known);
        QVERIFY(details.photo.isEmpty());

        details = book.contactDetails(QStringLiteral("nobody@example.org"));
        QVERIFY(!details.known);
        QCOMPARE(details.email, QStringLiteral("nobody@example.org"));
        QVERIFY(details.name.isEmpty());

        // The plugin feeds the panel
        QStandardItemModel model;
        auto *message = messageItem(QStringLiteral("ann@work.example"), {QStringLiteral("nobody@example.org")});
        model.appendRow(message);
        Host host;
        host.panel->setAddressBook(&book);
        host.panel->setMessage(message->index());
        QTRY_VERIFY(host.panel->contactIsKnown(0));
        QTRY_VERIFY(!host.panel->lookupsPending());
        QVERIFY(host.list->item(0)->text().contains(QStringLiteral("Ann Example")));
        QVERIFY(!host.panel->contactIsKnown(1));
        host.list->setCurrentRow(1);
        QCOMPARE(host.addressBook->accessibleName(), i18nc("@action:button", "Add to Address Book"));

        QSignalSpy changed(&book, &Plugins::AddressbookPlugin::contactsChanged);
        book.saveContacts();
        QCOMPARE(changed.count(), 1);
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    // The abook plugin keeps its data below the home folder
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toLocal8Bit());
    QApplication app(argc, argv);
    ContactPanelAddressBookTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_ContactPanelAddressBook.moc"
