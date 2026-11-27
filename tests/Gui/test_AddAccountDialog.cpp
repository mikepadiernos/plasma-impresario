/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

#include <KLocalizedString>

#include "Gui/AddAccountDialog.h"
#include "OAuth/OAuth.h"
#include "Utils/FakeDiscoveryNetwork.h"

using Gui::AddAccountDialog;
using Tests::FakeDiscoveryNetwork;
using namespace UiUtils::ServerDiscovery;

namespace {

struct Fixture {
    std::shared_ptr<FakeDiscoveryNetwork::State> state = std::make_shared<FakeDiscoveryNetwork::State>();
    std::unique_ptr<AddAccountDialog> dialog;

    Fixture()
        : dialog(std::make_unique<AddAccountDialog>(nullptr, std::make_unique<FakeDiscoveryNetwork>(state)))
    {
        dialog->show();
    }

    template<typename T> T *child(const char *name) const
    {
        T *widget = dialog->findChild<T *>(QString::fromLatin1(name));
        if (!widget)
            qFatal("missing child %s", name);
        return widget;
    }

    QLineEdit *address() const { return child<QLineEdit>("emailAddress"); }
    QPushButton *find() const { return child<QPushButton>("findSettings"); }
    QPushButton *stop() const { return child<QPushButton>("stopFinding"); }
    QGroupBox *servers() const { return child<QGroupBox>("servers"); }
    QCheckBox *oauth() const { return child<QCheckBox>("useOAuth"); }
    QCheckBox *confirm() const { return child<QCheckBox>("confirmServers"); }
    QString status() const { return child<QLabel>("discoveryStatus")->text(); }
    QPushButton *ok() const { return dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok); }

    /** Look @arg email up and wait until the lookup ends */
    bool lookUp(const QString &email)
    {
        address()->setText(email);
        if (!find()->isEnabled())
            return false;
        find()->click();
        return QTest::qWaitFor([this]() { return find()->isVisible(); }, 2000);
    }
};

QByteArray autoconfig(const char *imapHost)
{
    return QByteArray("<clientConfig version=\"1.1\"><emailProvider id=\"x\"><displayName>Hosting</displayName>"
                      "<incomingServer type=\"imap\"><hostname>")
        + imapHost
        + "</hostname><port>993</port><socketType>SSL</socketType><username>%EMAILADDRESS%</username>"
          "</incomingServer></emailProvider></clientConfig>";
}

}

class AddAccountDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void accountCanBeAddedWithoutServers()
    {
        Fixture f;
        QVERIFY(!f.ok()->isEnabled());
        QVERIFY(!f.find()->isEnabled());
        f.child<QLineEdit>("accountName")->setText(QStringLiteral("Work"));
        QVERIFY(f.ok()->isEnabled());
        QCOMPARE(f.dialog->accountName(), QStringLiteral("Work"));

        // A mistyped address blocks adding and looking up
        f.address()->setText(QStringLiteral("jo@"));
        QVERIFY(!f.ok()->isEnabled());
        QVERIFY(!f.find()->isEnabled());

        // The account is named after the address unless another name was typed
        f.child<QLineEdit>("accountName")->clear();
        f.address()->setText(QStringLiteral("jo@example.org"));
        QVERIFY(f.ok()->isEnabled());
        QVERIFY(f.find()->isEnabled());
        QCOMPARE(f.dialog->accountName(), QStringLiteral("jo@example.org"));
        QVERIFY(!f.dialog->result().isValid());
        QVERIFY(!f.dialog->useOAuth());
    }

    void nothingFoundStillAllowsAdding()
    {
        Fixture f;
        QVERIFY(f.lookUp(QStringLiteral("jo@example.org")));
        QVERIFY(!f.dialog->result().isValid());
        QVERIFY(f.servers()->isHidden());
        QVERIFY(f.status().contains(QStringLiteral("example.org")));
        QVERIFY(f.ok()->isEnabled());
    }

    void showsWhatWasFoundAndWhere()
    {
        Fixture f;
        f.state->pages.insert(QStringLiteral("autoconfig.example.org/mail/config-v1.1.xml"), autoconfig("imap.example.org"));
        QVERIFY(f.lookUp(QStringLiteral("jo@example.org")));
        QVERIFY(f.dialog->result().isValid());
        QVERIFY(f.servers()->isVisible());
        QCOMPARE(f.child<QLabel>("provider")->text(), QStringLiteral("Hosting"));
        QVERIFY(f.child<QLabel>("imapServer")->text().contains(QStringLiteral("imap.example.org")));
        QVERIFY(f.child<QLabel>("imapServer")->text().contains(QStringLiteral("993")));
        QVERIFY(!f.child<QLabel>("smtpServer")->text().contains(QStringLiteral("example.org")));
        QVERIFY(f.child<QLabel>("settingsSource")->text().contains(QStringLiteral("example.org")));
        QVERIFY(f.oauth()->isHidden());
        QVERIFY(f.confirm()->isHidden());
        QVERIFY(f.ok()->isEnabled());
    }

    void unverifiedServersNeedConfirmation()
    {
        Fixture f;
        f.state->mx.insert(QStringLiteral("example.org"), {QStringLiteral("mx.hosting.test")});
        f.state->pages.insert(QStringLiteral("autoconfig.thunderbird.net/v1.1/hosting.test"), autoconfig("imap.hosting.test"));
        QVERIFY(f.lookUp(QStringLiteral("jo@example.org")));
        QVERIFY(f.dialog->result().needsConfirmation());
        QVERIFY(f.child<QLabel>("unconfirmedWarning")->isVisible());
        QVERIFY(f.confirm()->isVisible());
        QVERIFY(f.confirm()->text().contains(QStringLiteral("imap.hosting.test")));
        QVERIFY(!f.ok()->isEnabled());
        f.confirm()->setChecked(true);
        QVERIFY(f.ok()->isEnabled());
    }

    void providersWithoutPasswordsRequireOAuth()
    {
        Fixture f;
        QVERIFY(f.lookUp(QStringLiteral("jo@outlook.com")));
        QVERIFY(f.state->log.isEmpty());
        QVERIFY(f.oauth()->isVisible());
        QVERIFY(f.oauth()->isChecked());
        QVERIFY(!f.oauth()->isEnabled());
        QVERIFY(f.dialog->useOAuth());
        const QString guidance = f.child<QLabel>("guidance")->text();
        QVERIFY(guidance.contains(QStringLiteral("OAuth")));
        QVERIFY(guidance.contains(QStringLiteral("href=\"https://support.microsoft.com/")));
        QCOMPARE(f.child<QLabel>("oauthNote")->isVisible(), OAuth::provider(QStringLiteral("microsoft")).defaultClientId.isEmpty());
    }

    void oauthIsOptionalWhereAppPasswordsWork()
    {
        Fixture f;
        QVERIFY(f.lookUp(QStringLiteral("jo@gmail.com")));
        QVERIFY(f.oauth()->isVisible());
        QVERIFY(f.oauth()->isEnabled());
        const bool hasClient = !OAuth::provider(QStringLiteral("google")).defaultClientId.isEmpty();
        // OAuth is preselected only when this build can sign in with it
        QCOMPARE(f.oauth()->isChecked(), hasClient);
        QCOMPARE(f.child<QLabel>("oauthNote")->isVisible(), !hasClient);
        f.oauth()->setChecked(false);
        QVERIFY(!f.dialog->useOAuth());
        f.oauth()->setChecked(true);
        QVERIFY(f.dialog->useOAuth());
        QVERIFY(f.child<QLabel>("guidance")->text().contains(QStringLiteral("app password")));
    }

    void editingTheAddressForgetsTheResult()
    {
        Fixture f;
        QVERIFY(f.lookUp(QStringLiteral("jo@gmail.com")));
        QVERIFY(f.dialog->result().isValid());
        f.address()->setText(QStringLiteral("jo@gmail.co"));
        QVERIFY(!f.dialog->result().isValid());
        QVERIFY(f.servers()->isHidden());
    }

    void stopEndsTheLookup()
    {
        Fixture f;
        f.state->hold = true;
        f.address()->setText(QStringLiteral("jo@example.org"));
        f.find()->click();
        QVERIFY(f.stop()->isVisible());
        QVERIFY(f.find()->isHidden());
        QVERIFY(!f.ok()->isEnabled());
        QTRY_VERIFY(!f.state->held.isEmpty());
        QVERIFY(!f.status().isEmpty());
        f.stop()->click();
        QVERIFY(f.find()->isVisible());
        QVERIFY(f.ok()->isEnabled());
        f.state->release();
        QTest::qWait(50);
        QVERIFY(f.servers()->isHidden());
        QVERIFY(!f.dialog->result().isValid());
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
    qputenv("LANGUAGE", "C");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("plasma-impresario");
    qRegisterMetaType<Result>();
    qRegisterMetaType<Source>();
    AddAccountDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_AddAccountDialog.moc"
