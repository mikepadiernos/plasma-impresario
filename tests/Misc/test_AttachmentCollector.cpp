/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QStandardItemModel>
#include <QtTest>
#include "test_AttachmentCollector.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/AttachmentCollector.h"

namespace {

QStandardItem *part(const char *mimeType, const QString &fileName = QString(), const char *disposition = "")
{
    auto *item = new QStandardItem(QString::fromLatin1(mimeType));
    item->setData(QByteArray(mimeType), Imap::Mailbox::RolePartMimeType);
    item->setData(fileName, Imap::Mailbox::RolePartFileName);
    item->setData(QByteArray(disposition), Imap::Mailbox::RolePartBodyDisposition);
    return item;
}

QStandardItem *withChildren(QStandardItem *parent, const QList<QStandardItem *> &children)
{
    for (auto *child : children)
        parent->appendRow(child);
    return parent;
}

/** Build a model shaped like Cryptography::MessageModel: index(0, 0) is the message, its child is the body. */
void message(QStandardItemModel &model, QStandardItem *body)
{
    auto *root = new QStandardItem(QStringLiteral("message"));
    root->appendRow(body);
    model.appendRow(root);
}

QStringList names(const QList<QPersistentModelIndex> &parts)
{
    QStringList out;
    for (const auto &index : parts)
        out << index.data(Imap::Mailbox::RolePartFileName).toString();
    return out;
}

}

void AttachmentCollectorTest::noModelOrMessage()
{
    QVERIFY(UiUtils::collectAttachments(static_cast<const QAbstractItemModel *>(nullptr)).isEmpty());
    QStandardItemModel model;
    QVERIFY(UiUtils::collectAttachments(&model).isEmpty());
}

void AttachmentCollectorTest::plainMessageHasNone()
{
    QStandardItemModel model;
    message(model, part("text/plain"));
    QVERIFY(UiUtils::collectAttachments(&model).isEmpty());
}

void AttachmentCollectorTest::alternativeHasNone()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/alternative"), {part("text/plain"), part("text/html", QString(), "inline")}));
    QVERIFY(UiUtils::collectAttachments(&model).isEmpty());
}

void AttachmentCollectorTest::mixedListsFilesInOrder()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/mixed"), {
        withChildren(part("multipart/alternative"), {part("text/plain"), part("text/html")}),
        part("application/pdf", QStringLiteral("report.pdf"), "attachment"),
        part("image/png", QStringLiteral("photo.png"), "inline"),
        part("application/zip", QStringLiteral("archive.zip")),
    }));
    QCOMPARE(names(UiUtils::collectAttachments(&model)),
             QStringList({QStringLiteral("report.pdf"), QStringLiteral("photo.png"), QStringLiteral("archive.zip")}));
}

void AttachmentCollectorTest::inlineTextIsSkippedUnlessNamedOrAttached()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/mixed"), {
        part("text/plain"),
        part("text/plain", QString(), "INLINE"),
        part("text/plain", QStringLiteral("notes.txt")),
        part("text/html", QString(), "attachment"),
    }));
    const auto attachments = UiUtils::collectAttachments(&model);
    QCOMPARE(attachments.size(), 2);
    QCOMPARE(attachments.at(0).data(Imap::Mailbox::RolePartFileName).toString(), QStringLiteral("notes.txt"));
    QCOMPARE(attachments.at(1).data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArray("text/html"));
}

void AttachmentCollectorTest::signedIgnoresSignature()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/signed"), {
        withChildren(part("multipart/mixed"), {part("text/plain"), part("application/pdf", QStringLiteral("invoice.pdf"))}),
        part("application/pgp-signature", QStringLiteral("signature.asc")),
    }));
    QCOMPARE(names(UiUtils::collectAttachments(&model)), QStringList({QStringLiteral("invoice.pdf")}));
}

void AttachmentCollectorTest::relatedListsOnlyExplicitAttachments()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/related"), {
        part("text/html"),
        part("image/png", QStringLiteral("logo.png"), "inline"),
        part("image/gif"),
        part("application/pdf", QStringLiteral("terms.pdf"), "attachment"),
    }));
    QCOMPARE(names(UiUtils::collectAttachments(&model)), QStringList({QStringLiteral("terms.pdf")}));
}

void AttachmentCollectorTest::attachedMessageIsListedOnce()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/mixed"), {
        part("text/plain"),
        withChildren(part("message/rfc822", QStringLiteral("forwarded.eml")), {
            withChildren(part("multipart/mixed"), {part("text/plain"), part("image/jpeg", QStringLiteral("inner.jpg"))}),
        }),
    }));
    QCOMPARE(names(UiUtils::collectAttachments(&model)), QStringList({QStringLiteral("forwarded.eml")}));
}

void AttachmentCollectorTest::encryptedSkipsCiphertextButListsDecryptedParts()
{
    QStandardItemModel model;
    auto *encrypted = withChildren(part("multipart/encrypted"), {
        part("application/pgp-encrypted"),
        part("application/octet-stream", QStringLiteral("encrypted.asc"), "inline"),
    });
    message(model, encrypted);
    QVERIFY(UiUtils::collectAttachments(&model).isEmpty());

    encrypted->appendRow(withChildren(part("multipart/mixed"), {part("text/plain"), part("text/csv", QStringLiteral("data.csv"))}));
    QCOMPARE(names(UiUtils::collectAttachments(&model)), QStringList({QStringLiteral("data.csv")}));
}

void AttachmentCollectorTest::unknownMultipartIsTreatedAsMixed()
{
    QStandardItemModel model;
    message(model, withChildren(part("multipart/x-unknown"), {part("text/plain"), part("audio/ogg", QStringLiteral("memo.ogg"))}));
    QCOMPARE(names(UiUtils::collectAttachments(&model)), QStringList({QStringLiteral("memo.ogg")}));

    const QModelIndex body = model.index(0, 0, model.index(0, 0));
    QCOMPARE(names(UiUtils::collectAttachments(body)), QStringList({QStringLiteral("memo.ogg")}));
}

QTEST_GUILESS_MAIN(AttachmentCollectorTest)
