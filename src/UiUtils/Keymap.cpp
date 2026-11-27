/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "UiUtils/Keymap.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>

namespace UiUtils {

namespace Keymap {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("UiUtils::Keymap", text);
}

const char formatName[] = "plasma-keymap";

QList<Command> makeCommands()
{
    const Contexts all = Context::FolderTree | Context::MessageList | Context::Reader;
    const Contexts mail = Context::MessageList | Context::Reader;
    auto shared = [](const char *id, const char *keys, Contexts contexts) {
        return Command{QLatin1String(id), QLatin1String(keys), contexts, true, QString()};
    };
    auto local = [](const char *id, const char *keys, Contexts contexts, const char *action) {
        return Command{QLatin1String(id), QLatin1String(keys), contexts, false, QLatin1String(action)};
    };
    return {
        shared("move.down", "j", all),
        shared("move.up", "k", all),
        shared("move.top", "gg", all),
        shared("move.bottom", "G", all),
        shared("move.out", "h", all),
        shared("move.in", "l", all),
        shared("page.down", "<C-d>", all),
        shared("page.up", "<C-u>", all),
        shared("search", "/", all),
        shared("select.toggle", "x", Context::MessageList),
        shared("tab.next", "gt", all),
        shared("tab.previous", "gT", all),
        shared("help", "?", all),
        local("message.reply", "r", mail, "action_reply_private"),
        local("message.replyAll", "R", mail, "action_reply_all"),
        local("message.forward", "f", mail, "action_forward_attachment"),
        local("message.delete", "dd", mail, "action_mark_as_deleted"),
        local("message.archive", "a", mail, "action_archive"),
        local("message.toggleRead", "u", mail, "action_mark_as_read"),
        local("message.toggleFlag", "s", mail, "action_mark_as_flagged"),
        local("message.nextUnread", "n", all, "action_go_to_next_unread"),
        local("message.previousUnread", "N", all, "action_go_to_previous_unread"),
        local("message.move", "mv", mail, "action_move_to_folder"),
        local("mail.compose", "c", all, "action_compose_mail"),
        local("go.inbox", "gi", all, ""),
    };
}

QString ctrlToken(QChar letter)
{
    return QStringLiteral("<C-%1>").arg(letter.toLower());
}

}

const QList<Command> &commands()
{
    static const QList<Command> all = makeCommands();
    return all;
}

const Command *command(const QString &id)
{
    for (const Command &cmd : commands()) {
        if (cmd.id == id)
            return &cmd;
    }
    return nullptr;
}

QString description(const QString &id)
{
    static const QHash<QString, const char *> texts = {
        {QStringLiteral("move.down"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Move down")},
        {QStringLiteral("move.up"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Move up")},
        {QStringLiteral("move.top"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Go to the first item")},
        {QStringLiteral("move.bottom"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Go to the last item")},
        {QStringLiteral("move.out"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Go to the previous pane or collapse")},
        {QStringLiteral("move.in"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Open, expand or go to the next pane")},
        {QStringLiteral("page.down"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Scroll half a page down")},
        {QStringLiteral("page.up"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Scroll half a page up")},
        {QStringLiteral("search"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Search")},
        {QStringLiteral("select.toggle"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Select or deselect the message")},
        {QStringLiteral("tab.next"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Next tab")},
        {QStringLiteral("tab.previous"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Previous tab")},
        {QStringLiteral("help"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Show the keyboard shortcuts")},
        {QStringLiteral("message.reply"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Reply")},
        {QStringLiteral("message.replyAll"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Reply to all")},
        {QStringLiteral("message.forward"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Forward")},
        {QStringLiteral("message.delete"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Delete")},
        {QStringLiteral("message.archive"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Archive")},
        {QStringLiteral("message.toggleRead"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Mark as read or unread")},
        {QStringLiteral("message.toggleFlag"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Flag or unflag")},
        {QStringLiteral("message.nextUnread"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Next unread message")},
        {QStringLiteral("message.previousUnread"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Previous unread message")},
        {QStringLiteral("message.move"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Move to a folder")},
        {QStringLiteral("mail.compose"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Write a new message")},
        {QStringLiteral("go.inbox"), QT_TRANSLATE_NOOP("UiUtils::Keymap", "Go to the inbox")},
    };
    const auto it = texts.constFind(id);
    return it == texts.constEnd() ? id : QCoreApplication::translate("UiUtils::Keymap", it.value());
}

Bindings defaultBindings()
{
    Bindings result;
    for (const Command &cmd : commands())
        result.insert(cmd.id, cmd.defaultKeys);
    return result;
}

Bindings merged(const Bindings &overrides)
{
    Bindings result = defaultBindings();
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
        if (result.contains(it.key()) && (it.value().isEmpty() || isValid(it.value())))
            result.insert(it.key(), it.value());
    }
    return result;
}

Bindings overrides(const Bindings &bindings)
{
    Bindings result;
    const Bindings defaults = defaultBindings();
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        if (defaults.contains(it.key()) && defaults.value(it.key()) != it.value())
            result.insert(it.key(), it.value());
    }
    return result;
}

QStringList tokens(const QString &keys)
{
    QStringList result;
    for (qsizetype i = 0; i < keys.size(); ++i) {
        const QChar ch = keys.at(i);
        if (ch == QLatin1Char('<')) {
            const qsizetype end = keys.indexOf(QLatin1Char('>'), i);
            if (end < 0)
                return {};
            const QString name = keys.mid(i, end - i + 1);
            const bool ctrl = name.size() == 5 && name.startsWith(QLatin1String("<C-")) && name.at(3).isLetter()
                && name.at(3).isLower();
            if (!ctrl && name != QLatin1String("<Esc>"))
                return {};
            result << name;
            i = end;
        } else if (ch.isSpace() || !ch.isPrint()) {
            return {};
        } else {
            result << QString(ch);
        }
    }
    return result;
}

bool isValid(const QString &keys)
{
    return !tokens(keys).isEmpty();
}

QString token(int key, Qt::KeyboardModifiers modifiers, const QString &text)
{
    if (key == Qt::Key_Escape)
        return QStringLiteral("<Esc>");
    if (modifiers & (Qt::AltModifier | Qt::MetaModifier))
        return QString();
    if (modifiers & Qt::ControlModifier) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
            return ctrlToken(QLatin1Char(char('a' + (key - Qt::Key_A))));
        return QString();
    }
    if (text.size() != 1 || !text.at(0).isPrint() || text.at(0).isSpace())
        return QString();
    return text;
}

QList<Conflict> conflicts(const Bindings &bindings)
{
    QList<Conflict> result;
    QMap<QString, QStringList> byKeys;
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        if (!it.value().isEmpty() && command(it.key()))
            byKeys[it.value()] << it.key();
    }
    auto overlap = [](const QString &a, const QString &b) {
        return bool(command(a)->contexts & command(b)->contexts);
    };
    for (auto it = byKeys.cbegin(); it != byKeys.cend(); ++it) {
        QStringList clashing;
        for (const QString &a : it.value()) {
            for (const QString &b : it.value()) {
                if (a != b && overlap(a, b) && !clashing.contains(a))
                    clashing << a;
            }
        }
        if (clashing.size() > 1) {
            clashing.sort();
            result.append({Conflict::Kind::Same, it.key(), clashing});
        }
    }
    for (auto shorter = byKeys.cbegin(); shorter != byKeys.cend(); ++shorter) {
        const QStringList shortTokens = tokens(shorter.key());
        for (auto longer = byKeys.cbegin(); longer != byKeys.cend(); ++longer) {
            const QStringList longTokens = tokens(longer.key());
            if (longTokens.size() <= shortTokens.size() || longTokens.mid(0, shortTokens.size()) != shortTokens)
                continue;
            QStringList clashing;
            for (const QString &a : shorter.value()) {
                for (const QString &b : longer.value()) {
                    if (overlap(a, b)) {
                        if (!clashing.contains(a))
                            clashing << a;
                        if (!clashing.contains(b))
                            clashing << b;
                    }
                }
            }
            if (!clashing.isEmpty()) {
                clashing.sort();
                result.append({Conflict::Kind::Prefix, longer.key(), clashing});
            }
        }
    }
    return result;
}

QStringList shadowedShortcuts(const Bindings &bindings, const QList<QKeySequence> &shortcuts)
{
    QSet<QString> singleKeys;
    for (const QKeySequence &sequence : shortcuts) {
        if (sequence.count() != 1)
            continue;
        const QKeyCombination combination = sequence[0];
        const Qt::Key key = combination.key();
        const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
        if (modifiers & ~Qt::ShiftModifier)
            continue;
        QString text;
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            const QChar letter = QLatin1Char(char('a' + (key - Qt::Key_A)));
            text = modifiers & Qt::ShiftModifier ? QString(letter.toUpper()) : QString(letter);
        } else if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde && !(modifiers & Qt::ShiftModifier)) {
            text = QString(QLatin1Char(char(key)));
        }
        if (!text.isEmpty())
            singleKeys.insert(text);
    }
    QStringList result;
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        const QStringList keys = tokens(it.value());
        if (!keys.isEmpty() && singleKeys.contains(keys.first()))
            result << it.key();
    }
    return result;
}

Matcher::Matcher(const Bindings &bindings)
    : m_bindings(bindings)
{
}

void Matcher::setBindings(const Bindings &bindings)
{
    m_bindings = bindings;
    reset();
}

Matcher::Result Matcher::feed(const QString &token, Context context)
{
    m_matched.clear();
    if (token == QLatin1String("<Esc>")) {
        const bool wasPending = !m_pending.isEmpty();
        reset();
        return wasPending ? Result::Cancelled : Result::None;
    }
    QStringList typed = m_pending;
    typed << token;
    QString exact;
    bool longer = false;
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        const Command *cmd = command(it.key());
        if (!cmd || !cmd->contexts.testFlag(context))
            continue;
        const QStringList keys = tokens(it.value());
        if (keys.size() < typed.size() || keys.mid(0, typed.size()) != typed)
            continue;
        if (keys.size() == typed.size())
            exact = it.key();
        else
            longer = true;
    }
    if (!exact.isEmpty()) {
        // A complete sequence wins over longer ones which start with it; conflicts() reports those
        m_pending.clear();
        m_matched = exact;
        return Result::Matched;
    }
    if (longer) {
        m_pending = typed;
        return Result::Pending;
    }
    m_pending.clear();
    return Result::None;
}

void Matcher::reset()
{
    m_pending.clear();
    m_matched.clear();
}

QString Matcher::matched() const
{
    return m_matched;
}

QString Matcher::pending() const
{
    return m_pending.join(QString());
}

QList<Matcher::Continuation> Matcher::continuations(Context context) const
{
    QList<Continuation> result;
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        const Command *cmd = command(it.key());
        if (!cmd || !cmd->contexts.testFlag(context))
            continue;
        const QStringList keys = tokens(it.value());
        if (keys.size() <= m_pending.size() || keys.mid(0, m_pending.size()) != m_pending)
            continue;
        result.append({keys.mid(m_pending.size()).join(QString()), it.key()});
    }
    std::sort(result.begin(), result.end(), [](const Continuation &a, const Continuation &b) {
        return a.keys != b.keys ? a.keys < b.keys : a.command < b.command;
    });
    return result;
}

Bindings sharedOnly(const Bindings &bindings)
{
    Bindings result;
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
        const Command *cmd = command(it.key());
        if (cmd && cmd->shared)
            result.insert(it.key(), it.value());
    }
    return result;
}

QByteArray toJson(const Profile &profile)
{
    QJsonObject bindings;
    const Bindings shared = sharedOnly(profile.bindings);
    for (auto it = shared.cbegin(); it != shared.cend(); ++it)
        bindings.insert(it.key(), it.value());
    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(formatName));
    root.insert(QStringLiteral("version"), ProfileVersion);
    root.insert(QStringLiteral("application"), profile.application);
    root.insert(QStringLiteral("vimMode"), profile.vimMode);
    root.insert(QStringLiteral("hints"), profile.hints);
    root.insert(QStringLiteral("hintDelay"), profile.hintDelay);
    root.insert(QStringLiteral("timeout"), profile.timeout);
    root.insert(QStringLiteral("bindings"), bindings);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ParseResult fromJson(const QByteArray &json)
{
    ParseResult result;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = tr("The file is not a keymap profile.");
        return result;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(formatName)) {
        result.error = tr("The file is not a keymap profile.");
        return result;
    }
    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version < 1 || version > ProfileVersion) {
        result.error = tr("The keymap profile is of version %1, which this version cannot read.").arg(version);
        return result;
    }
    Profile profile;
    profile.application = root.value(QStringLiteral("application")).toString();
    auto readBool = [&root](const char *name, bool fallback) {
        const QJsonValue value = root.value(QLatin1String(name));
        return value.isBool() ? value.toBool() : fallback;
    };
    auto readInt = [&root](const char *name, int fallback, int low, int high) {
        const QJsonValue value = root.value(QLatin1String(name));
        return value.isDouble() ? std::clamp(value.toInt(fallback), low, high) : fallback;
    };
    profile.vimMode = readBool("vimMode", profile.vimMode);
    profile.hints = readBool("hints", profile.hints);
    profile.hintDelay = readInt("hintDelay", profile.hintDelay, 0, 5000);
    profile.timeout = readInt("timeout", profile.timeout, 200, 10000);
    const QJsonObject bindings = root.value(QStringLiteral("bindings")).toObject();
    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        const Command *cmd = command(it.key());
        const QString keys = it.value().toString();
        if (!cmd || !cmd->shared || !it.value().isString() || (!keys.isEmpty() && !isValid(keys))) {
            result.ignored << it.key();
            continue;
        }
        profile.bindings.insert(it.key(), keys);
    }
    result.profile = profile;
    return result;
}

QList<Change> preview(const Profile &current, const Profile &incoming)
{
    QList<Change> result;
    auto onOff = [](bool value) { return value ? QStringLiteral("on") : QStringLiteral("off"); };
    if (current.vimMode != incoming.vimMode)
        result.append({QStringLiteral("vimMode"), onOff(current.vimMode), onOff(incoming.vimMode)});
    if (current.hints != incoming.hints)
        result.append({QStringLiteral("hints"), onOff(current.hints), onOff(incoming.hints)});
    if (current.hintDelay != incoming.hintDelay)
        result.append({QStringLiteral("hintDelay"), QString::number(current.hintDelay), QString::number(incoming.hintDelay)});
    if (current.timeout != incoming.timeout)
        result.append({QStringLiteral("timeout"), QString::number(current.timeout), QString::number(incoming.timeout)});
    const Bindings shared = sharedOnly(incoming.bindings);
    for (auto it = shared.cbegin(); it != shared.cend(); ++it) {
        const QString from = current.bindings.value(it.key(), command(it.key())->defaultKeys);
        if (from != it.value())
            result.append({it.key(), from, it.value()});
    }
    return result;
}

Profile apply(const Profile &current, const Profile &incoming)
{
    Profile result = current;
    result.vimMode = incoming.vimMode;
    result.hints = incoming.hints;
    result.hintDelay = incoming.hintDelay;
    result.timeout = incoming.timeout;
    const Bindings shared = sharedOnly(incoming.bindings);
    for (auto it = shared.cbegin(); it != shared.cend(); ++it)
        result.bindings.insert(it.key(), it.value());
    return result;
}

}

}
