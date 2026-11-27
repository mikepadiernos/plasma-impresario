/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/NewMailSettings.h"

#include <KConfigGroup>

namespace UiUtils {

namespace NewMailSettings {

using NewMail::Mode;

namespace {

const char groupName[] = "Notifications";
const char seenGroupName[] = "NewMailSeen";

QString accountGroupName(const QString &accountId)
{
    return QStringLiteral("Account ") + accountId;
}

QString modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Each:
        return QStringLiteral("each");
    case Mode::Summary:
        return QStringLiteral("summary");
    case Mode::Off:
        return QStringLiteral("off");
    }
    return QStringLiteral("each");
}

Mode modeFromKey(const QString &key)
{
    if (key == QLatin1String("summary"))
        return Mode::Summary;
    if (key == QLatin1String("off"))
        return Mode::Off;
    return Mode::Each;
}

/** The state entry of one folder: the account and the folder, which may contain any character, made safe */
QString seenKey(const QString &accountId, const QString &mailbox)
{
    return accountId + QLatin1Char(' ') + QString::fromLatin1(mailbox.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
}

}

NewMail::Settings load(const KConfigBase &config)
{
    NewMail::Settings settings;
    const KConfigGroup group = config.group(QLatin1String(groupName));
    settings.quietWhileActive = group.readEntry("QuietWhileActive", settings.quietWhileActive);
    settings.showContent = group.readEntry("ShowContent", settings.showContent);
    settings.summaryMinutes = std::clamp(group.readEntry("SummaryMinutes", settings.summaryMinutes), 1, 24 * 60);
    const QStringList groups = group.groupList();
    for (const QString &name : groups) {
        if (!name.startsWith(QLatin1String("Account ")))
            continue;
        const QString accountId = name.mid(8);
        const KConfigGroup account = group.group(name);
        const QString mode = account.readEntry("Mode", QString());
        if (!mode.isEmpty() && modeFromKey(mode) != Mode::Each)
            settings.accountModes.insert(accountId, modeFromKey(mode));
        const QStringList on = account.readEntry("AnnouncedFolders", QStringList());
        const QStringList off = account.readEntry("QuietFolders", QStringList());
        for (const QString &folder : on)
            settings.setAnnounces(accountId, folder, true);
        for (const QString &folder : off)
            settings.setAnnounces(accountId, folder, false);
    }
    return settings;
}

void save(KConfigBase &config, const NewMail::Settings &settings)
{
    KConfigGroup group = config.group(QLatin1String(groupName));
    group.writeEntry("QuietWhileActive", settings.quietWhileActive);
    group.writeEntry("ShowContent", settings.showContent);
    group.writeEntry("SummaryMinutes", settings.summaryMinutes);
    const QStringList old = group.groupList();
    for (const QString &name : old) {
        if (name.startsWith(QLatin1String("Account ")))
            group.deleteGroup(name);
    }
    QStringList accounts = settings.accountModes.keys() + settings.folders.keys();
    accounts.removeDuplicates();
    for (const QString &accountId : std::as_const(accounts)) {
        KConfigGroup account = group.group(accountGroupName(accountId));
        if (settings.mode(accountId) != Mode::Each)
            account.writeEntry("Mode", modeKey(settings.mode(accountId)));
        QStringList on;
        QStringList off;
        const auto folders = settings.folders.value(accountId);
        for (auto it = folders.constBegin(); it != folders.constEnd(); ++it)
            (it.value() ? on : off).append(it.key());
        std::sort(on.begin(), on.end());
        std::sort(off.begin(), off.end());
        if (!on.isEmpty())
            account.writeEntry("AnnouncedFolders", on);
        if (!off.isEmpty())
            account.writeEntry("QuietFolders", off);
    }
}

void removeAccount(KConfigBase &config, const QString &accountId)
{
    NewMail::Settings settings = load(config);
    settings.accountModes.remove(accountId);
    settings.folders.remove(accountId);
    save(config, settings);
}

bool renameFolder(KConfigBase &config, const QString &accountId, const QString &oldName, const QString &newName)
{
    NewMail::Settings settings = load(config);
    if (!settings.hasFolderSetting(accountId, oldName))
        return false;
    const bool announce = settings.announces(accountId, oldName);
    settings.folders[accountId].remove(oldName);
    settings.setAnnounces(accountId, newName, announce);
    save(config, settings);
    return true;
}

NewMailWatcher::Baseline loadBaseline(const KConfigBase &state, const QString &accountId, const QString &mailbox)
{
    const QStringList values = state.group(QLatin1String(seenGroupName)).readEntry(seenKey(accountId, mailbox), QStringList());
    NewMailWatcher::Baseline baseline;
    if (values.size() != 3)
        return baseline;
    baseline.uidValidity = values.at(0).toUInt();
    baseline.highestUid = values.at(1).toUInt();
    baseline.lastCheck = QDateTime::fromString(values.at(2), Qt::ISODate);
    if (!baseline.isValid())
        return {};
    return baseline;
}

void saveBaseline(KConfigBase &state, const QString &accountId, const QString &mailbox,
                  const NewMailWatcher::Baseline &baseline)
{
    KConfigGroup group = state.group(QLatin1String(seenGroupName));
    if (!baseline.isValid()) {
        group.deleteEntry(seenKey(accountId, mailbox));
        return;
    }
    group.writeEntry(seenKey(accountId, mailbox),
                     QStringList{QString::number(baseline.uidValidity), QString::number(baseline.highestUid),
                                 baseline.lastCheck.toUTC().toString(Qt::ISODate)});
}

void removeBaselines(KConfigBase &state, const QString &accountId)
{
    KConfigGroup group = state.group(QLatin1String(seenGroupName));
    const QStringList keys = group.keyList();
    for (const QString &key : keys) {
        if (key.startsWith(accountId + QLatin1Char(' ')))
            group.deleteEntry(key);
    }
}

}

}
