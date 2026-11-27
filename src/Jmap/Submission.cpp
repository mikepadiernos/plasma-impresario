/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Jmap/Submission.h"

namespace Jmap {

namespace {

QString bareAddress(const QByteArray &address)
{
    QString text = QString::fromUtf8(address).trimmed();
    const int open = text.lastIndexOf(QLatin1Char('<'));
    const int close = text.lastIndexOf(QLatin1Char('>'));
    if (open >= 0 && close > open)
        text = text.mid(open + 1, close - open - 1);
    return text.trimmed();
}

}

Submission::Submission(const Settings &settings, Client::TrustCheck trustCheck, QObject *parent)
    : AbstractMSA(parent)
    , m_settings(settings)
    , m_trustCheck(std::move(trustCheck))
{
}

Submission::~Submission()
{
    if (m_client)
        m_client->abort();
}

QString Submission::emailId() const
{
    return m_emailId;
}

void Submission::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data)
{
    m_from = from;
    m_to = to;
    m_data = data;
    emit progressMax(4);
    emit progress(0);
    emit connecting();
    if (m_settings.password.isEmpty())
        emit passwordRequested(m_settings.user, m_settings.host());
    else
        begin();
}

void Submission::setPassword(const QString &password)
{
    m_settings.password = password;
    begin();
}

void Submission::cancel()
{
    m_done = true;
    if (m_client)
        m_client->abort();
}

void Submission::setCertificateAccepted(bool accepted)
{
    if (!accepted) {
        finishWithError(tr("The certificate of the JMAP server was not accepted, so the message was not sent."));
        return;
    }
    m_acceptedErrors = m_rejectedErrors;
    begin();
}

void Submission::begin()
{
    if (m_done)
        return;
    if (m_client) {
        m_client->abort();
        m_client->deleteLater();
    }
    m_client = new Client(m_settings, this);
    m_client->setTrustCheck(m_trustCheck);
    m_client->setAcceptedErrors(m_acceptedErrors);
    connect(m_client, &Client::certificateRejected, this, [this](const QList<QSslCertificate> &chain, const QList<QSslError> &errors) {
        m_rejectedErrors = errors;
        emit certificateCheckRequested(chain, errors);
    });
    connect(m_client, &Client::failed, this, &Submission::onFailed);
    m_client->fetchSession([this]() { onSession(); });
}

void Submission::onFailed(Client::Failure failure, const QString &message)
{
    // A certificate question is pending; its answer goes on or ends the attempt
    if (failure == Client::Failure::Certificate && !m_rejectedErrors.isEmpty())
        return;
    if (!m_emailId.isEmpty()) {
        // The server may have sent it before the connection broke, so its copy in Sent stays
        finishWithError(tr("%1 The message may have been sent; check the Sent folder before sending it again.").arg(message));
        return;
    }
    finishWithError(message);
}

void Submission::finishWithError(const QString &message)
{
    if (m_done)
        return;
    m_done = true;
    emit error(message);
}

void Submission::onSession()
{
    const Session &session = m_client->session();
    if (!session.canSubmit) {
        finishWithError(tr("The JMAP server does not send mail for this account. Choose SMTP for sending in the account "
                           "settings."));
        return;
    }
    emit progress(1);
    const QJsonObject account{{QStringLiteral("accountId"), session.accountId}};
    QJsonObject mailboxes = account;
    mailboxes.insert(QStringLiteral("ids"), QJsonValue::Null);
    mailboxes.insert(QStringLiteral("properties"), QJsonArray{QStringLiteral("id"), QStringLiteral("role")});
    m_client->call(QJsonArray{Client::method(QStringLiteral("Mailbox/get"), mailboxes, QStringLiteral("m")),
                              Client::method(QStringLiteral("Identity/get"), account, QStringLiteral("i"))},
                   [this](const QJsonArray &responses) {
        QString error;
        QString sent, drafts;
        for (const QJsonValue &value : Client::response(responses, QStringLiteral("m"), &error).value(QLatin1String("list")).toArray()) {
            const QString role = value.toObject().value(QLatin1String("role")).toString().toLower();
            if (role == QLatin1String("sent"))
                sent = value.toObject().value(QLatin1String("id")).toString();
            else if (role == QLatin1String("drafts"))
                drafts = value.toObject().value(QLatin1String("id")).toString();
        }
        if (sent.isEmpty())
            sent = drafts;
        if (sent.isEmpty()) {
            finishWithError(tr("The JMAP server has no Sent folder to keep the message in."));
            return;
        }
        const QString from = bareAddress(m_from);
        const QString domain = from.section(QLatin1Char('@'), -1).toLower();
        QString identity;
        for (const QJsonValue &value : Client::response(responses, QStringLiteral("i"), &error).value(QLatin1String("list")).toArray()) {
            const QString email = value.toObject().value(QLatin1String("email")).toString();
            const QString id = value.toObject().value(QLatin1String("id")).toString();
            if (email.compare(from, Qt::CaseInsensitive) == 0) {
                identity = id;
                break;
            }
            if (identity.isEmpty() && email.startsWith(QLatin1String("*@")) && email.mid(2).toLower() == domain)
                identity = id;
        }
        if (identity.isEmpty()) {
            finishWithError(tr("The JMAP server may not send as %1. Add this address as a sending identity on the "
                               "server, or choose another sender.").arg(from));
            return;
        }
        store(sent, identity);
    });
}

void Submission::store(const QString &mailboxId, const QString &identityId)
{
    emit sending();
    m_client->upload(m_data, "message/rfc822", [this, mailboxId, identityId](const QString &blobId) {
        emit progress(2);
        const QJsonObject email{{QStringLiteral("blobId"), blobId},
                                {QStringLiteral("mailboxIds"), QJsonObject{{mailboxId, true}}},
                                {QStringLiteral("keywords"), QJsonObject{{QStringLiteral("$seen"), true}}}};
        const QJsonObject arguments{{QStringLiteral("accountId"), m_client->session().accountId},
                                    {QStringLiteral("emails"), QJsonObject{{QStringLiteral("m"), email}}}};
        m_client->call(QJsonArray{Client::method(QStringLiteral("Email/import"), arguments, QStringLiteral("i"))},
                       [this, identityId](const QJsonArray &responses) {
            QString error;
            const QJsonObject result = Client::response(responses, QStringLiteral("i"), &error);
            const QString id = result.value(QLatin1String("created")).toObject().value(QLatin1String("m")).toObject()
                                   .value(QLatin1String("id")).toString();
            if (id.isEmpty()) {
                const QJsonObject refused = result.value(QLatin1String("notCreated")).toObject().value(QLatin1String("m")).toObject();
                const QString reason = refused.value(QLatin1String("description")).toString(
                    refused.value(QLatin1String("type")).toString(error));
                finishWithError(tr("The JMAP server did not accept the message: %1").arg(reason));
                return;
            }
            m_emailId = id;
            submit(id, identityId);
        });
    });
}

void Submission::submit(const QString &emailId, const QString &identityId)
{
    emit progress(3);
    QJsonArray recipients;
    for (const QByteArray &recipient : std::as_const(m_to))
        recipients.append(QJsonObject{{QStringLiteral("email"), bareAddress(recipient)}});
    const QJsonObject envelope{{QStringLiteral("mailFrom"), QJsonObject{{QStringLiteral("email"), bareAddress(m_from)}}},
                               {QStringLiteral("rcptTo"), recipients}};
    const QJsonObject submission{{QStringLiteral("identityId"), identityId},
                                 {QStringLiteral("emailId"), emailId},
                                 {QStringLiteral("envelope"), envelope}};
    const QJsonObject arguments{{QStringLiteral("accountId"), m_client->session().accountId},
                                {QStringLiteral("create"), QJsonObject{{QStringLiteral("s"), submission}}}};
    m_client->call(QJsonArray{Client::method(QStringLiteral("EmailSubmission/set"), arguments, QStringLiteral("s"))},
                   [this, emailId](const QJsonArray &responses) {
        QString error;
        const QJsonObject result = Client::response(responses, QStringLiteral("s"), &error);
        if (result.value(QLatin1String("created")).toObject().contains(QLatin1String("s"))) {
            m_emailId = emailId;
            m_done = true;
            emit progress(4);
            emit sent();
            return;
        }
        const QJsonObject refused = result.value(QLatin1String("notCreated")).toObject().value(QLatin1String("s")).toObject();
        const QString reason = refused.value(QLatin1String("description")).toString(refused.value(QLatin1String("type")).toString(error));
        m_emailId.clear();
        discard(emailId, tr("The JMAP server did not send the message: %1").arg(reason));
    });
}

void Submission::discard(const QString &emailId, const QString &message)
{
    // Sent keeps only mail that left; the outcome of this clean-up does not change the error shown
    const QJsonObject arguments{{QStringLiteral("accountId"), m_client->session().accountId},
                                {QStringLiteral("destroy"), QJsonArray{emailId}}};
    disconnect(m_client, &Client::failed, this, &Submission::onFailed);
    connect(m_client, &Client::failed, this, [this, message]() { finishWithError(message); });
    m_client->call(QJsonArray{Client::method(QStringLiteral("Email/set"), arguments, QStringLiteral("d"))},
                   [this, message](const QJsonArray &) { finishWithError(message); });
}

SubmissionFactory::SubmissionFactory(const Settings &settings, Client::TrustCheck trustCheck, std::function<void()> sent)
    : m_settings(settings)
    , m_trustCheck(std::move(trustCheck))
    , m_sent(std::move(sent))
{
}

SubmissionFactory::~SubmissionFactory() = default;

MSA::AbstractMSA *SubmissionFactory::create(QObject *parent) const
{
    auto *submission = new Submission(m_settings, m_trustCheck, parent);
    if (m_sent)
        QObject::connect(submission, &MSA::AbstractMSA::sent, submission, m_sent);
    return submission;
}

}
