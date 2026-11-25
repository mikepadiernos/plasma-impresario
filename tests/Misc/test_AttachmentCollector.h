#ifndef TEST_ATTACHMENTCOLLECTOR_H
#define TEST_ATTACHMENTCOLLECTOR_H

#include <QObject>

/** @short Verify which MIME parts the attachments panel lists */
class AttachmentCollectorTest : public QObject
{
    Q_OBJECT

private slots:
    void noModelOrMessage();
    void plainMessageHasNone();
    void alternativeHasNone();
    void mixedListsFilesInOrder();
    void inlineTextIsSkippedUnlessNamedOrAttached();
    void signedIgnoresSignature();
    void relatedListsOnlyExplicitAttachments();
    void attachedMessageIsListedOnce();
    void encryptedSkipsCiphertextButListsDecryptedParts();
    void unknownMultipartIsTreatedAsMixed();
};

#endif
