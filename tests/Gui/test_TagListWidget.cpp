/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/TagListWidget.h"
#include "Gui/TagWidget.h"
#include "Imap/Model/FavoriteTagsModel.h"

#include <KLocalizedString>
#include <QApplication>
#include <QStandardPaths>
#include <QTest>

namespace {

QStringList chipTexts(const Gui::TagListWidget &list) {
  QStringList out;
  for (const auto *chip : list.findChildren<Gui::TagWidget *>())
    out << chip->text();
  return out;
}

const Gui::TagWidget *chip(const Gui::TagListWidget &list,
                           const QString &tagName) {
  for (const auto *chip : list.findChildren<Gui::TagWidget *>()) {
    if (chip->tagName() == tagName)
      return chip;
  }
  return nullptr;
}

} // namespace

class TagListWidgetTest : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    KLocalizedString::setApplicationDomain("plasma-impresario");
  }

  void gmailLabelNames() {
    using Gui::TagListWidget;
    QCOMPARE(TagListWidget::gmailLabelDisplayName(QStringLiteral("Trips/2026")),
             QStringLiteral("Trips/2026"));
    QCOMPARE(TagListWidget::gmailLabelDisplayName(QStringLiteral("\\Inbox")),
             QStringLiteral("Inbox"));
    QCOMPARE(
        TagListWidget::gmailLabelDisplayName(QStringLiteral("\\IMPORTANT")),
        QStringLiteral("Important"));
    QCOMPARE(TagListWidget::gmailLabelDisplayName(QStringLiteral("\\Sent")),
             QStringLiteral("Sent"));
    // Already shown as the star and the Drafts folder
    QVERIFY(TagListWidget::gmailLabelDisplayName(QStringLiteral("\\Starred"))
                .isEmpty());
    QVERIFY(TagListWidget::gmailLabelDisplayName(QStringLiteral("\\Draft"))
                .isEmpty());
    QCOMPARE(TagListWidget::gmailLabelDisplayName(QStringLiteral("\\Future")),
             QStringLiteral("Future"));
  }

  void labelsComeBeforeTagsAndStay() {
    Imap::Mailbox::FavoriteTagsModel favorites;
    Gui::TagListWidget list(nullptr, &favorites);
    list.setTagList({QStringLiteral("$work")});
    list.setLabelList({QStringLiteral("\\Important"),
                       QStringLiteral("\\Starred"), QStringLiteral("Trips")});

    const QStringList texts = chipTexts(list);
    QVERIFY(texts.contains(QStringLiteral("Important")));
    QVERIFY(texts.contains(QStringLiteral("Trips")));
    QVERIFY(!texts.contains(QStringLiteral("Starred")));
    const auto *trips = chip(list, QStringLiteral("Trips"));
    QVERIFY(trips);
    QVERIFY(trips->toolTip().contains(QStringLiteral("Gmail label")));
    // Read-only: no remove marker, unlike the user's own tag
    QCOMPARE(trips->text(), QStringLiteral("Trips"));
    QVERIFY(chip(list, QStringLiteral("$work"))->text() !=
            QStringLiteral("$work"));

    // New flags keep the labels, and new labels keep the tags
    list.setTagList({QStringLiteral("$home")});
    QVERIFY(chip(list, QStringLiteral("Trips")));
    QVERIFY(!chip(list, QStringLiteral("$work")));
    list.setLabelList({});
    QVERIFY(!chip(list, QStringLiteral("Trips")));
    QVERIFY(chip(list, QStringLiteral("$home")));
  }
};

QTEST_MAIN(TagListWidgetTest)
#include "test_TagListWidget.moc"
