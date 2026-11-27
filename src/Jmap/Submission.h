/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef JMAP_SUBMISSION_H
#define JMAP_SUBMISSION_H

#include <functional>

#include <QPointer>

#include "Jmap/Client.h"
#include "MSA/AbstractMSA.h"

namespace Jmap {

/**
 * @short Sends a message through a JMAP server (RFC 8621, section 7)
 *
 * The message is uploaded, stored in the Sent mailbox (or Drafts on a server without one), and then submitted with
 * the sending identity that matches its sender. When the server refuses to send it, the stored copy is removed
 * again, so Sent holds only mail that left.
 */
class Submission : public MSA::AbstractMSA
{
    Q_OBJECT
public:
    Submission(const Settings &settings, Client::TrustCheck trustCheck, QObject *parent);
    ~Submission() override;

    void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data) override;

    /** The message stored on the server, once it is known */
    QString emailId() const;

public slots:
    void cancel() override;
    void setPassword(const QString &password) override;
    void setCertificateAccepted(bool accepted) override;

private:
    void begin();
    void onSession();
    void store(const QString &mailboxId, const QString &identityId);
    void submit(const QString &emailId, const QString &identityId);
    void discard(const QString &emailId, const QString &message);
    void onFailed(Client::Failure failure, const QString &message);
    void finishWithError(const QString &message);

    Settings m_settings;
    Client::TrustCheck m_trustCheck;
    QPointer<Client> m_client;
    QByteArray m_from;
    QList<QByteArray> m_to;
    QByteArray m_data;
    QString m_emailId;
    QList<QSslError> m_acceptedErrors;
    QList<QSslError> m_rejectedErrors;
    bool m_done = false;
};

/** @short Creates a Submission for each message; @arg sent runs after each message was sent */
class SubmissionFactory : public MSA::MSAFactory
{
public:
    SubmissionFactory(const Settings &settings, Client::TrustCheck trustCheck = {}, std::function<void()> sent = {});
    ~SubmissionFactory() override;
    MSA::AbstractMSA *create(QObject *parent) const override;

private:
    Settings m_settings;
    Client::TrustCheck m_trustCheck;
    std::function<void()> m_sent;
};

}

#endif
