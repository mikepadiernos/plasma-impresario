/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Composer/Outbox.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <limits>

#include "Common/PrivateFiles.h"
#include "MSA/AbstractMSA.h"

namespace Composer {

namespace {

const QLatin1String entryFile("entry.json");
const QLatin1String messageFile("message.eml");
const QLatin1String sentFile("sent.eml");
const QLatin1String draftFile("draft.draft");

constexpr int defaultAttemptTimeout = 15 * 60 * 1000;

QString stateName(Outbox::State state)
{
    switch (state) {
    case Outbox::State::Queued:
        return QStringLiteral("queued");
    case Outbox::State::Sending:
        return QStringLiteral("sending");
    case Outbox::State::Submitted:
        return QStringLiteral("submitted");
    case Outbox::State::Interrupted:
        return QStringLiteral("interrupted");
    case Outbox::State::Refused:
        return QStringLiteral("refused");
    }
    return QString();
}

bool stateFromName(const QString &name, Outbox::State *state)
{
    for (auto candidate : {Outbox::State::Queued, Outbox::State::Sending, Outbox::State::Submitted,
             Outbox::State::Interrupted, Outbox::State::Refused}) {
        if (stateName(candidate) == name) {
            *state = candidate;
            return true;
        }
    }
    return false;
}

QString dateText(const QDateTime &date)
{
    return date.isValid() ? date.toString(Qt::ISODateWithMs) : QString();
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

bool readFile(const QString &path, QByteArray *data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    *data = file.readAll();
    return true;
}

} // namespace

Outbox::Outbox(const QString &folder, const QString &draftsFolder, QObject *parent)
    : QObject(parent)
    , m_folder(folder)
    , m_draftsFolder(draftsFolder)
    , m_backoff(&Outbox::defaultBackoff)
    , m_attemptTimeout(defaultAttemptTimeout)
    , m_retryTimer(new QTimer(this))
    , m_watchdog(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &Outbox::pump);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, &Outbox::onTimeout);
    load();
}

Outbox::~Outbox()
{
    if (m_msa) {
        // Whatever the server answers now is not seen; the next start treats the
        // message as interrupted
        m_msa->disconnect(this);
    }
}

QString Outbox::folder() const
{
    return m_folder;
}

QString Outbox::draftsFolder() const
{
    return m_draftsFolder;
}

qint64 Outbox::defaultBackoff(int attempts)
{
    constexpr qint64 minute = 60 * 1000;
    constexpr qint64 longest = 30 * minute;
    if (attempts < 1)
        return 0;
    return std::min(longest, minute << std::min(attempts - 1, 5));
}

void Outbox::setBackoff(const std::function<qint64(int)> &backoff)
{
    m_backoff = backoff ? backoff : std::function<qint64(int)>(&Outbox::defaultBackoff);
}

void Outbox::setAttemptTimeout(int milliseconds)
{
    m_attemptTimeout = milliseconds;
}

void Outbox::setTransport(const QString &accountId, const Transport &transport)
{
    m_transports.insert(accountId, transport);
    schedulePump();
}

void Outbox::removeTransport(const QString &accountId)
{
    m_transports.remove(accountId);
}

QString Outbox::entryFolder(const QString &id) const
{
    return m_folder + QLatin1Char('/') + id;
}

int Outbox::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id)
            return i;
    }
    return -1;
}

int Outbox::count() const
{
    return m_entries.size();
}

int Outbox::count(const QString &accountId) const
{
    return std::count_if(m_entries.cbegin(), m_entries.cend(),
        [&accountId](const Entry &entry) { return entry.accountId == accountId; });
}

QList<Outbox::Entry> Outbox::entries() const
{
    return m_entries;
}

Outbox::Entry Outbox::entry(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? Entry() : m_entries.at(i);
}

bool Outbox::isBusy(const QString &id) const
{
    return !id.isEmpty() && id == m_current;
}

bool Outbox::hasWorkDue(const QString &accountId) const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const Entry &entry : m_entries) {
        if (entry.accountId != accountId)
            continue;
        if (entry.id == m_current)
            return true;
        if (entry.state != State::Queued && entry.state != State::Submitted)
            continue;
        if (entry.nextAttempt.isValid() && entry.nextAttempt > now)
            continue;
        const auto transport = m_transports.constFind(accountId);
        if (transport != m_transports.constEnd() && (!transport->online || transport->online()))
            return true;
    }
    return false;
}

void Outbox::load()
{
    const QDir root(m_folder);
    if (!root.exists())
        return;
    const QStringList folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &id : folders) {
        const QString folder = root.filePath(id);
        QByteArray json;
        if (!QFile::exists(folder + QLatin1Char('/') + entryFile)) {
            // A message whose recording did not finish was never accepted; its
            // composer still had it
            QDir(folder).removeRecursively();
            continue;
        }
        if (!readFile(folder + QLatin1Char('/') + entryFile, &json, nullptr)
            || !QFile::exists(folder + QLatin1Char('/') + messageFile)) {
            qWarning("Outbox: skipping the unreadable message %s", qPrintable(id));
            continue;
        }
        const QJsonObject object = QJsonDocument::fromJson(json).object();
        Entry entry;
        entry.id = id;
        entry.accountId = object.value(QLatin1String("account")).toString();
        if (entry.accountId.isEmpty()
            || !stateFromName(object.value(QLatin1String("state")).toString(), &entry.state)) {
            qWarning("Outbox: skipping the unreadable message %s", qPrintable(id));
            continue;
        }
        entry.subject = object.value(QLatin1String("subject")).toString();
        entry.from = object.value(QLatin1String("from")).toString().toUtf8();
        const QJsonArray recipients = object.value(QLatin1String("recipients")).toArray();
        for (const QJsonValue &recipient : recipients)
            entry.recipients.append(recipient.toString().toUtf8());
        entry.queued = QDateTime::fromString(object.value(QLatin1String("queued")).toString(), Qt::ISODateWithMs);
        entry.timestamp = QDateTime::fromString(object.value(QLatin1String("timestamp")).toString(), Qt::ISODateWithMs);
        entry.sentFolder = object.value(QLatin1String("sentFolder")).toString();
        entry.attempts = object.value(QLatin1String("attempts")).toInt();
        entry.nextAttempt
            = QDateTime::fromString(object.value(QLatin1String("nextAttempt")).toString(), Qt::ISODateWithMs);
        entry.lastError = object.value(QLatin1String("lastError")).toString();
        entry.hasDraft = QFile::exists(folder + QLatin1Char('/') + draftFile);
        if (entry.state == State::Sending) {
            entry.state = State::Interrupted;
            entry.lastError = tr("The application ended while sending this message, "
                                 "so it may or may not have "
                                 "been sent.");
            save(entry);
        }
        m_entries.append(entry);
    }
    std::stable_sort(m_entries.begin(), m_entries.end(),
        [](const Entry &a, const Entry &b) { return a.queued != b.queued ? a.queued < b.queued : a.id < b.id; });
}

bool Outbox::save(const Entry &entry, QString *error)
{
    QJsonArray recipients;
    for (const QByteArray &recipient : entry.recipients)
        recipients.append(QString::fromUtf8(recipient));
    QJsonObject object{
        {QLatin1String("account"), entry.accountId},
        {QLatin1String("state"), stateName(entry.state)},
        {QLatin1String("subject"), entry.subject},
        {QLatin1String("from"), QString::fromUtf8(entry.from)},
        {QLatin1String("recipients"), recipients},
        {QLatin1String("queued"), dateText(entry.queued)},
        {QLatin1String("timestamp"), dateText(entry.timestamp)},
        {QLatin1String("sentFolder"), entry.sentFolder},
        {QLatin1String("attempts"), entry.attempts},
        {QLatin1String("nextAttempt"), dateText(entry.nextAttempt)},
        {QLatin1String("lastError"), entry.lastError},
    };
    return writeFile(entryFolder(entry.id) + QLatin1Char('/') + entryFile, QJsonDocument(object).toJson(), error);
}

QString Outbox::enqueue(const Message &message, QString *error)
{
    Entry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.accountId = message.accountId;
    entry.subject = message.subject;
    entry.from = message.from;
    entry.recipients = message.recipients;
    entry.queued = QDateTime::currentDateTimeUtc();
    entry.timestamp = message.timestamp;
    entry.sentFolder = message.sentFolder;
    entry.hasDraft = !message.draft.isEmpty();

    const QString folder = entryFolder(entry.id);
    QString problem;
    bool ok = Common::PrivateFiles::makeFolder(m_folder, &problem) && Common::PrivateFiles::makeFolder(folder, &problem)
        && writeFile(folder + QLatin1Char('/') + messageFile, message.data, &problem)
        && (message.sentCopy.isEmpty() || writeFile(folder + QLatin1Char('/') + sentFile, message.sentCopy, &problem))
        && (message.draft.isEmpty() || writeFile(folder + QLatin1Char('/') + draftFile, message.draft, &problem))
        // The entry exists once its state is written
        && save(entry, &problem);
    if (!ok) {
        QDir(folder).removeRecursively();
        if (error)
            *error = tr("Cannot keep the message in the outbox: %1").arg(problem);
        return QString();
    }
    m_entries.append(entry);
    emit changed();
    schedulePump();
    return entry.id;
}

void Outbox::wake(const QString &accountId)
{
    for (Entry &entry : m_entries) {
        if ((accountId.isEmpty() || entry.accountId == accountId) && entry.nextAttempt.isValid()
            && (entry.state == State::Queued || entry.state == State::Submitted) && entry.id != m_current) {
            entry.nextAttempt = QDateTime();
            save(entry);
        }
    }
    schedulePump();
}

bool Outbox::sendNow(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0 || isBusy(id))
        return false;
    const State state = m_entries.at(i).state;
    if (state != State::Queued && state != State::Submitted)
        return false;
    // The account's earlier messages go first
    for (int j = 0; j <= i; ++j) {
        Entry &entry = m_entries[j];
        if (entry.accountId == m_entries.at(i).accountId && (j == i || entry.state == State::Queued)
            && entry.nextAttempt.isValid()) {
            entry.nextAttempt = QDateTime();
            save(entry);
        }
    }
    emit changed();
    schedulePump();
    return true;
}

bool Outbox::sendAgain(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0)
        return false;
    Entry &entry = m_entries[i];
    if (entry.state != State::Interrupted && entry.state != State::Refused)
        return false;
    entry.state = State::Queued;
    entry.attempts = 0;
    entry.nextAttempt = QDateTime();
    entry.lastError.clear();
    save(entry);
    emit changed();
    schedulePump();
    return true;
}

QString Outbox::moveToDrafts(const QString &id, QString *error)
{
    const int i = indexOf(id);
    if (i < 0 || isBusy(id)) {
        if (error)
            *error = tr("The message is being sent right now.");
        return QString();
    }
    const Entry entry = m_entries.at(i);
    if (entry.state == State::Submitted) {
        if (error)
            *error = tr("The server already accepted this message.");
        return QString();
    }
    const QString path = writeDraft(entry, entry.state == State::Refused ? entry.lastError : QString(), error);
    if (path.isEmpty())
        return QString();
    removeEntry(id);
    emit changed();
    return path;
}

bool Outbox::forget(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0 || isBusy(id) || m_entries.at(i).state != State::Submitted)
        return false;
    removeEntry(id);
    emit changed();
    return true;
}

void Outbox::schedulePump()
{
    if (m_pumpScheduled)
        return;
    m_pumpScheduled = true;
    QMetaObject::invokeMethod(this, &Outbox::pump, Qt::QueuedConnection);
}

void Outbox::pump()
{
    m_pumpScheduled = false;
    if (!m_current.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime earliest;
    QSet<QString> accountsWithQueued;
    for (int i = 0; i < m_entries.size(); ++i) {
        Entry &entry = m_entries[i];
        const bool queued = entry.state == State::Queued;
        if (!queued && entry.state != State::Submitted)
            continue;
        if (queued) {
            // Messages leave in the order they were written
            if (accountsWithQueued.contains(entry.accountId))
                continue;
            accountsWithQueued.insert(entry.accountId);
        }
        const auto transport = m_transports.constFind(entry.accountId);
        if (transport == m_transports.constEnd() || (queued && !transport->createMsa)
            || (transport->online && !transport->online()))
            continue;
        if (entry.nextAttempt.isValid() && entry.nextAttempt > now) {
            if (!earliest.isValid() || entry.nextAttempt < earliest)
                earliest = entry.nextAttempt;
            continue;
        }
        m_retryTimer->stop();
        if (queued)
            startSending(entry);
        else
            startFiling(entry);
        return;
    }
    if (earliest.isValid()) {
        m_retryTimer->start(
            static_cast<int>(std::clamp<qint64>(now.msecsTo(earliest), 0, std::numeric_limits<int>::max())));
    }
}

void Outbox::startSending(Entry &entry)
{
    const QString id = entry.id;
    const QByteArray from = entry.from;
    const QList<QByteArray> recipients = entry.recipients;
    const Transport transport = m_transports.value(entry.accountId);
    QByteArray data;
    QString problem;
    if (!readFile(entryFolder(id) + QLatin1Char('/') + messageFile, &data, &problem)) {
        retryLater(entry, tr("Cannot read the message: %1").arg(problem), false);
        return;
    }
    // Once the server may have the message, it is never handed over again without
    // asking
    entry.state = State::Sending;
    if (!save(entry, &problem)) {
        entry.state = State::Queued;
        retryLater(entry, tr("Cannot record that the message is being sent: %1").arg(problem), false);
        return;
    }
    m_current = id;
    m_currentFiling = false;
    const quint64 serial = ++m_serial;
    emit sending(id);
    emit changed();

    MSA::AbstractMSA *msa = transport.createMsa(this);
    if (!msa) {
        onError(serial, tr("The account has no outgoing mail server set up."), false);
        return;
    }
    m_msa = msa;
    connect(msa, &MSA::AbstractMSA::sent, this, [this, serial]() { onSent(serial); });
    connect(msa, &MSA::AbstractMSA::error, this,
        [this, serial, msa](const QString &message) { onError(serial, message, msa->isPermanentFailure()); });
    connect(msa, &MSA::AbstractMSA::passwordRequested, this,
        [this, serial, transport](const QString &user, const QString &host) {
            if (serial != m_serial || m_current.isEmpty())
                return;
            const QString missing = tr("The outgoing server asks for a password, and none is stored. Send "
                                       "the message from the composer to enter it.");
            if (!transport.password) {
                failCurrent(missing);
                return;
            }
            QPointer<Outbox> self(this);
            transport.password(user, host, [self, serial, missing](const QString &password) {
                if (!self || serial != self->m_serial || self->m_current.isEmpty() || !self->m_msa)
                    return;
                if (password.isNull())
                    self->failCurrent(missing);
                else
                    self->m_msa->setPassword(password);
            });
        });
    connect(msa, &MSA::AbstractMSA::certificateCheckRequested, this,
        [this, serial, transport](const QList<QSslCertificate> &chain, const QList<QSslError> &errors) {
            if (serial != m_serial || m_current.isEmpty() || !m_msa)
                return;
            m_msa->setCertificateAccepted(transport.certificate && transport.certificate(chain, errors));
        });
    m_watchdog->start(m_attemptTimeout);
    msa->sendMail(from, recipients, data);
}

void Outbox::startFiling(Entry &entry)
{
    const QString id = entry.id;
    m_current = id;
    m_currentFiling = true;
    const quint64 serial = ++m_serial;
    const Transport transport = m_transports.value(entry.accountId);
    if (entry.sentFolder.isEmpty() || !transport.fileSentCopy) {
        complete(id);
        return;
    }
    const QString folder = entry.sentFolder;
    const QDateTime timestamp = entry.timestamp;
    QByteArray data;
    QString problem;
    const QString copy = entryFolder(id) + QLatin1Char('/') + sentFile;
    if (!readFile(QFile::exists(copy) ? copy : entryFolder(id) + QLatin1Char('/') + messageFile, &data, &problem)) {
        retryLater(entry, tr("Cannot read the copy for %1: %2").arg(folder, problem), true);
        return;
    }
    emit changed();
    m_watchdog->start(m_attemptTimeout);
    QPointer<Outbox> self(this);
    transport.fileSentCopy(folder, data, timestamp, [self, serial](bool ok, const QString &error) {
        if (self)
            self->onFiled(serial, ok, error);
    });
}

void Outbox::releaseMsa()
{
    if (!m_msa)
        return;
    m_msa->disconnect(this);
    m_msa->deleteLater();
    m_msa.clear();
}

void Outbox::onSent(quint64 serial)
{
    if (serial != m_serial || m_current.isEmpty() || m_currentFiling)
        return;
    m_watchdog->stop();
    releaseMsa();
    const int i = indexOf(m_current);
    if (i < 0) {
        finish();
        return;
    }
    Entry &entry = m_entries[i];
    entry.state = State::Submitted;
    entry.attempts = 0;
    entry.nextAttempt = QDateTime();
    entry.lastError.clear();
    // Should this fail, the next start finds the message interrupted, which never
    // sends it again unasked
    save(entry);
    startFiling(entry);
}

void Outbox::onError(quint64 serial, const QString &message, bool permanent)
{
    if (serial != m_serial || m_current.isEmpty() || m_currentFiling)
        return;
    m_watchdog->stop();
    releaseMsa();
    const int i = indexOf(m_current);
    if (i < 0) {
        finish();
        return;
    }
    if (permanent)
        refuse(m_entries[i], message);
    else
        retryLater(m_entries[i], message, false);
}

void Outbox::onFiled(quint64 serial, bool ok, const QString &error)
{
    if (serial != m_serial || m_current.isEmpty() || !m_currentFiling)
        return;
    m_watchdog->stop();
    if (ok) {
        complete(m_current);
        return;
    }
    const int i = indexOf(m_current);
    if (i < 0) {
        finish();
        return;
    }
    retryLater(m_entries[i],
        tr("The message was sent, but filing its copy in %1 failed: %2").arg(m_entries.at(i).sentFolder, error), true);
}

void Outbox::failCurrent(const QString &error)
{
    if (m_current.isEmpty() || m_currentFiling)
        return;
    QPointer<MSA::AbstractMSA> msa = m_msa;
    const quint64 serial = m_serial;
    releaseMsa();
    if (msa)
        msa->cancel();
    onError(serial, error, false);
}

void Outbox::onTimeout()
{
    if (m_current.isEmpty())
        return;
    const QString id = m_current;
    const bool filing = m_currentFiling;
    QPointer<MSA::AbstractMSA> msa = m_msa;
    releaseMsa();
    if (msa)
        msa->cancel();
    const int i = indexOf(id);
    if (i < 0) {
        finish();
        return;
    }
    Entry &entry = m_entries[i];
    if (filing) {
        retryLater(entry, tr("Filing the copy in %1 took too long.").arg(entry.sentFolder), true);
        return;
    }
    entry.state = State::Interrupted;
    entry.lastError = tr("The server did not answer in time, so the message may "
                         "or may not have been sent.");
    save(entry);
    finish();
    emit interrupted(id);
    emit changed();
}

void Outbox::retryLater(Entry &entry, const QString &error, bool filing)
{
    entry.attempts += 1;
    entry.nextAttempt = QDateTime::currentDateTimeUtc().addMSecs(m_backoff(entry.attempts));
    entry.lastError = error;
    if (!filing)
        entry.state = State::Queued;
    save(entry);
    const QString id = entry.id;
    finish();
    emit failed(id, error, filing);
    emit changed();
}

void Outbox::finish()
{
    m_current.clear();
    m_currentFiling = false;
    m_watchdog->stop();
    schedulePump();
}

void Outbox::complete(QString id)
{
    removeEntry(id);
    finish();
    emit sent(id);
    emit changed();
}

void Outbox::refuse(Entry &entry, const QString &reason)
{
    QString problem;
    const QString path = writeDraft(entry, reason, &problem);
    if (path.isEmpty()) {
        entry.state = State::Refused;
        entry.lastError = reason;
        save(entry);
        const Entry kept = entry;
        finish();
        emit refused(kept, reason, QString());
        emit changed();
        return;
    }
    const Entry removed = entry;
    removeEntry(entry.id);
    finish();
    emit refused(removed, reason, path);
    emit changed();
}

QString Outbox::writeDraft(const Entry &entry, const QString &reason, QString *error) const
{
    QString problem;
    QByteArray data;
    const QString source = entryFolder(entry.id) + QLatin1Char('/') + (entry.hasDraft ? draftFile : messageFile);
    if (!Common::PrivateFiles::makeFolder(m_draftsFolder, &problem) || !readFile(source, &data, &problem)) {
        if (error)
            *error = tr("Cannot save the draft: %1").arg(problem);
        return QString();
    }
    const QString extension = entry.hasDraft ? QStringLiteral(".draft") : QStringLiteral(".eml");
    QString base = Common::PrivateFiles::safeFileName(
        (entry.subject.trimmed().isEmpty() ? tr("Untitled") : entry.subject.trimmed()) + extension);
    if (base.endsWith(extension))
        base.chop(extension.size());
    const QDir drafts(m_draftsFolder);
    QString path = drafts.filePath(base + extension);
    for (int n = 2; QFile::exists(path) || QFile::exists(path + QLatin1String(reasonSuffix)); ++n)
        path = drafts.filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(extension));
    if (!writeFile(path, data, &problem)
        || (!reason.isEmpty() && !writeFile(path + QLatin1String(reasonSuffix), reason.toUtf8(), &problem))) {
        QFile::remove(path);
        if (error)
            *error = tr("Cannot save the draft: %1").arg(problem);
        return QString();
    }
    return path;
}

void Outbox::removeEntry(const QString &id)
{
    const int i = indexOf(id);
    if (i >= 0)
        m_entries.removeAt(i);
    QDir(entryFolder(id)).removeRecursively();
}

} // namespace Composer
