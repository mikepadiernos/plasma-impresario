/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef IMAP_MODEL_ATTACHMENTTEXTEXTRACTOR_H
#define IMAP_MODEL_ATTACHMENTTEXTEXTRACTOR_H

#include <memory>
#include <QByteArray>
#include <QString>

namespace Imap
{
namespace Mailbox
{

/** @short Reads the text of an attached file for the search index

The default extractor uses KFileMetaData, the text extractors which Plasma's file search uses, when the application is
built with it. Which formats it can read depends on the extractor plugins installed on the computer, so a format
without one is reported as such rather than indexed as if it had no text.
*/
class AttachmentTextExtractor
{
public:
    enum class Outcome {
        /** The text was read; it may be empty, as for a scanned document */
        Extracted,
        /** No extractor is installed for the format */
        NoExtractor,
        /** An extractor is installed but could not read the file */
        Failed,
    };

    virtual ~AttachmentTextExtractor();

    /** @short Whether an extractor is installed for @arg mimeType */
    virtual bool canExtract(const QString &mimeType) const = 0;
    /** @short Read the text of @arg data, a file of type @arg mimeType called @arg fileName, into @arg text */
    virtual Outcome extract(const QByteArray &data, const QString &mimeType, const QString &fileName,
                            QString *text) const = 0;

    /** @short The extractor this build offers; without KFileMetaData it knows no format */
    static std::shared_ptr<AttachmentTextExtractor> createDefault();

    /** @short The type to read @arg data by: the declared type, unless it is generic and the name or data say more */
    static QString effectiveMimeType(const QByteArray &data, const QString &declared, const QString &fileName);
};

}
}

#endif
