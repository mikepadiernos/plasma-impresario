/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "SearchIndexer.h"

#include <QDataStream>
#include <QDebug>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMimeDatabase>
#include <QSet>
#include <algorithm>

#include "Imap/Encoders.h"
#include "Imap/Exceptions.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Message.h"

namespace Imap {
namespace Mailbox {

namespace {

/** Text kept of one message; longer text is cut, as it rarely adds anything to
 * a search */
const int maximumText = 512 * 1024;

/** A batch which reads attachments ends after this long, so that the window
 * stays responsive */
const int batchBudgetMs = 100;

QString addresses(const QList<Message::MailAddress> &list)
{
    QStringList result;
    for (const Message::MailAddress &address : list)
        result << address.asPrettyString();
    return result.join(QLatin1String(", "));
}

QStringList normalizedFlags(const QStringList &flags)
{
    QStringList result;
    for (const QString &flag : flags) {
        if (flag.compare(QLatin1String("\\Recent"), Qt::CaseInsensitive) != 0)
            result << flag.toLower();
    }
    result.sort();
    result.removeDuplicates();
    return result;
}

bool hasFlag(const QStringList &flags, const QString &flag)
{
    return std::any_of(
        flags.cbegin(), flags.cend(), [&flag](const QString &f) { return f.compare(flag, Qt::CaseInsensitive) == 0; });
}

/** @short Downloaded data of a part, with its transfer encoding undone, or a
 * null array */
QByteArray partData(
    const AbstractCache &cache, const QString &mailbox, uint uid, const QByteArray &partId, const QByteArray &encoding)
{
    QByteArray data = cache.messagePart(mailbox, uid, partId);
    if (!data.isNull())
        return data;
    const QByteArray raw = cache.messagePart(mailbox, uid, partId + ".X-RAW");
    if (raw.isNull())
        return QByteArray();
    decodeContentTransferEncoding(raw, encoding.toLower(), &data);
    return data.isNull() ? QByteArray("") : data;
}

/** @short The BODYSTRUCTURE which the cache keeps serialized, or null */
QSharedPointer<Message::AbstractMessage> bodyStructure(QByteArray serialized)
{
    QDataStream stream(&serialized, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    QVariantList list;
    stream >> list;
    try {
        return Message::AbstractMessage::fromList(list, QByteArray(), 0);
    } catch (const Imap::ParserException &) {
        return QSharedPointer<Message::AbstractMessage>();
    }
}

/** @short Collects the text and the attachments of a message from its
 * BODYSTRUCTURE and the cached parts */
struct PartWalker {
    const AbstractCache &cache;
    const QString &mailbox;
    uint uid;
    /** Whether the text of downloaded attachments is read, up to @short limit
     * bytes each */
    bool readAttachments = false;
    quint64 limit = 0;
    const AttachmentTextExtractor *extractor = nullptr;
    QStringList textParts;
    QStringList texts;
    QStringList attachments;
    QStringList attachmentTexts;
    QStringList attachmentParts;
    QStringList attachmentReport;
    bool hasAttachment = false;
    bool foundText = false;

    void children(const Message::MultiMessage *multi, const QByteArray &prefix)
    {
        for (int i = 0; i < multi->bodies.size(); ++i) {
            const QByteArray number = QByteArray::number(i + 1);
            visit(multi->bodies.at(i).data(), prefix.isEmpty() ? number : prefix + '.' + number);
        }
    }

    void visit(const Message::AbstractMessage *part, const QByteArray &id)
    {
        if (!part)
            return;
        if (const auto *multi = dynamic_cast<const Message::MultiMessage *>(part)) {
            children(multi, id);
            return;
        }
        if (const auto *attached = dynamic_cast<const Message::MsgMessage *>(part)) {
            // An attached message counts by its subject; its own text is not indexed
            hasAttachment = true;
            const QString name = extractRfc2231Param(attached->bodyFldDsp.second, "FILENAME");
            attachments << (name.isEmpty() ? attached->envelope.subject : name);
            return;
        }
        const auto *one = dynamic_cast<const Message::OneMessage *>(part);
        if (!one)
            return;
        const QByteArray disposition = one->bodyFldDsp.first.toLower();
        QString fileName = extractRfc2231Param(one->bodyFldDsp.second, "FILENAME");
        if (fileName.isEmpty())
            fileName = extractRfc2231Param(one->bodyFldParam, "NAME");
        const bool isText = one->mediaType == "text" && (one->mediaSubType == "plain" || one->mediaSubType == "html");
        if (isText && disposition != "attachment") {
            textParts << QString::fromLatin1(id);
            const QByteArray data = partData(cache, mailbox, uid, id, one->bodyFldEnc);
            if (data.isNull())
                return;
            foundText = true;
            const QString text = decodeByteArray(data, one->bodyFldParam.value("CHARSET"));
            texts << (one->mediaSubType == "html" ? SearchIndex::plainTextOfHtml(text) : text);
            return;
        }
        const bool signature = one->mediaType == "application"
            && (one->mediaSubType == "pgp-signature" || one->mediaSubType == "pkcs7-signature"
                || one->mediaSubType == "x-pkcs7-signature");
        if (signature)
            return;
        if (disposition == "attachment" || !fileName.isEmpty()) {
            hasAttachment = true;
            if (!fileName.isEmpty())
                attachments << fileName;
            attachment(one, id, fileName);
        }
    }

    void attachment(const Message::OneMessage *one, const QByteArray &id, const QString &fileName)
    {
        const QByteArray type = one->mediaType.toLower();
        const QByteArray subType = one->mediaSubType.toLower();
        // Pictures, sound and video have no text to read, and keys and encrypted
        // data none worth searching
        if (type == "image" || type == "audio" || type == "video")
            return;
        if (type == "application" && (subType.startsWith("pgp") || subType.contains("pkcs7") || subType == "pkcs10"))
            return;
        const QString partId = QString::fromLatin1(id);
        if (!readAttachments) {
            // Kept, so that the attachment is read once that is turned on
            attachmentParts << partId;
            return;
        }
        const QString declared = QString::fromLatin1(type + '/' + subType);
        // The size on the server is that of the encoded part, a third more than the
        // file in base64
        const quint64 size = one->bodyFldEnc.toLower() == "base64" ? one->bodyFldOctets / 4 * 3 : one->bodyFldOctets;
        if (size > limit) {
            attachmentReport << QLatin1String("large ") + declared;
            return;
        }
        const QByteArray data = partData(cache, mailbox, uid, id, one->bodyFldEnc);
        if (data.isNull()) {
            // Never downloaded for the index; it is read once the user downloads it
            attachmentParts << partId;
            return;
        }
        const QString mimeType = AttachmentTextExtractor::effectiveMimeType(data, declared, fileName);
        if (quint64(data.size()) > limit) {
            attachmentReport << QLatin1String("large ") + mimeType;
            return;
        }
        QString text;
        AttachmentTextExtractor::Outcome outcome = AttachmentTextExtractor::Outcome::NoExtractor;
        const QMimeType mime = QMimeDatabase().mimeTypeForName(mimeType);
        if (mime.isValid()
            && (mime.inherits(QStringLiteral("text/plain")) || mime.inherits(QStringLiteral("text/html")))) {
            // Text needs no extractor
            text = decodeByteArray(data, one->bodyFldParam.value("CHARSET"));
            if (mime.inherits(QStringLiteral("text/html")))
                text = SearchIndex::plainTextOfHtml(text);
            outcome = AttachmentTextExtractor::Outcome::Extracted;
        } else if (extractor) {
            outcome = extractor->extract(data, mimeType, fileName, &text);
        }
        switch (outcome) {
        case AttachmentTextExtractor::Outcome::Extracted:
            attachmentReport << QLatin1String("read ") + mimeType;
            if (!text.trimmed().isEmpty())
                attachmentTexts << text;
            break;
        case AttachmentTextExtractor::Outcome::NoExtractor:
            attachmentReport << QLatin1String("none ") + mimeType;
            break;
        case AttachmentTextExtractor::Outcome::Failed:
            attachmentReport << QLatin1String("failed ") + mimeType;
            break;
        }
    }
};

} // namespace

SearchIndexer::SearchIndexer(const CacheSource &cache, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
{
    m_scheduled.setSingleShot(true);
    m_scheduled.setInterval(30 * 1000);
    connect(&m_scheduled, &QTimer::timeout, this, &SearchIndexer::update);
    m_batch.setSingleShot(true);
    m_batch.setInterval(20);
    connect(&m_batch, &QTimer::timeout, this, &SearchIndexer::step);
}

SearchIndexer::~SearchIndexer() = default;

void SearchIndexer::setTimings(int scheduleMs, int batchMs, int batchSize)
{
    m_scheduled.setInterval(scheduleMs);
    m_batch.setInterval(batchMs);
    m_batchSize = std::max(1, batchSize);
}

bool SearchIndexer::open()
{
    m_index.reset();
    m_connection.clear();
    m_changed.clear();
    m_recoveries = 0;
    const std::shared_ptr<AbstractCache> cache = m_cache ? m_cache() : nullptr;
    const auto *combined = dynamic_cast<const CombinedCache *>(cache.get());
    if (!combined) {
        m_unavailable = tr("The account keeps no offline cache, so there is nothing to index. "
                           "Choose to keep messages offline in the account settings.");
        return false;
    }
    m_connection = combined->databaseConnectionName();
    auto index = std::make_unique<SearchIndex>(m_connection);
    if (!index->open()) {
        m_unavailable = index->errorString();
        return false;
    }
    QString problem;
    if (!index->verify(false, &problem)) {
        if (!index->isCorrupt()) {
            m_unavailable = problem;
            return false;
        }
        qWarning() << "Search index: building the index again:" << problem;
        // Prepared statements of the cache which are still reading would stop
        // SQLite from dropping tables
        if (auto writable = std::dynamic_pointer_cast<CombinedCache>(cache))
            writable->finishQueries();
        if (!index->recreate()) {
            m_unavailable = index->errorString();
            return false;
        }
        ++m_recoveries;
    }
    m_index = std::move(index);
    m_unavailable.clear();
    return true;
}

bool SearchIndexer::isAvailable() const
{
    return m_index && m_index->isOpen() && usableCache();
}

QString SearchIndexer::unavailableReason() const
{
    if (!m_unavailable.isEmpty() || isAvailable())
        return m_unavailable;
    return tr("The offline cache of the account failed, so the index cannot be "
              "used until the account reconnects.");
}

SearchIndex *SearchIndexer::index() const
{
    return m_index.get();
}

bool SearchIndexer::isRunning() const
{
    return m_running;
}

std::shared_ptr<AbstractCache> SearchIndexer::usableCache() const
{
    std::shared_ptr<AbstractCache> cache = m_cache ? m_cache() : nullptr;
    // After a failure the account switches to a cache in memory, which has a
    // different connection or none
    const auto *combined = dynamic_cast<const CombinedCache *>(cache.get());
    if (!combined || combined->databaseConnectionName() != m_connection)
        return nullptr;
    return cache;
}

SearchIndexer::Progress SearchIndexer::progress() const
{
    Progress progress;
    if (!m_running)
        return progress;
    progress.mailbox = m_mailbox;
    progress.folders = m_passFolders;
    progress.folder = std::min(m_passFolders, std::max(1, m_passFolders - int(m_queue.size())));
    progress.position = m_position;
    progress.messages = int(m_uids.size());
    return progress;
}

SearchIndexer::PassStatistics SearchIndexer::lastPass() const
{
    return m_lastPass;
}

int SearchIndexer::recoveries() const
{
    return m_recoveries;
}

bool SearchIndexer::recover()
{
    qWarning() << "Search index: the index is damaged, building it again:" << m_index->errorString();
    m_scheduled.stop();
    abandonPass();
    m_changed.clear();
    ++m_recoveries;
    // Prepared statements of the cache which are still reading would stop SQLite
    // from dropping tables
    if (auto combined = std::dynamic_pointer_cast<CombinedCache>(usableCache()))
        combined->finishQueries();
    if (!m_index->recreate()) {
        m_unavailable = m_index->errorString();
        qWarning() << "Search index: cannot build the index again:" << m_unavailable;
        m_index.reset();
        emit progressChanged();
        return false;
    }
    emit progressChanged();
    return true;
}

void SearchIndexer::noteChange(const QModelIndex &index)
{
    if (!m_index || !index.isValid())
        return;
    // Neither role makes the model fetch anything
    const QVariant name = index.data(RoleMailboxName);
    QString mailbox;
    if (index.data(RoleMessageUid).isValid())
        mailbox = name.toString();
    else if (!name.isValid() && index.parent().isValid())
        mailbox = index.parent().data(RoleMailboxName).toString();
    markChanged(mailbox);
    scheduleUpdate();
}

void SearchIndexer::markChanged(const QString &mailbox)
{
    if (mailbox.isEmpty() || m_changed.contains(mailbox))
        return;
    m_changed.insert(mailbox);
    if (m_index)
        m_index->markChanged(mailbox);
}

bool SearchIndexer::isPaused() const
{
    return m_paused;
}

bool SearchIndexer::hasPendingChanges() const
{
    return m_pending || m_again || m_scheduled.isActive();
}

void SearchIndexer::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    if (paused) {
        // What was not indexed yet is indexed on resume; the folders done so far
        // stay done
        m_pending = m_pending || hasPendingChanges() || m_running;
        m_scheduled.stop();
        abandonPass();
        emit progressChanged();
        return;
    }
    emit progressChanged();
    if (m_pending)
        update();
}

void SearchIndexer::abandonPass()
{
    m_batch.stop();
    m_running = false;
    m_again = false;
    m_queue.clear();
    m_passFolders = 0;
    m_mailbox.clear();
    m_uids.clear();
    m_position = 0;
    m_entries.clear();
}

bool SearchIndexer::rebuild()
{
    if (!m_index)
        return false;
    m_scheduled.stop();
    abandonPass();
    const bool cleared = m_index->clear();
    if (m_paused)
        m_pending = true;
    else
        update();
    emit progressChanged();
    return cleared;
}

QList<SearchIndexer::FolderCoverage> SearchIndexer::coverage() const
{
    QList<FolderCoverage> result;
    const std::shared_ptr<AbstractCache> cache = m_index ? usableCache() : nullptr;
    if (!cache)
        return result;
    QHash<QString, SearchIndex::Folder> indexed;
    const QList<SearchIndex::Folder> folders = m_index->folders();
    for (const SearchIndex::Folder &folder : folders)
        indexed.insert(folder.mailbox, folder);
    const QStringList mailboxes = cachedFolders(*cache);
    for (const QString &mailbox : mailboxes) {
        FolderCoverage coverage;
        coverage.mailbox = mailbox;
        const SyncState state = cache->mailboxSyncState(mailbox);
        const bool cached = state.isUsableForSyncing() && state.uidValidity() != 0;
        int mapped = 0;
        if (cached) {
            const Imap::Uids uids = cache->uidMapping(mailbox);
            mapped = int(uids.size());
            coverage.cached = int(std::count_if(uids.cbegin(), uids.cend(), [](uint uid) { return uid != 0; }));
        }
        const auto record = indexed.constFind(mailbox);
        if (record != indexed.constEnd()) {
            coverage.indexed = record->indexed;
            coverage.withText = record->withText;
            coverage.updated = record->updated;
        }
        if (m_excluded.contains(mailbox))
            coverage.state = FolderCoverage::State::Excluded;
        else if (!cached)
            coverage.state = FolderCoverage::State::NotCached;
        else if (record == indexed.constEnd())
            coverage.state = FolderCoverage::State::NotIndexed;
        else if (record->uidValidity != state.uidValidity() || record->messages != mapped)
            coverage.state = FolderCoverage::State::OutOfDate;
        else if (record->indexed < coverage.cached)
            coverage.state = FolderCoverage::State::Partial;
        else
            coverage.state = FolderCoverage::State::Indexed;
        result << coverage;
    }
    return result;
}

void SearchIndexer::setExcludedFolders(const QStringList &mailboxes)
{
    QSet<QString> excluded(mailboxes.cbegin(), mailboxes.cend());
    excluded.remove(QString());
    if (excluded == m_excluded)
        return;
    m_excluded = excluded;
    if (m_index) {
        // The folders done so far stay done; a new pass skips the excluded folders
        // and takes in the included ones
        if (m_running)
            abandonPass();
        for (const QString &mailbox : std::as_const(m_excluded))
            m_index->removeFolder(mailbox);
        if (m_paused)
            m_pending = true;
        else
            update();
    }
    emit progressChanged();
}

QStringList SearchIndexer::excludedFolders() const
{
    QStringList result(m_excluded.cbegin(), m_excluded.cend());
    result.sort();
    return result;
}

void SearchIndexer::setIndexDecrypted(bool enabled)
{
    m_indexDecrypted = enabled;
    if (enabled || !m_index || m_index->decryptedMessages() == 0)
        return;
    m_scheduled.stop();
    abandonPass();
    m_index->removeDecrypted();
    if (m_paused)
        m_pending = true;
    else
        update();
    emit progressChanged();
}

bool SearchIndexer::indexesDecrypted() const
{
    return m_indexDecrypted;
}

bool SearchIndexer::addDecryptedText(const QString &mailbox, uint uid, const QString &text)
{
    if (!m_indexDecrypted || !m_index || !m_index->isOpen() || m_excluded.contains(mailbox) || uid == 0
        || text.trimmed().isEmpty())
        return false;
    if (!m_index->hasMessage(mailbox, uid)) {
        const std::shared_ptr<AbstractCache> cache = usableCache();
        if (!cache)
            return false;
        SearchIndex::Document document;
        if (!this->document(*cache, mailbox, uid, normalizedFlags(cache->msgFlags(mailbox, uid)), &document)
            || !m_index->store({document}))
            return false;
    }
    if (!m_index->addDecryptedText(mailbox, uid, text))
        return false;
    emit progressChanged();
    return true;
}

void SearchIndexer::setAttachmentIndexing(bool enabled, quint64 limit)
{
    m_indexAttachments = enabled;
    m_attachmentLimit = limit;
    if (!m_index || !m_index->isOpen())
        return;
    // The index remembers how its attachments were read, so that a change is
    // noticed after a restart as well
    const QString key = QStringLiteral("attachments");
    const QString wanted = enabled ? QString::number(limit) : QString();
    const QString built = m_index->setting(key);
    if (built == wanted)
        return;
    if (!m_index->setSetting(key, wanted))
        return;
    if (!built.isEmpty()) {
        // Turned off, or a different limit: the messages whose attachments were
        // read or tried are indexed again
        m_scheduled.stop();
        abandonPass();
        m_index->forgetAttachmentText();
    }
    // Turned on: the next pass reads the attachments which are already downloaded
    if (m_paused)
        m_pending = true;
    else
        update();
    emit progressChanged();
}

bool SearchIndexer::indexesAttachments() const
{
    return m_indexAttachments;
}

quint64 SearchIndexer::attachmentLimit() const
{
    return m_attachmentLimit;
}

void SearchIndexer::setAttachmentExtractor(const std::shared_ptr<AttachmentTextExtractor> &extractor)
{
    m_extractor = extractor;
}

const AttachmentTextExtractor *SearchIndexer::attachmentExtractor() const
{
    if (!m_extractor)
        m_extractor = AttachmentTextExtractor::createDefault();
    return m_extractor.get();
}

bool SearchIndexer::deleteIndex(QString *problem)
{
    m_scheduled.stop();
    abandonPass();
    m_pending = false;
    // Prepared statements of the index and the cache which are still reading
    // would stop SQLite from dropping tables
    m_index.reset();
    if (auto combined = std::dynamic_pointer_cast<CombinedCache>(usableCache()))
        combined->finishQueries();
    const bool deleted = m_connection.isEmpty() || SearchIndex::drop(m_connection, problem);
    m_unavailable = tr("The offline search index was deleted.");
    emit progressChanged();
    return deleted;
}

SearchIndexer *SearchIndexer::forModel(const QObject *model)
{
    return model ? model->findChild<SearchIndexer *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

void SearchIndexer::scheduleUpdate()
{
    if (!m_index)
        return;
    if (m_paused) {
        m_pending = true;
        return;
    }
    if (m_running) {
        m_again = true;
        return;
    }
    if (!m_scheduled.isActive())
        m_scheduled.start();
}

void SearchIndexer::update()
{
    m_scheduled.stop();
    if (!m_index)
        return;
    if (m_paused) {
        m_pending = true;
        return;
    }
    if (m_running) {
        m_again = true;
        return;
    }
    if (m_index->isCorrupt() && !recover())
        return;
    const std::shared_ptr<AbstractCache> cache = usableCache();
    if (!cache)
        return;
    m_running = true;
    m_again = false;
    m_pending = false;
    m_pass = PassStatistics();
    m_passTimer.start();
    beginPass(*cache);
    emit progressChanged();
    m_batch.start();
}

QStringList SearchIndexer::cachedFolders(const AbstractCache &cache, QSet<QString> *listed)
{
    QStringList result;
    QSet<QString> seen;
    QStringList pending{QString()};
    while (!pending.isEmpty()) {
        const QString parent = pending.takeFirst();
        const QList<MailboxMetadata> children = cache.childMailboxes(parent);
        for (const MailboxMetadata &child : children) {
            if (child.mailbox.isEmpty() || seen.contains(child.mailbox))
                continue;
            seen.insert(child.mailbox);
            if (!hasFlag(child.flags, QStringLiteral("\\Noselect"))
                && !hasFlag(child.flags, QStringLiteral("\\NonExistent")))
                result << child.mailbox;
            if (!hasFlag(child.flags, QStringLiteral("\\HasNoChildren"))
                && !hasFlag(child.flags, QStringLiteral("\\Noinferiors")))
                pending << child.mailbox;
        }
    }
    if (listed)
        *listed = seen;
    return result;
}

void SearchIndexer::beginPass(const AbstractCache &cache)
{
    m_mailbox.clear();
    QSet<QString> listed;
    m_queue = cachedFolders(cache, &listed);
    m_queue.removeIf([this](const QString &mailbox) { return m_excluded.contains(mailbox); });
    m_passFolders = int(m_queue.size());
    // Folders which are gone from the account, or kept out of the index, are gone
    // from the index too
    const QList<SearchIndex::Folder> indexed = m_index->folders();
    for (const SearchIndex::Folder &folder : indexed) {
        if (!listed.contains(folder.mailbox) || m_excluded.contains(folder.mailbox))
            m_index->removeFolder(folder.mailbox);
    }
}

bool SearchIndexer::beginFolder(const AbstractCache &cache, const QString &mailbox)
{
    if (m_excluded.contains(mailbox))
        return false;
    const SyncState state = cache.mailboxSyncState(mailbox);
    if (!state.isUsableForSyncing() || state.uidValidity() == 0)
        return false;
    SearchIndex::Folder folder = m_index->folder(mailbox);
    if (folder.uidValidity != 0 && folder.uidValidity != state.uidValidity()) {
        // The server numbered the folder anew, so none of the indexed UIDs mean the
        // same messages any more
        m_index->removeFolder(mailbox);
    } else if (!m_changed.contains(mailbox) && folder.highestModSeq != 0
        && folder.highestModSeq == state.highestModSeq() && folder.uidNext == state.uidNext()
        && folder.messages == int(state.exists()) && m_index->documentCount(mailbox) == folder.indexed) {
        // With CONDSTORE, any change of flags raises HIGHESTMODSEQ and new mail
        // raises UIDNEXT; the model reports downloaded text, so nothing in the
        // cache differs from the index
        ++m_pass.skipped;
        return false;
    }
    // Changes from here on are looked at by the next pass
    m_changed.remove(mailbox);
    ++m_pass.scanned;
    m_mailbox = mailbox;
    m_folder = SearchIndex::Folder();
    m_folder.mailbox = mailbox;
    m_folder.uidValidity = state.uidValidity();
    m_folder.highestModSeq = state.highestModSeq();
    m_folder.uidNext = state.uidNext();
    m_uids = cache.uidMapping(mailbox);
    m_folder.messages = int(m_uids.size());
    m_position = 0;
    m_entries = m_index->entries(mailbox);
    const QSet<uint> present(m_uids.cbegin(), m_uids.cend());
    QList<uint> gone;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (!present.contains(it.key()))
            gone << it.key();
    }
    if (!gone.isEmpty())
        m_index->remove(mailbox, gone);
    return true;
}

void SearchIndexer::indexBatch(const AbstractCache &cache)
{
    QList<SearchIndex::Document> documents;
    QHash<uint, QStringList> flagChanges;
    QElapsedTimer elapsed;
    elapsed.start();
    const int start = m_position;
    const int end = std::min(int(m_uids.size()), m_position + m_batchSize);
    for (; m_position < end; ++m_position) {
        // Reading attachments can take a while, so a slow batch stops early and
        // goes on after a pause
        if (m_position > start && elapsed.elapsed() > batchBudgetMs)
            break;
        const uint uid = m_uids.at(m_position);
        if (uid == 0)
            continue;
        const QStringList flags = normalizedFlags(cache.msgFlags(m_mailbox, uid));
        const auto entry = m_entries.constFind(uid);
        const auto downloaded = [&](const QString &part) {
            const QByteArray id = part.toLatin1();
            return !cache.messagePart(m_mailbox, uid, id).isNull()
                || !cache.messagePart(m_mailbox, uid, id + ".X-RAW").isNull();
        };
        bool textArrived = false;
        if (entry != m_entries.constEnd() && !entry->hasText)
            textArrived = std::any_of(entry->textParts.cbegin(), entry->textParts.cend(), downloaded);
        if (entry != m_entries.constEnd() && !textArrived && m_indexAttachments)
            textArrived = std::any_of(entry->attachmentParts.cbegin(), entry->attachmentParts.cend(), downloaded);
        if (entry == m_entries.constEnd() || textArrived) {
            SearchIndex::Document document;
            if (!this->document(cache, m_mailbox, uid, flags, &document))
                continue;
            ++m_folder.indexed;
            if (document.hasText)
                ++m_folder.withText;
            documents << document;
            continue;
        }
        ++m_folder.indexed;
        if (entry->hasText)
            ++m_folder.withText;
        if (entry->flags != flags)
            flagChanges.insert(uid, flags);
    }
    m_pass.messages += m_position - start;
    m_pass.stored += int(documents.size());
    if (!documents.isEmpty())
        m_index->store(documents);
    if (!flagChanges.isEmpty())
        m_index->setFlags(m_mailbox, flagChanges);
}

void SearchIndexer::endFolder()
{
    m_folder.updated = QDateTime::currentDateTimeUtc();
    // A folder which changed while it was being indexed is looked at in full
    // again, even after a restart
    if (m_changed.contains(m_mailbox))
        m_folder.highestModSeq = 0;
    m_index->setFolder(m_folder);
    m_mailbox.clear();
    m_uids.clear();
    m_entries.clear();
    emit progressChanged();
}

void SearchIndexer::endPass()
{
    m_running = false;
    m_queue.clear();
    m_passFolders = 0;
    m_pass.finished = QDateTime::currentDateTimeUtc();
    m_pass.elapsedMs = m_passTimer.isValid() ? m_passTimer.elapsed() : 0;
    m_lastPass = m_pass;
    emit progressChanged();
    emit finished();
    if (m_again) {
        m_again = false;
        scheduleUpdate();
    }
}

void SearchIndexer::step()
{
    if (!m_running)
        return;
    const std::shared_ptr<AbstractCache> cache = usableCache();
    if (!cache || !m_index) {
        m_mailbox.clear();
        endPass();
        return;
    }
    if (m_mailbox.isEmpty()) {
        while (!m_queue.isEmpty() && !beginFolder(*cache, m_queue.takeFirst())) { }
        if (m_mailbox.isEmpty()) {
            endPass();
            return;
        }
    }
    indexBatch(*cache);
    if (m_index->isCorrupt()) {
        // Indexing goes on with an index built anew, from the first folder
        if (recover())
            update();
        else
            emit finished();
        return;
    }
    if (m_position >= m_uids.size())
        endFolder();
    m_batch.start();
}

QString SearchIndexer::downloadedText(const AbstractCache &cache, const QString &mailbox, uint uid)
{
    const AbstractCache::MessageDataBundle data = cache.messageMetadata(mailbox, uid);
    if (data.uid != uid)
        return QString();
    const QSharedPointer<Message::AbstractMessage> structure = bodyStructure(data.serializedBodyStructure);
    if (!structure)
        return QString();
    PartWalker walker{cache, mailbox, uid};
    if (const auto *multi = dynamic_cast<const Message::MultiMessage *>(structure.data()))
        walker.children(multi, QByteArray());
    else
        walker.visit(structure.data(), "1");
    QString text = walker.texts.join(QLatin1Char('\n'));
    if (text.size() > maximumText)
        text.truncate(maximumText);
    return text;
}

bool SearchIndexer::document(const AbstractCache &cache, const QString &mailbox, uint uid, const QStringList &flags,
    SearchIndex::Document *document) const
{
    AbstractCache::MessageDataBundle data = cache.messageMetadata(mailbox, uid);
    if (data.uid != uid)
        return false;
    SearchIndex::Document &d = *document;
    d = SearchIndex::Document();
    d.mailbox = mailbox;
    d.uid = uid;
    d.flags = flags;
    const Message::Envelope &envelope = data.envelope;
    d.sent = envelope.date.isValid() ? envelope.date.date() : QDate();
    d.received = data.internalDate.isValid() ? data.internalDate.date() : QDate();
    d.size = data.size;
    d.subject = envelope.subject;
    d.from = addresses(envelope.from);
    d.to = addresses(envelope.to);
    d.cc = addresses(envelope.cc);
    d.bcc = addresses(envelope.bcc);
    d.messageId = QString::fromUtf8(envelope.messageId);
    QStringList inReplyTo;
    for (const QByteArray &id : envelope.inReplyTo)
        inReplyTo << QString::fromUtf8(id);
    d.inReplyTo = inReplyTo.join(QLatin1Char(' '));
    QStringList references;
    for (const QByteArray &id : std::as_const(data.hdrReferences))
        references << QString::fromUtf8(id);
    d.references = references.join(QLatin1Char(' '));
    d.listId = QString::fromUtf8(data.hdrList.value("list-id"));
    QStringList listPost;
    for (const QUrl &url : std::as_const(data.hdrListPost))
        listPost << url.toString();
    d.listPost = listPost.join(QLatin1Char(' '));

    const QSharedPointer<Message::AbstractMessage> structure = bodyStructure(data.serializedBodyStructure);
    if (!structure) {
        // Without the structure there are no parts to find; the headers are still
        // worth indexing
        d.hasText = true;
        return true;
    }
    d.contentType = QString::fromLatin1(structure->mediaType + '/' + structure->mediaSubType).toLower();
    PartWalker walker{cache, mailbox, uid};
    walker.readAttachments = m_indexAttachments;
    walker.limit = m_attachmentLimit;
    if (m_indexAttachments)
        walker.extractor = attachmentExtractor();
    if (const auto *multi = dynamic_cast<const Message::MultiMessage *>(structure.data()))
        walker.children(multi, QByteArray());
    else
        walker.visit(structure.data(), "1");
    d.textParts = walker.textParts;
    d.hasText = walker.foundText || walker.textParts.isEmpty();
    d.body = walker.texts.join(QLatin1Char('\n'));
    if (d.body.size() > maximumText)
        d.body.truncate(maximumText);
    d.hasAttachment = walker.hasAttachment;
    d.attachments = walker.attachments.join(QLatin1Char('\n'));
    d.attachmentText = walker.attachmentTexts.join(QLatin1Char('\n'));
    if (d.attachmentText.size() > maximumText)
        d.attachmentText.truncate(maximumText);
    d.attachmentParts = walker.attachmentParts;
    d.attachmentReport = walker.attachmentReport;
    return true;
}

} // namespace Mailbox
} // namespace Imap
