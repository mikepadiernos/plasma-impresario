/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AttachmentTextExtractor.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QTemporaryDir>

#include "configure.cmake.h"

#ifdef TROJITA_HAVE_KFILEMETADATA
#include <KFileMetaData/ExtractionResult>
#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/SimpleExtractionResult>
#endif

namespace Imap
{
namespace Mailbox
{

namespace
{

#ifndef TROJITA_HAVE_KFILEMETADATA
class NoAttachmentTextExtractor : public AttachmentTextExtractor
{
public:
    bool canExtract(const QString &) const override { return false; }
    Outcome extract(const QByteArray &, const QString &, const QString &, QString *text) const override
    {
        text->clear();
        return Outcome::NoExtractor;
    }
};
#else
class KFileMetaDataTextExtractor : public AttachmentTextExtractor
{
public:
    bool canExtract(const QString &mimeType) const override { return !m_collection.fetchExtractors(mimeType).isEmpty(); }

    Outcome extract(const QByteArray &data, const QString &mimeType, const QString &fileName,
                    QString *text) const override
    {
        text->clear();
        const QList<KFileMetaData::Extractor *> extractors = m_collection.fetchExtractors(mimeType);
        if (extractors.isEmpty())
            return Outcome::NoExtractor;
        // The extractors read files, so the attachment is written to a private folder which goes away afterwards
        QTemporaryDir dir;
        if (!dir.isValid())
            return Outcome::Failed;
        QString suffix = QFileInfo(fileName).suffix();
        if (suffix.isEmpty())
            suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();
        const QString path = dir.filePath(suffix.isEmpty() ? QStringLiteral("attachment")
                                                           : QStringLiteral("attachment.") + suffix);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
            return Outcome::Failed;
        KFileMetaData::SimpleExtractionResult result(path, mimeType, KFileMetaData::ExtractionResult::ExtractPlainText);
        for (KFileMetaData::Extractor *extractor : extractors)
            extractor->extract(&result);
        *text = result.text().trimmed();
        return Outcome::Extracted;
    }

private:
    mutable KFileMetaData::ExtractorCollection m_collection;
};
#endif

}

AttachmentTextExtractor::~AttachmentTextExtractor() = default;

std::shared_ptr<AttachmentTextExtractor> AttachmentTextExtractor::createDefault()
{
#ifdef TROJITA_HAVE_KFILEMETADATA
    return std::make_shared<KFileMetaDataTextExtractor>();
#else
    return std::make_shared<NoAttachmentTextExtractor>();
#endif
}

QString AttachmentTextExtractor::effectiveMimeType(const QByteArray &data, const QString &declared,
                                                   const QString &fileName)
{
    QMimeDatabase db;
    const QString lower = declared.toLower();
    const bool generic = lower.isEmpty() || lower == QLatin1String("application/octet-stream")
        || lower == QLatin1String("application/x-download") || lower == QLatin1String("application/download")
        || lower == QLatin1String("application/force-download") || lower == QLatin1String("application/unknown");
    if (!generic) {
        const QMimeType type = db.mimeTypeForName(lower);
        return type.isValid() ? type.name() : lower;
    }
    const QMimeType guessed = db.mimeTypeForFileNameAndData(fileName, data);
    return guessed.isValid() ? guessed.name() : QStringLiteral("application/octet-stream");
}

}
}
