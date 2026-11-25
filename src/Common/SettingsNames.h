/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>

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
#ifndef SETTINGSNAMES_H
#define SETTINGSNAMES_H

#include <QString>

namespace Common
{

struct SettingsNames {
    static const QString identitiesKey, realNameKey, addressKey, organisationKey, signatureKey, obsRealNameKey, obsAddressKey;
    /** @short Per-account flag: the account sends without an identity */
    static const QString composerNoIdentityKey;
    static const QString favoriteTagsKey, tagNameKey, tagColorKey;
    static const QString msaMethodKey, methodSMTP, methodSSMTP, methodSENDMAIL, methodImapSendmail, smtpHostKey,
           smtpPortKey, smtpAuthKey, smtpStartTlsKey, smtpUserKey, smtpAuthReuseImapCredsKey, smtpSslPemPubKey,
           sendmailKey, sendmailDefaultCmd;
    static const QString imapMethodKey, methodTCP, methodSSL, methodProcess, imapHostKey,
           imapPortKey, imapStartTlsKey, imapUserKey, imapProcessKey, imapStartMode, netOffline, netExpensive, netOnline,
           obsImapStartOffline, obsImapSslPemCertificate, imapSslPemPubKey,
           imapBlacklistedCapabilities, imapUseSystemProxy, imapNeedsNetwork, imapNumberRefreshInterval,
           imapAccountIcon, imapArchiveFolderName, imapDefaultArchiveFolderName, imapDeleteMovesToTrash,
           imapRecentDestinationFolders;
    /** @short Per-account authentication method for IMAP and SMTP, and the account's OAuth 2.0 configuration */
    static const QString imapAuthMethodKey, smtpAuthMethodKey, authMethodPassword, authMethodOAuth,
           oauthProviderKey, oauthClientIdKey, oauthClientSecretKey;
    /** @short Profile-wide list of accepted server keys, as "host sha256-of-public-key" */
    static const QString sslTrustedPublicKeys;
    static const QString composerSaveToImapKey, composerImapSentKey, smtpUseBurlKey, composerUseSharedIdentitiesKey;
    static const QString cacheMetadataKey, cacheMetadataMemory,
           cacheOfflineKey, cacheOfflineNone, cacheOfflineXDays, cacheOfflineAll, cacheOfflineNumberDaysKey;
    static const QString watchedFoldersKey, watchOnlyInbox, watchSubscribed, watchAll;
    static const QString guiMsgListShowThreading;
    static const QString guiMsgListHideRead;
    /** @short Quick filters of the message list, stored per account */
    static const QString guiMsgListFilterUnread, guiMsgListFilterFlagged, guiMsgListFilterAttachments;
    static const QString guiMsgListGroupBy;
    static const QString guiMailboxListShowOnlySubscribed;
    static const QString guiPreferPlaintextRendering;
    static const QString guiMainWindowLayout, guiMainWindowLayoutCompact, guiMainWindowLayoutWide, guiMainWindowLayoutOneAtTime;
    static const QString geometry;
    static const QString windowState;
    static const QString mainVSplitterState;
    static const QString mainHSplitterState;
    static const QString messageListState;
    static const QString messageListHeader;
    static const QString columnWidth;
    static const QString menubarVisible;
    static const QString guiSizesInMainWinWhenCompact, guiSizesInMainWinWhenWide, guiSizesInaMainWinWhenOneAtATime;
    static const QString guiAllowRawSearch;
    static const QString guiExpandedMailboxes;
    static const QString guiShowSystray, guiOnSystrayClose, guiStartMinimized;
    static const QString knownEmailsKey;
    static const QString addressbookPlugin, passwordPlugin, spellcheckerPlugin;
    static const QString imapIdleRenewal;
    static const QString autoMarkReadEnabled, autoMarkReadSeconds;
    static const QString interopRevealVersions;
    static const QString completeMessageWidgetGeometry;
    static const QString mboxDropAction;
    static const QString msgViewColorScheme;
    static const QString settingsVersion;
};

}

#endif // SETTINGSNAMES_H
