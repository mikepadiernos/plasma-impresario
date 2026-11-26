/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QStringList>

class QModelIndex;

/** @short Client-side relevance of the loaded messages for a quick search

The IMAP server tells which messages match a search, but not how well. These
helpers rank the matching messages by what is already known about them, without
downloading anything.
*/
namespace UiUtils {

/** @short Words and phrases which the IMAP SEARCH @arg conditions look for

The values of the SUBJECT, FROM, TO, CC, BCC, BODY, TEXT and HEADER keys are
returned case-folded and without duplicates. A single element is a raw search
typed by the user, such as `OR SUBJECT "release plan" FROM ann`; it is split
into tokens first. Other keys, such as dates or flags, are ignored, and so are
keys under NOT, since a match does not contain their text.
*/
QStringList relevanceTerms(const QStringList &conditions);

/** @short Text which the reader highlights in a message opened from the search
@arg conditions

This is the first value of a SUBJECT, BODY or TEXT key, with its case kept and
whitespace simplified. A search which only looks at the senders, recipients or
other headers gives an empty string, since those are not part of the message
body. Keys under NOT are skipped. A single element is a raw search, as for
relevanceTerms().
*/
QString searchHighlightTerm(const QStringList &conditions);

/** @short How well @arg message matches @arg terms; higher is better

A term found in the subject counts more than one in the senders, which counts
more than one in the To and Cc recipients. A whole phrase counts more than its
words found apart. Body text is not loaded, so a message which matched only
through its body scores 0.

Returns -1 for a message whose envelope has not been loaded yet, and -2 for a
row which is not a message, such as a placeholder or a group header.
*/
int relevanceScore(const QModelIndex &message, const QStringList &terms);

} // namespace UiUtils
