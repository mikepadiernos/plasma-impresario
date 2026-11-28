/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef COMPOSER_SUBMISSION_H
#define COMPOSER_SUBMISSION_H

#include <memory>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>

#include "Common/Logging.h"
#include "Composer/Recipients.h"

namespace Imap {
namespace Mailbox {
class ImapTask;
class Model;
}
}

namespace MSA {
class AbstractMSA;
class MSAFactory;
}

namespace Composer {

class AbstractComposer;

/** @short Handle submission of an e-mail via multiple ways

This class uses the MessageComposer for modelling a message and an MSA implementation for the actual submission.
The whole process is (trying to be) interruptable once started.
*/
class Submission : public QObject
{
    Q_OBJECT
public:
    explicit Submission(QObject *parent, std::shared_ptr<AbstractComposer> composer, Imap::Mailbox::Model *model, MSA::MSAFactory *msaFactory, const QString &accountId);
    ~Submission() override;

    QString accountId() const;

    void setImapOptions(const bool saveToSentFolder, const QString &sentFolderName,
                        const QString &hostname, const QString &username, const bool useImapSubmit);
    void setSmtpOptions(const bool useBurl, const QString &smtpUsername);
    /** @short Whether the message refers to messages of another account than the one it is sent from

    Such a message is always built locally, as references to another server's messages cannot be used by CATENATE.
    */
    void setForeignSource(const bool foreign);

    void send();

    /** @short Whether the copy for the Sent folder has already been stored */
    bool sentCopyFiled() const;
    /** @short Whether the server permanently refused the message, so retrying it cannot help */
    bool refused() const;

    /** @short Progress of the current submission */
    enum SubmissionProgress {
        STATE_INIT, /**< Nothing is happening yet */
        STATE_BUILDING_MESSAGE, /**< Waiting for data to become available */
        STATE_SAVING, /**< Saving the message to the Sent folder */
        STATE_PREPARING_URLAUTH, /**< Making the resulting message available via IMAP's URLAUTH */
        STATE_SUBMITTING, /**< Submitting the message via an MSA */
        STATE_UPDATING_FLAGS, /**< Updating flags of the relevant message(s) */
        STATE_SENT, /**< All done, succeeded */
        STATE_FAILED /**< Unable to send */
    };

public slots:
    void setPassword(const QString &password);
    void cancelPassword();
    void setCertificateAccepted(bool accepted);

private slots:
    void gotError(const QString &error);
    void sent();

    void slotAppendUidKnown(const uint uidValidity, const uint uid);
    void slotGenUrlAuthReceived(const QString &url);
    void slotAppendSucceeded();
    void slotAppendFailed(const QString &error);
    void onUpdatingFlagsOfReplyingToSucceded();
    void onUpdatingFlagsOfReplyingToFailed();
    void onUpdatingFlagsOfForwardingSucceeded();
    void onUpdatingFlagsOfForwardingFailed();

    void slotMessageDataAvailable();
    void slotAskForUrl();
    void slotInvokeMsaNow();

    void onMsaProgressMaxChanged(const int max);
    void onMsaProgressCurrentChanged(const int value);

signals:
    void progressMin(const int min);
    void progressMax(const int max);
    void progress(const int progress);
    void updateStatusMessage(const QString &message);
    void failed(const QString &message);
    void succeeded();
    void passwordRequested(const QString &user, const QString &host);
    void gotPassword(const QString &password);
    /** @short The MSA's server certificate is not trusted; answer with setCertificateAccepted() */
    void certificateCheckRequested(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);
    void gotCertificateDecision(bool accepted);
    void canceled();
    void logged(const Common::LogKind kind, const QString& source, const QString& message);

private:
    bool shouldBuildMessageLocally() const;
    bool canCatenate() const;
    /** @short The model of @p message, which may belong to another account than the one the message is sent from */
    Imap::Mailbox::Model *modelOf(const QModelIndex &message) const;

    static QString killDomainPartFromString(const QString &s);

    void changeConnectionState(const SubmissionProgress state);

    bool m_appendUidReceived;
    uint m_appendUidValidity;
    uint m_appendUid;
    bool m_genUrlAuthReceived;
    QString m_urlauth;
    bool m_saveToSentFolder;
    QString m_sentFolderName;
    QString m_imapHostname;
    QString m_imapUsername;
    bool m_useBurl;
    QString m_smtpUsername;
    bool m_useImapSubmit;
    bool m_foreignSource = false;
    bool m_sentCopyFiled = false;
    bool m_refused = false;
    QPointer<MSA::AbstractMSA> m_msa;

    SubmissionProgress m_state;
    QByteArray m_rawMessageData;
    int m_msaMaximalProgress;

    std::shared_ptr<AbstractComposer> m_source;
    QPointer<Imap::Mailbox::Model> m_model;
    MSA::MSAFactory *m_msaFactory;
    QString m_accountId;

    Imap::Mailbox::ImapTask *m_updateReplyingToMessageFlagsTask;
    Imap::Mailbox::ImapTask *m_updateForwardingMessageFlagsTask;

    Submission(const Submission &); // don't implement
    Submission &operator=(const Submission &); // don't implement
};

QString submissionProgressToString(const Submission::SubmissionProgress progress);

}

#endif // COMPOSER_SUBMISSION_H
