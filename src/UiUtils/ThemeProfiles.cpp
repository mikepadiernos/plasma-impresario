/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/ThemeProfiles.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace UiUtils {

namespace ThemeProfiles {

namespace {

QString tr(const char *text, const char *comment = nullptr)
{
    return QCoreApplication::translate("UiUtils::ThemeProfiles", text, comment);
}

/** @short Options whose Plasma Opus names differ, as Plasma Impresario names
 * them */
const QHash<QString, QString> &opusNames()
{
    static const QHash<QString, QString> names{
        {QStringLiteral("MessageListFont"), QStringLiteral("FileListFont")},
        {QStringLiteral("StripeRows"), QStringLiteral("StripeDetailsRows")},
    };
    return names;
}

/** @short Options only a mail client has, which a profile for Plasma Opus
 * leaves out */
QStringList mailOnly()
{
    return {QStringLiteral("MessageListStyle"), QStringLiteral("StripeFolderTree"), QStringLiteral("MessageHeaderFont"),
            QStringLiteral("PlainTextFont")};
}

QString nameFor(const QString &entry, Target target)
{
    return target == Target::Opus ? opusNames().value(entry, entry) : entry;
}

QString entryFor(const QString &name, bool fromOpus)
{
    if (fromOpus) {
        for (auto it = opusNames().constBegin(); it != opusNames().constEnd(); ++it) {
            if (it.value() == name)
                return it.key();
        }
        if (opusNames().contains(name) || mailOnly().contains(name))
            return {};
    }
    return name;
}

bool hasControlCharacter(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar ch) {
        return ch.category() == QChar::Other_Control || ch == QChar::LineSeparator || ch == QChar::ParagraphSeparator;
    });
}

QJsonValue encode(const Theme::Option &option, const QVariant &value)
{
    switch (option.type) {
    case Theme::Type::Bool:
        return value.toBool();
    case Theme::Type::Int:
        return value.toInt();
    case Theme::Type::Color:
        return value.value<QColor>().name(QColor::HexRgb);
    case Theme::Type::StringList:
        return QJsonArray::fromStringList(value.toStringList());
    case Theme::Type::String:
    case Theme::Type::Choice:
        break;
    }
    return value.toString();
}

std::optional<QVariant> decode(const Theme::Option &option, const QJsonValue &json)
{
    QVariant value;
    switch (option.type) {
    case Theme::Type::Bool:
        if (!json.isBool())
            return std::nullopt;
        value = json.toBool();
        break;
    case Theme::Type::Int: {
        if (!json.isDouble())
            return std::nullopt;
        const double number = json.toDouble();
        if (!std::isfinite(number) || std::trunc(number) != number || std::abs(number) > 1e9)
            return std::nullopt;
        value = static_cast<int>(number);
        break;
    }
    case Theme::Type::StringList: {
        if (!json.isArray())
            return std::nullopt;
        QStringList list;
        for (const QJsonValue &item : json.toArray()) {
            if (!item.isString())
                return std::nullopt;
            list.append(item.toString());
        }
        value = list;
        break;
    }
    case Theme::Type::Color:
    case Theme::Type::String:
    case Theme::Type::Choice:
        if (!json.isString())
            return std::nullopt;
        value = json.toString();
        break;
    }
    return Theme::validate(option.entry, value);
}

QJsonObject encodeValues(const Theme::Values &values, Target target, bool withStyleSheet)
{
    QJsonObject appearance;
    for (const QString &entry : Theme::profileEntries()) {
        if (target == Target::Opus && mailOnly().contains(entry))
            continue;
        if (!withStyleSheet && Theme::styleSheetEntries().contains(entry))
            continue;
        const Theme::Option *option = Theme::option(entry);
        appearance.insert(nameFor(entry, target), encode(*option, Theme::value(values, entry)));
    }
    return appearance;
}

std::optional<Theme::Values> decodeValues(const QJsonValue &json, bool fromOpus, QString *error, QStringList *warnings)
{
    if (!json.isObject()) {
        *error = tr("The theme profile has no appearance options.");
        return std::nullopt;
    }
    const QJsonObject appearance = json.toObject();
    const QStringList allowed = Theme::profileEntries();
    Theme::Values values = Theme::defaults();
    for (const QString &entry : Theme::entries()) {
        if (!allowed.contains(entry))
            values.remove(entry);
    }
    for (auto it = appearance.constBegin(); it != appearance.constEnd(); ++it) {
        const QString entry = entryFor(it.key(), fromOpus);
        if (entry.isEmpty() || !allowed.contains(entry)) {
            if (warnings)
                warnings->append(tr("Skipped the unknown option %1.").arg(it.key().left(MaximumNameLength)));
            continue;
        }
        const std::optional<QVariant> value = decode(*Theme::option(entry), it.value());
        if (!value) {
            *error = tr("The theme profile value %1 is not valid.").arg(it.key());
            return std::nullopt;
        }
        values.insert(entry, *value);
    }
    return values;
}

QJsonObject document(const Profile &profile, Target target, bool withStyleSheet)
{
    QJsonObject object;
    object.insert(QStringLiteral("format"), QLatin1String(target == Target::Opus ? OpusFormat : Format));
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("name"), profile.name.trimmed());
    object.insert(QStringLiteral("appearance"), encodeValues(profile.values, target, withStyleSheet));
    return object;
}

} // namespace

QString nameError(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return tr("Enter a name for the theme profile.");
    if (trimmed.size() > MaximumNameLength)
        return tr("Theme profile names can have at most %1 characters.").arg(MaximumNameLength);
    if (hasControlCharacter(trimmed))
        return tr("Theme profile names cannot contain line breaks or control "
                  "characters.");
    return {};
}

QByteArray toJson(const Profile &profile, Target target, bool withStyleSheet)
{
    return QJsonDocument(document(profile, target, withStyleSheet)).toJson(QJsonDocument::Indented);
}

ParseResult fromJson(const QByteArray &json)
{
    ParseResult result;
    if (json.size() > MaximumFileSize) {
        result.error = tr("The theme profile is larger than %1 KiB and was not read.").arg(MaximumFileSize / 1024);
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject object = doc.object();
    const QString format = object.value(QStringLiteral("format")).toString();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject() ||
        (format != QLatin1String(Format) && format != QLatin1String(OpusFormat))) {
        result.error = tr("The file is not a Plasma Impresario or Plasma Opus theme profile.");
        return result;
    }
    result.fromOpus = format == QLatin1String(OpusFormat);
    const QJsonValue version = object.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != std::trunc(version.toDouble()) || version.toDouble() < 1) {
        result.error = tr("The theme profile has no valid format version.");
        return result;
    }
    if (version.toDouble() > Version) {
        result.error = tr("The theme profile uses format version %1, newer than "
                          "the version %2 this version reads.")
                           .arg(static_cast<qint64>(version.toDouble()))
                           .arg(Version);
        return result;
    }
    Profile profile;
    profile.name = object.value(QStringLiteral("name")).toString().trimmed();
    if (profile.name.isEmpty())
        profile.name = tr("Imported theme");
    if (const QString error = nameError(profile.name); !error.isEmpty()) {
        result.error = error;
        return result;
    }
    const std::optional<Theme::Values> values =
        decodeValues(object.value(QStringLiteral("appearance")), result.fromOpus, &result.error, &result.warnings);
    if (!values)
        return result;
    profile.values = *values;
    result.profile = profile;
    return result;
}

ParseResult importFile(const QString &path)
{
    ParseResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = tr("Could not open %1: %2").arg(path, file.errorString());
        return result;
    }
    if (file.size() > MaximumFileSize) {
        result.error = tr("The theme profile is larger than %1 KiB and was not read.").arg(MaximumFileSize / 1024);
        return result;
    }
    return fromJson(file.read(MaximumFileSize + 1));
}

QString exportFile(const Profile &profile, const QString &path, Target target, bool withStyleSheet)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson(profile, target, withStyleSheet)) < 0 || !file.commit())
        return tr("Could not write %1: %2").arg(path, file.errorString());
    return {};
}

QStringList store(const QList<Profile> &profiles)
{
    QStringList result;
    for (const Profile &profile : profiles) {
        QJsonObject object = document(profile, Target::Impresario, true);
        object.insert(QStringLiteral("id"), profile.id);
        result.append(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }
    return result;
}

QList<Profile> restore(const QStringList &stored, QStringList *problems)
{
    QList<Profile> profiles;
    QSet<QString> ids;
    for (const QString &item : stored) {
        ParseResult parsed = fromJson(item.toUtf8());
        const QString id = QJsonDocument::fromJson(item.toUtf8()).object().value(QStringLiteral("id")).toString();
        if (!parsed.profile || id.isEmpty() || ids.contains(id)) {
            if (problems)
                problems->append(parsed.error.isEmpty() ? tr("A saved theme profile could not be read.")
                                                        : parsed.error);
            continue;
        }
        parsed.profile->id = id;
        ids.insert(id);
        profiles.append(*parsed.profile);
        if (profiles.size() == MaximumProfiles)
            break;
    }
    return profiles;
}

QString listError(const QList<Profile> &profiles)
{
    if (profiles.size() > MaximumProfiles)
        return tr("At most %1 theme profiles can be saved.").arg(MaximumProfiles);
    QSet<QString> names;
    for (const Profile &profile : profiles) {
        if (const QString error = nameError(profile.name); !error.isEmpty())
            return error;
        const QString key = profile.name.trimmed().toCaseFolded();
        if (names.contains(key))
            return tr("There is already a theme profile called %1.").arg(profile.name.trimmed());
        names.insert(key);
    }
    return {};
}

QString newId(const QList<Profile> &profiles)
{
    for (;;) {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!find(profiles, id))
            return id;
    }
}

qsizetype indexOfName(const QList<Profile> &profiles, const QString &name)
{
    const QString key = name.trimmed().toCaseFolded();
    for (qsizetype i = 0; i < profiles.size(); ++i) {
        if (profiles.at(i).name.trimmed().toCaseFolded() == key)
            return i;
    }
    return -1;
}

const Profile *find(const QList<Profile> &profiles, const QString &id)
{
    for (const Profile &profile : profiles) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

} // namespace ThemeProfiles

} // namespace UiUtils
