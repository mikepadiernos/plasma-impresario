/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_LinkSafety.h"
#include "UiUtils/LinkSafety.h"
#include <QtTest>

using UiUtils::LinkWarning;
using Warnings = QList<UiUtils::LinkWarning>;

void LinkSafetyTest::readsLinkTextHost_data() {
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("host");

  QTest::newRow("url") << QStringLiteral("https://www.Example.org/login?x=1")
                       << QStringLiteral("www.example.org");
  QTest::newRow("url with port")
      << QStringLiteral("http://example.org:8080/") << QStringLiteral("example.org");
  QTest::newRow("www") << QStringLiteral(" www.example.org ")
                       << QStringLiteral("www.example.org");
  QTest::newRow("bare domain with path")
      << QStringLiteral("bank.example.com/account")
      << QStringLiteral("bank.example.com");
  QTest::newRow("trailing dot") << QStringLiteral("example.org.")
                                << QStringLiteral("example.org");
  QTest::newRow("international")
      << QString::fromUtf8("https://b\xc3\xbc" "cher.example/")
      << QStringLiteral("xn--bcher-kva.example");
  QTest::newRow("words") << QStringLiteral("Click here") << QString();
  QTest::newRow("sentence") << QStringLiteral("Sign in at example.org")
                            << QString();
  QTest::newRow("file name") << QStringLiteral("report.pdf") << QString();
  QTest::newRow("version") << QStringLiteral("1.2.3") << QString();
  QTest::newRow("single label") << QStringLiteral("https://intranet/")
                                << QString();
  QTest::newRow("address") << QStringLiteral("ann@example.org") << QString();
  QTest::newRow("empty") << QString() << QString();
}

void LinkSafetyTest::readsLinkTextHost() {
  QFETCH(QString, text);
  QFETCH(QString, host);
  QCOMPARE(UiUtils::linkTextHost(text), host);
}

void LinkSafetyTest::comparesSites_data() {
  QTest::addColumn<QString>("host");
  QTest::addColumn<QString>("site");
  QTest::addColumn<bool>("same");

  QTest::newRow("equal") << QStringLiteral("example.org")
                         << QStringLiteral("example.org") << true;
  QTest::newRow("www") << QStringLiteral("example.org")
                       << QStringLiteral("www.example.org") << true;
  QTest::newRow("subdomain") << QStringLiteral("news.example.org")
                             << QStringLiteral("example.org") << true;
  QTest::newRow("parent") << QStringLiteral("example.org")
                          << QStringLiteral("mail.example.org") << true;
  QTest::newRow("case") << QStringLiteral("Example.ORG")
                        << QStringLiteral("example.org") << true;
  QTest::newRow("lookalike suffix") << QStringLiteral("example.org.evil.test")
                                    << QStringLiteral("example.org") << false;
  QTest::newRow("prefix") << QStringLiteral("myexample.org")
                          << QStringLiteral("example.org") << false;
  QTest::newRow("other") << QStringLiteral("evil.test")
                         << QStringLiteral("example.org") << false;
  QTest::newRow("empty") << QString() << QStringLiteral("example.org")
                         << false;
}

void LinkSafetyTest::comparesSites() {
  QFETCH(QString, host);
  QFETCH(QString, site);
  QFETCH(bool, same);
  QCOMPARE(UiUtils::isSameSite(host, site), same);
}

void LinkSafetyTest::assessesLinks_data() {
  QTest::addColumn<QUrl>("target");
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("host");
  QTest::addColumn<Warnings>("warnings");

  QTest::newRow("plain link")
      << QUrl(QStringLiteral("https://example.org/news"))
      << QStringLiteral("Read the news") << QStringLiteral("example.org")
      << Warnings();
  QTest::newRow("matching text")
      << QUrl(QStringLiteral("https://www.example.org/news"))
      << QStringLiteral("example.org/news") << QStringLiteral("www.example.org")
      << Warnings();
  QTest::newRow("text shows other site")
      << QUrl(QStringLiteral("https://evil.test/login"))
      << QStringLiteral("https://bank.example.com/login")
      << QStringLiteral("evil.test")
      << Warnings{LinkWarning::TextShowsOtherSite};
  QTest::newRow("lookalike suffix")
      << QUrl(QStringLiteral("https://bank.example.com.evil.test/"))
      << QStringLiteral("bank.example.com")
      << QStringLiteral("bank.example.com.evil.test")
      << Warnings{LinkWarning::TextShowsOtherSite};
  QTest::newRow("user info")
      << QUrl(QStringLiteral("https://bank.example.com@evil.test/"))
      << QStringLiteral("Sign in") << QStringLiteral("evil.test")
      << Warnings{LinkWarning::UserInfo};
  QTest::newRow("ip address")
      << QUrl(QStringLiteral("http://192.0.2.7/login"))
      << QStringLiteral("Sign in") << QStringLiteral("192.0.2.7")
      << Warnings{LinkWarning::IpAddress};
  // QUrl normalizes the forms which browsers accept
  QTest::newRow("hex ip address")
      << QUrl(QStringLiteral("http://0x7f.1/")) << QString()
      << QStringLiteral("127.0.0.1") << Warnings{LinkWarning::IpAddress};
  QTest::newRow("ipv6 address")
      << QUrl(QStringLiteral("http://[2001:db8::1]/")) << QString()
      << QStringLiteral("2001:db8::1") << Warnings{LinkWarning::IpAddress};
  QTest::newRow("international host")
      << QUrl(QString::fromUtf8("https://p\xd0\xb0ypal.example/"))
      << QStringLiteral("Pay now") << QStringLiteral("xn--pypal-4ve.example")
      << Warnings{LinkWarning::InternationalHost};
  QTest::newRow("everything")
      << QUrl(QStringLiteral("https://user@xn--pypal-4ve.example/"))
      << QStringLiteral("www.example.org")
      << QStringLiteral("xn--pypal-4ve.example")
      << Warnings{LinkWarning::TextShowsOtherSite, LinkWarning::UserInfo,
                  LinkWarning::InternationalHost};
}

void LinkSafetyTest::assessesLinks() {
  QFETCH(QUrl, target);
  QFETCH(QString, text);
  QFETCH(QString, host);
  QFETCH(Warnings, warnings);
  const UiUtils::LinkAssessment result = UiUtils::assessLink(target, text);
  QCOMPARE(result.host, host);
  QCOMPARE(result.warnings, warnings);
}

void LinkSafetyTest::listsRemoteHosts() {
  QCOMPARE(UiUtils::remoteHosts(
               {QUrl(QStringLiteral("https://Images.example.org/a.png")),
                QUrl(QStringLiteral("https://tracker.test/p.gif?id=1")),
                QUrl(QStringLiteral("https://images.example.org/b.png")),
                QUrl(QStringLiteral("cid:part1@example.org")), QUrl()}),
           (QStringList{QStringLiteral("images.example.org"),
                        QStringLiteral("tracker.test")}));
}

QTEST_GUILESS_MAIN(LinkSafetyTest)
