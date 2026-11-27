/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace UiUtils {

namespace SearchQuery {
class Query;
}

/** @short Rules which sort, mark and announce incoming mail

A rule has a scope (one account or every account, and the folders whose new mail it checks), conditions on the
sender, the recipients, the subject, attachments, the folder, tags and the junk mark, and actions: move, archive, move
to the trash, flag, mark as read, add a tag and notify. Rules run in their order. Every matching rule adds its marks;
the first one which moves the message, or which says to stop, ends the processing, because a later rule cannot act on
a message which has left the folder.

Moving, archiving and moving to the trash take a message out of sight, so a rule with such an action runs on its own
only after its matches were previewed once. The preview is tied to what the rule does: editing its conditions, actions
or scope asks for a new preview, while renaming it or turning it off and on does not.

Rules check what the list of messages knows without downloading a message: the envelope, the flags and whether the
message has attachments. Message text, dates and sizes cannot be checked on arrival.

This namespace has no storage and no network access; see MailRulesSettings for the configuration and the GUI for the
execution.
*/
namespace MailRules {

enum class Field {
    /** The From addresses and names */
    Sender,
    /** The To and Cc addresses and names */
    Recipients,
    Subject,
    /** Whether the message has attachments; compared with Is and IsNot */
    Attachment,
    /** The full name of the folder the message is in */
    Folder,
    /** The message's tags (IMAP keywords) */
    Tag,
    /** Whether the server or the user marked the message as junk; compared with Is and IsNot */
    Junk,
};

enum class Comparison {
    Contains,
    DoesNotContain,
    Is,
    IsNot,
};

struct Condition {
    Field field = Field::Subject;
    Comparison comparison = Comparison::Contains;
    QString value;
    bool operator==(const Condition &other) const = default;
};

enum class ActionKind {
    /** Move to the folder in Action::value */
    Move,
    /** Move to the account's archive folder */
    Archive,
    /** Move to the account's trash folder */
    Delete,
    Flag,
    MarkRead,
    /** Add the tag in Action::value */
    AddTag,
    /** Announce the message, even in a folder whose new mail is not announced otherwise */
    Notify,
};

struct Action {
    ActionKind kind = ActionKind::Flag;
    QString value;
    bool operator==(const Action &other) const = default;
};

enum class Match {
    /** Every condition has to match */
    All,
    /** One matching condition is enough */
    Any,
};

struct Rule {
    QString id;
    QString name;
    bool enabled = true;
    /** The account whose mail the rule checks; empty for every account */
    QString accountId;
    /** The folders whose new mail the rule checks; empty for the INBOX alone */
    QStringList folders;
    Match match = Match::All;
    QList<Condition> conditions;
    QList<Action> actions;
    /** Whether the rules after this one are skipped for a matching message */
    bool stop = false;
    /** The fingerprint() of the version whose matches were last previewed */
    QString reviewedFingerprint;

    /** Whether an action takes the message out of the folder */
    bool movesMessages() const;
    /** A short hash of what the rule does: its scope, conditions, actions and stop */
    QString fingerprint() const;
    /** Whether the version as it is now was previewed */
    bool isReviewed() const;
    /** Whether the rule is on but waits for a preview before it moves messages on its own */
    bool needsReview() const;
    /** Whether the rule runs on new mail by itself: it is on and does not need a preview */
    bool runsAutomatically() const;
    /** Record that the matches of this version were previewed */
    void markReviewed();
    /** The folders checked, with the INBOX standing in for an empty list */
    QStringList checkedFolders() const;

    bool operator==(const Rule &other) const = default;
};

/** @short A message as the rules see it */
struct Message {
    QString accountId;
    QString folder;
    uint uid = 0;
    /** The From addresses as "Name <address>", or the address alone */
    QStringList senders;
    /** The To and Cc addresses, in the same form */
    QStringList recipients;
    QString subject;
    bool hasAttachment = false;
    /** IMAP flags and keywords */
    QStringList flags;
    /** When the server received the message (INTERNALDATE) */
    QDateTime received;
    QString messageId;
    /** The Message-IDs which the message replies to */
    QStringList inReplyTo;
    bool unread = true;
};

/** Whether @arg rule checks new mail of @arg folder in @arg accountId */
bool appliesTo(const Rule &rule, const QString &accountId, const QString &folder);
/** Whether @arg condition matches @arg message */
bool matches(const Condition &condition, const Message &message);
/** Whether the conditions of @arg rule match @arg message; a rule without conditions matches every message */
bool matches(const Rule &rule, const Message &message);
/** Whether @arg flags mark a message as junk: $Junk or Junk, unless $NotJunk or NotJunk is there too */
bool isJunk(const QStringList &flags);

/** @short Why a rule cannot be saved, one sentence each; empty when it can */
QStringList problems(const Rule &rule);
/** @short Warnings which do not stop saving, such as a rule without conditions */
QStringList warnings(const Rule &rule);

enum class Mode {
    /** New mail: rules waiting for a preview do nothing and are reported in Outcome::waiting */
    Automatic,
    /** A preview or a run started by the user: every enabled rule counts */
    Manual,
};

/** @short What the rules do to one message */
struct Outcome {
    enum class Destination {
        Stay,
        Move,
        Archive,
        Trash,
    };
    /** The rules which matched, in their order */
    QStringList ruleIds;
    QStringList ruleNames;
    Destination destination = Destination::Stay;
    /** The folder of Destination::Move */
    QString folder;
    /** The rule which decided the destination */
    QString destinationRuleId;
    bool flag = false;
    bool markRead = false;
    bool notify = false;
    QStringList tags;
    /** The rules which matched but wait for a preview before they move anything (Mode::Automatic only) */
    QStringList waiting;

    /** Whether any rule matched */
    bool matched() const { return !ruleIds.isEmpty(); }
    /** Whether the message leaves its folder */
    bool moves() const { return destination != Destination::Stay; }
    bool operator==(const Outcome &other) const = default;
};

/** @short Apply @arg rules in their order to @arg message

Only enabled rules whose scope covers the message's account and folder count. In Mode::Automatic, a rule which needs a
preview adds nothing and is listed in Outcome::waiting.
*/
Outcome evaluate(const QList<Rule> &rules, const Message &message, Mode mode = Mode::Automatic);

/** The name of @arg field for the rule editor, such as "Sender" */
QString fieldName(Field field);
/** The name of @arg comparison for @arg field, such as "contains" or, for attachments, "is present" */
QString comparisonName(Field field, Comparison comparison);
/** The comparisons which make sense for @arg field */
QList<Comparison> comparisonsFor(Field field);
/** Whether @arg field takes a value, as opposed to Attachment and Junk */
bool takesValue(Field field);
/** The name of @arg kind for the rule editor, such as "Move to folder" */
QString actionName(ActionKind kind);
/** Whether @arg kind takes a value: the folder of Move and the tag of AddTag */
bool takesValue(ActionKind kind);
/** @short One condition as a sentence part, such as "Subject contains “invoice”" */
QString describe(const Condition &condition);
/** @short One action as a sentence part, such as "move to “Lists/KDE”" */
QString describe(const Action &action);
/** @short The rule in one line: its conditions and actions */
QString summary(const Rule &rule);

/** @short A rule made from a search, with what could not be carried over */
struct Conversion {
    Rule rule;
    /** Parts of the query which rules cannot check, such as body text or dates; the rule is not usable when set */
    QStringList unsupported;
    /** Changes in meaning worth telling, such as words without a field being checked in the subject */
    QStringList notes;
    bool isUsable() const { return unsupported.isEmpty(); }
};

/** @short Turn a search into a rule's conditions

A single term, terms joined by AND and terms joined by OR become conditions matching all or any of them; a term
negated with NOT or a minus sign becomes the opposite comparison. Fields map as follows: from: to Sender, to:, cc: and
bcc: to Recipients, subject: and words without a field to Subject, tag: to Tag and has:attachment to Attachment.
Body text, the whole message, mailing lists, message states (is:), dates and sizes, and nested groups cannot be checked
by rules and are listed in Conversion::unsupported. The rule has no actions yet.
*/
Conversion fromQuery(const SearchQuery::Query &query);

/** A new rule ID which is not in @arg taken */
QString newId(const QStringList &taken);

}

}
