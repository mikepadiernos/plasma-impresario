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
#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <memory>
#include <QSet>

#include <QList>
#include <QMap>
#include <QPersistentModelIndex>
#include <QUrl>
#include <QPointer>
#include <QSslCertificate>
#include <QSslError>
#include <QWidget>

#include "Common/Logging.h"
#include "Composer/Recipients.h"
#include "Composer/MessageComposer.h"
#include "Nextcloud/Files.h"
#include "Plugins/AddressbookPlugin.h"


namespace Ui
{
class ComposeWidget;
}

class QAbstractListModel;
class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSettings;
class QToolButton;

namespace Composer {
class SenderIdentitiesModel;
class Submission;
}

namespace Imap {
class ImapAccess;
namespace Mailbox {
class Model;
}
}

namespace MSA {
class MSAFactory;
}

namespace Gui
{

class MainWindow;

class InhibitComposerDirtying;

class ComposerSaveState;

class InlineNotificationBar;
class RemoteAttachmentFetcher;

/** @short A "Compose New Mail..." dialog

  Implements a widget which can act as a standalone window for composing e-mail messages.
  Uses Imap::Mailbox::MessageComposer as a backend for composing a message. @see Imap::Mailbox::MessageComposer

  */
class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    ~ComposeWidget() override;
    static ComposeWidget *warnIfMsaNotConfigured(ComposeWidget *widget, MainWindow *mainWindow);
    static ComposeWidget *createDraft(MainWindow *mainWindow, const QString &path);
    static ComposeWidget *createBlank(MainWindow *mainWindow);
    /** Who opened a mailto: URL */
    enum class UrlOrigin {
        /** A link in a message or a panel; it may not attach files */
        ThisApplication,
        /** Another application, such as a file manager handing over files through the e-mail portal */
        OtherApplication,
    };
    /** @short Compose from a mailto: @arg url; from another application, also attach the files it and @arg files request

    Requested files are listed in a notice above the message, together with any that could not be attached and why.
    */
    static ComposeWidget *createFromUrl(MainWindow *mainWindow, const QUrl &url, UrlOrigin origin = UrlOrigin::ThisApplication,
                                        const QStringList &files = {});
    static ComposeWidget *createReply(MainWindow *mainWindow, const Composer::ReplyMode &mode, const QModelIndex &replyingToMessage,
                                      const QList<QPair<Composer::RecipientKind, QString> > &recipients, const QString &subject,
                                      const QString &body, const QList<QByteArray> &inReplyTo, const QList<QByteArray> &references);
    static ComposeWidget *createForward(MainWindow *mainWindow, const Composer::ForwardMode mode, const QModelIndex &forwardingMessage,
                                        const QString &subject, const QList<QByteArray> &inReplyTo, const QList<QByteArray> &references);
    static ComposeWidget *createFromReadOnly(MainWindow *mainWindow, const QModelIndex &messageRoot, const QList<QPair<Composer::RecipientKind, QString>> &recipients);
    void placeOnMainWindow();
    /** @short The account this message is sent from */
    QString accountId() const;
    /** @short Send this message from @p accountId, with one of its identities; false when that account is unavailable */
    bool setAccount(const QString &accountId);
    /** @short The account the draft at @p path was written from, or an empty string for older drafts */
    static QString draftAccountId(const QString &path);
    /** @short Save changes made since the last save to the draft now, without asking

    Used when the desktop session ends, so that the next start recovers them. Messages that were sent or never edited
    are not saved.
    */
    void saveDraftNow();
protected:
    void changeEvent(QEvent *e) override;
    void closeEvent(QCloseEvent *ce) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private slots:
    void calculateMaxVisibleRecipients();
    void collapseRecipients();
    void completeRecipient(QAction *act);
    void completeRecipients(const QString &text);
    void send();
    void gotError(const QString &error);
    /** @short The submission failed; offer to keep the message in the outbox unless the server refused it */
    void submissionFailed(const QString &error);
    void sent();
    void updateRecipientList();
    void scrollRecipients(int);
    void gotoNextInputLineFrom(QWidget *w);
    void handleFocusChange();
    void scrollToFocus();
    void slotFadeFinished();

    void slotCheckAddressOfSender();
    void slotCheckAddress(QLineEdit *edit);

    void slotAskForFileAttachment();
    void slotAttachFiles(QList<QUrl> urls);
    /** @short Choose files in Nextcloud, to attach them or to insert links to them */
    void attachFromNextcloud();

    void slotUpdateSignature();
    void updateWindowTitle();

    void autoSaveDraft();
    void setMessageUpdated();

    void setUiWidgetsEnabled(const bool enabled);

    void onCompletionAvailable(const Plugins::NameEmailList &completion);
    void onCompletionFailed(Plugins::AddressbookJob::Error error);

    void passwordRequested(const QString &user, const QString &host);
    void passwordError();
    void certificateCheckRequested(const QList<QSslCertificate> &chain, const QList<QSslError> &errors);
    void toggleReplyMarking();
    void updateReplyMarkingAction();
    void updateReplyMode();
    void markReplyModeHandpicked();
    void updateAccountWarning();

signals:
    void logged(const Common::LogKind kind, const QString& source, const QString& message);

private:
    void attachNextcloudFiles(const QString &accountId, const QList<Nextcloud::Entry> &files);
    void insertNextcloudLinks(const QString &accountId, const QList<Nextcloud::Entry> &files);

    ComposeWidget(MainWindow *mainWindow, std::shared_ptr<Composer::AbstractComposer> messageComposer, MSA::MSAFactory *msaFactory);
    void setResponseData(const QList<QPair<Composer::RecipientKind, QString> > &recipients, const QString &subject,
                         const QString &body, const QList<QByteArray> &inReplyTo, const QList<QByteArray> &references,
                         const QModelIndex &replyingToMessage);
    bool setReplyMode(const Composer::ReplyMode mode);
    QList<Imap::Message::MailAddress> ownLoginAddresses() const;

    static QByteArray extractMailAddress(const QString &text, bool &ok);
    static Composer::RecipientKind recipientKindForNextRow(const Composer::RecipientKind kind);
    void addRecipient(int position, Composer::RecipientKind kind, const QString &address);
    bool parseRecipients(QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > &results, QString &errorMessage);
    void removeRecipient(int position);
    void fadeIn(QWidget *w);
    void askPassword(const QString &user, const QString &host);

    void createSubmission(Imap::Mailbox::Model *model, const QString &credentialAccountId);
    void populateAccounts();
    QString senderProblem() const;
    QString noOutgoingServerText() const;
    QString removedAccountText() const;

    bool buildMessageData();
    bool shouldBuildMessageLocally() const;

    void saveDraft(const QString &path);
    /** @short The draft as saveDraft() writes it, marked as explicitly saved or not by @p explicitDraft */
    QByteArray draftData(bool explicitDraft);
    void loadDraft(const QString &path);
    /** @short The folder which keeps a copy of what this account sends, or an empty string when none is kept here */
    QString sentCopyFolder() const;
    /** @short Queue the message in the outbox, with a Sent copy when @p fileCopy, and close; returns whether queued */
    bool queueInOutbox(bool fileCopy);

    std::shared_ptr<Composer::MessageComposer> interactiveComposer();
    /** Attach the files another application asked for and list them, and those refused, in the notice */
    void attachRequestedFiles(const QStringList &fromUrl, const QStringList &fromCommandLine);

    Ui::ComposeWidget *ui;
    QPushButton *sendButton;
    QPushButton *cancelButton;
    QToolButton *m_markButton;
    QActionGroup *m_markAsReply;
    QAction *m_actionStandalone;
    QAction *m_actionInReplyTo;
    QAction *m_actionToggleMarking;
    QToolButton *m_replyModeButton;
    QActionGroup *m_replyModeActions;
    QAction *m_actionHandPickedRecipients;
    QAction *m_actionReplyModePrivate;
    QAction *m_actionReplyModeAllButMe;
    QAction *m_actionReplyModeAll;
    QAction *m_actionReplyModeList;
    typedef QPair<QComboBox*, QLineEdit*> Recipient;
    QList<Recipient> m_recipients;
    QTimer *m_recipientListUpdateTimer;
    QPointer<QWidget> m_lastFocusedRecipient;
    int m_maxVisibleRecipients;

    bool m_sentMail;
    bool m_explicitDraft;
    QString m_autoSavePath;

    QList<QByteArray> m_inReplyTo;
    QList<QByteArray> m_references;
    QPersistentModelIndex m_replyingToMessage;

    std::unique_ptr<ComposerSaveState> m_saveState;

    bool m_appendUidReceived;
    uint m_appendUidValidity;
    uint m_appendUid;
    bool m_genUrlAuthReceived;
    QString m_urlauth;

    MainWindow *m_mainWindow;
    QSettings *m_settings;
    /** The account which sends the message: the one this composer opened for, or the one the user chose */
    QString m_accountId;
    /** Settings of the sending account */
    QPointer<QSettings> m_accountSettings;
    /** Connection of the sending account */
    QPointer<Imap::ImapAccess> m_imapAccess;
    /** Identities of the sending account */
    Composer::SenderIdentitiesModel *m_senderIdentities;
    /** How the sending account submits messages; empty when it has no outgoing server set up */
    std::unique_ptr<MSA::MSAFactory> m_msaFactory;
    /** The model whose messages this composer replies to, forwards or attaches */
    QPointer<Imap::Mailbox::Model> m_sourceModel;
    /** The sender saved with a loaded draft */
    QString m_draftSender;
    /** Why the message is not sent from the account it was written from */
    QString m_accountProblem;
    /** Why the server refused this draft when it was sent from the outbox */
    QString m_refusalProblem;
    QComboBox *m_account;
    QLabel *m_accountWarning;
    /** Reports the files another application asked to attach */
    InlineNotificationBar *m_handoffNotice;
    /** Reports the files being fetched from other computers to be attached */
    InlineNotificationBar *m_fetchNotice;
    RemoteAttachmentFetcher *m_remoteFetcher = nullptr;
    QPushButton *m_nextcloudButton = nullptr;
    /** The private copies of the files still being downloaded from Nextcloud */
    QSet<QString> m_nextcloudCopies;

    std::shared_ptr<Composer::AbstractComposer> m_composer;
    Composer::Submission *m_submission;

    QMenu *m_completionPopup;
    QLineEdit *m_completionReceiver;
    int m_completionCount;

    QMap<QLineEdit *, Plugins::AddressbookJob *> m_firstCompletionRequests;
    QMap<QLineEdit *, Plugins::AddressbookJob *> m_secondCompletionRequests;

    friend class InhibitComposerDirtying;
    friend class ComposerSaveState;

    ComposeWidget(const ComposeWidget &); // don't implement
    ComposeWidget &operator=(const ComposeWidget &); // don't implement
};

}

#endif // COMPOSEWIDGET_H
