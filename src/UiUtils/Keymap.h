/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFlags>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

namespace UiUtils {

/** @short The optional Vim mode: its commands, their key sequences and the profile shared with Plasma Opus

Key sequences are written in Vim notation: printable keys stand for themselves, so "gg" is g pressed twice and "G"
is Shift+G, while "<C-d>" is Ctrl+D and "<Esc>" is Escape. A sequence is a list of tokens; see tokens().

Shared commands, such as moving and searching, are what Plasma Opus offers too, and only they are exchanged through
the keymap profile. Mail-only commands, such as replying, stay in this application.
*/
namespace Keymap {

/** @short The parts of the window in which Vim mode handles keys */
enum class Context {
    FolderTree = 0x1,
    MessageList = 0x2,
    Reader = 0x4,
};
Q_DECLARE_FLAGS(Contexts, Context)

struct Command {
    /** Stable identifier, such as "move.down"; the same in the Plasma Opus profile for shared commands */
    QString id;
    QString defaultKeys;
    Contexts contexts;
    /** Whether the command is exchanged with Plasma Opus */
    bool shared = false;
    /** Object name of the window action the command triggers, empty when the window carries it out itself */
    QString action;
};

/** @short Every command, in the order of the shortcut reference */
const QList<Command> &commands();
const Command *command(const QString &id);
/** @short What @arg id does, in a few words for the hint panel and the shortcut reference */
QString description(const QString &id);

/** @short Key sequences by command; an empty sequence turns a command off */
using Bindings = QMap<QString, QString>;

Bindings defaultBindings();
/** @short The default bindings with @arg overrides applied; overrides of unknown commands are ignored */
Bindings merged(const Bindings &overrides);
/** @short The entries of @arg bindings which differ from the defaults */
Bindings overrides(const Bindings &bindings);

/** @short The tokens of @arg keys, or an empty list when @arg keys is not valid notation */
QStringList tokens(const QString &keys);
bool isValid(const QString &keys);
/** @short The token of a key press, or an empty string for keys Vim mode leaves alone, such as Tab or Return

@arg text is what the key types. Shift is part of the typed text, so Shift+G is "G"; with Ctrl, the token is
"<C-x>" for the key's letter.
*/
QString token(int key, Qt::KeyboardModifiers modifiers, const QString &text);

struct Conflict {
    enum class Kind {
        /** Two commands use the same sequence in a context they share */
        Same,
        /** One sequence starts another, so the longer one can never be typed */
        Prefix,
    };
    Kind kind = Kind::Same;
    QString keys;
    QStringList commands;

    bool operator==(const Conflict &other) const = default;
};
/** @short Sequences of @arg bindings which cannot all work */
QList<Conflict> conflicts(const Bindings &bindings);

/** @short Commands whose first key is also a single-key shortcut of the window, such as M for Mark as Read

In the folder tree, the message list and the reader, Vim mode handles these keys first while it is on.
*/
QStringList shadowedShortcuts(const Bindings &bindings, const QList<QKeySequence> &shortcuts);

/** @short Follows a sequence of keys as they are typed */
class Matcher {
public:
    enum class Result {
        /** No command starts with the keys typed; they are left to the window */
        None,
        /** The keys start one or more commands; more keys are needed */
        Pending,
        /** The keys complete a command; see matched() */
        Matched,
        /** Escape dropped the keys typed so far */
        Cancelled,
    };

    explicit Matcher(const Bindings &bindings = defaultBindings());
    void setBindings(const Bindings &bindings);

    Result feed(const QString &token, Context context);
    void reset();
    /** @short The command completed by the last key */
    QString matched() const;
    /** @short The keys typed so far of an incomplete sequence */
    QString pending() const;

    struct Continuation {
        /** The keys still to type */
        QString keys;
        QString command;

        bool operator==(const Continuation &other) const = default;
    };
    /** @short What may follow the pending keys in @arg context, sorted by the keys */
    QList<Continuation> continuations(Context context) const;

private:
    Bindings m_bindings;
    QStringList m_pending;
    QString m_matched;
};

/** @short The settings shared with Plasma Opus

Written as a JSON object with "format": "plasma-keymap", "version": 1, the settings and the bindings of shared
commands. Unknown commands and fields are ignored when reading, so that newer profiles stay readable.
*/
struct Profile {
    bool vimMode = false;
    bool hints = true;
    int hintDelay = 400;
    int timeout = 1000;
    /** Bindings of shared commands only */
    Bindings bindings;
    /** Which application wrote the profile */
    QString application;

    bool operator==(const Profile &other) const = default;
};

constexpr int ProfileVersion = 1;

QByteArray toJson(const Profile &profile);

struct ParseResult {
    std::optional<Profile> profile;
    /** Why the profile could not be read */
    QString error;
    /** Commands of the profile which were skipped: unknown, app-local or with invalid keys */
    QStringList ignored;
};
ParseResult fromJson(const QByteArray &json);

/** @short @arg bindings without the mail-only commands, as exported */
Bindings sharedOnly(const Bindings &bindings);

/** @short A difference between the current settings and a profile, for the preview before importing */
struct Change {
    /** A command id, or "vimMode", "hints", "hintDelay" or "timeout" */
    QString item;
    QString from;
    QString to;

    bool operator==(const Change &other) const = default;
};
QList<Change> preview(const Profile &current, const Profile &incoming);

/** @short @arg current with the settings and shared bindings of @arg incoming; mail-only bindings stay */
Profile apply(const Profile &current, const Profile &incoming);

}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(UiUtils::Keymap::Contexts)
