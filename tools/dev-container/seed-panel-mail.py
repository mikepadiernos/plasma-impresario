#!/usr/bin/env python3
"""Seed a Maildir with synthetic messages that exercise the embedded panels.

Usage: seed-panel-mail.py MAILDIR

Every address uses the reserved example.test domain, and the signature is not
a real OpenPGP signature. The messages are:

  plan-1     "Quarterly plan" from Alice to the panels user, Cc Bob; read and
             flagged, with plan.pdf and budget.csv attached
  plan-2     "Re: Quarterly plan" from Bob, Reply-To bob-replies, to Alice and
             Cc the panels user; read and answered
  budget     "Budget numbers" from Dave, In-Reply-To plan-1 (same conversation,
             different subject); unread
  signed     "Signed release note" from Carol, multipart/signed; unread
  lunch      "Lunch on Friday" from Alice (same sender as plan-1); unread
  news       "Weekly news" from Erin, HTML with a remote image on
             images.example.test and a link whose text shows another site;
             unread
  travel     "Travel plans" from Frank; unread, and never opened before the
             test goes offline, so its body is not available offline
  digest     "Release digest" from Grace, multipart/alternative with plain
             text and HTML versions which can be told apart; unread

The Conversations folder (MAILDIR/.Conversations) exercises the grouping of
conversations; all its messages are read:

  status     "[team] Status update" from Alice, 2 Nov
  status-aw  "AW: [team] Status update" from Bob, 3 Nov, without In-Reply-To
             or References
  status-re  "Re[2]: Status update" from Carol, 5 Nov, without links either
  status-old "Re: Status update" from Dave, 20 Sep, without links and more
             than 30 days before the others, so it stays apart
  hello-1    "Hello" from Erin, 2 Nov, and hello-2 "Hello" from Frank, 4 Nov:
             unrelated mail of the same subject
  minutes    "Minutes" from Alice, 9 Nov, and agenda "Agenda" from Bob, 9 Nov
  both       "Re: Minutes and agenda" from Carol, 10 Nov, which references both

The Older folder (MAILDIR/.Older) exercises the server search of the related
messages panel; its UIDs repeat those of INBOX, and all its messages are read:

  plan-final "Re: Quarterly plan (final)" from Carol, In-Reply-To plan-2 and
             References plan-1 plan-2, so it joins the INBOX conversation
  old-lunch  "Old lunch notes" from Alice (same sender as plan-1)
  malice     "Not from Alice" from malice@example.test, which FROM
             alice@example.test matches as a substring without being related
"""

import base64
import os
import sys

DOMAIN = "example.test"
USER = f"Panels User <panels@{DOMAIN}>"
ALICE = f"Alice Example <alice@{DOMAIN}>"
BOB = f"Bob Example <bob@{DOMAIN}>"
CAROL = f"Carol Example <carol@{DOMAIN}>"
DAVE = f"Dave Example <dave@{DOMAIN}>"
ERIN = f"Erin Example <erin@{DOMAIN}>"
FRANK = f"Frank Example <frank@{DOMAIN}>"
GRACE = f"Grace Example <grace@{DOMAIN}>"
MALICE = f"Malice Example <malice@{DOMAIN}>"


def headers(sender, to, subject, date, message_id, extra=()):
    lines = [
        f"From: {sender}",
        f"To: {to}",
        f"Subject: {subject}",
        f"Date: {date}",
        f"Message-ID: <{message_id}@{DOMAIN}>",
        "MIME-Version: 1.0",
    ]
    lines.extend(extra)
    return lines


def text_message(head, body):
    return head + ["Content-Type: text/plain; charset=utf-8", "", *body.splitlines()]


def wrap64(data):
    encoded = base64.b64encode(data).decode("ascii")
    return [encoded[i : i + 76] for i in range(0, len(encoded), 76)]


def plan_message():
    boundary = "synthetic-plan-boundary"
    pdf = (
        b"%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        b"2 0 obj<</Type/Pages/Kids[]/Count 0>>endobj\ntrailer<</Root 1 0 R>>\n%%EOF\n"
    )
    csv = b"quarter,amount\nQ1,1200\nQ2,1350\nQ3,990\nQ4,1500\n"
    head = headers(
        ALICE,
        USER,
        "Quarterly plan",
        "Fri, 20 Nov 2026 09:00:00 +0000",
        "plan-1",
        [f"Cc: {BOB}", f'Content-Type: multipart/mixed; boundary="{boundary}"'],
    )
    return head + [
        "",
        f"--{boundary}",
        "Content-Type: text/plain; charset=utf-8",
        "",
        "Hi both,",
        "",
        "The draft plan and the budget figures are attached.",
        "",
        "Alice",
        f"--{boundary}",
        'Content-Type: application/pdf; name="plan.pdf"',
        'Content-Disposition: attachment; filename="plan.pdf"',
        "Content-Transfer-Encoding: base64",
        "",
        *wrap64(pdf),
        f"--{boundary}",
        'Content-Type: text/csv; charset=utf-8; name="budget.csv"',
        'Content-Disposition: attachment; filename="budget.csv"',
        "Content-Transfer-Encoding: base64",
        "",
        *wrap64(csv),
        f"--{boundary}--",
    ]


def signed_message():
    boundary = "synthetic-signed-boundary"
    head = headers(
        CAROL,
        USER,
        "Signed release note",
        "Sun, 22 Nov 2026 16:45:00 +0000",
        "signed",
        [
            "Content-Type: multipart/signed; micalg=pgp-sha256;"
            f' protocol="application/pgp-signature"; boundary="{boundary}"'
        ],
    )
    return head + [
        "",
        f"--{boundary}",
        "Content-Type: text/plain; charset=utf-8",
        "",
        "The release note is final. This synthetic signature will not verify.",
        f"--{boundary}",
        'Content-Type: application/pgp-signature; name="signature.asc"',
        "Content-Description: OpenPGP digital signature",
        "",
        "-----BEGIN PGP SIGNATURE-----",
        "",
        "c3ludGhldGljLXNpZ25hdHVyZS1mb3ItcGxhc21hLWltcHJlc2FyaW8tdGVzdHM=",
        "=AAAA",
        "-----END PGP SIGNATURE-----",
        f"--{boundary}--",
    ]


MESSAGES = [
    # (file name, Maildir flags or None for unread in new/, lines)
    ("1-plan-1", "FS", plan_message()),
    (
        "2-plan-2",
        "RS",
        text_message(
            headers(
                BOB,
                ALICE,
                "Re: Quarterly plan",
                "Sat, 21 Nov 2026 10:30:00 +0000",
                "plan-2",
                [
                    f"Cc: {USER}",
                    f"Reply-To: Bob Replies <bob-replies@{DOMAIN}>",
                    f"In-Reply-To: <plan-1@{DOMAIN}>",
                    f"References: <plan-1@{DOMAIN}>",
                ],
            ),
            "Looks good to me. Replies go to my shared inbox.\n\nBob",
        ),
    ),
    (
        "3-budget",
        None,
        text_message(
            headers(
                DAVE,
                ALICE,
                "Budget numbers",
                "Sat, 21 Nov 2026 14:00:00 +0000",
                "budget",
                [f"Cc: {USER}", f"In-Reply-To: <plan-1@{DOMAIN}>", f"References: <plan-1@{DOMAIN}>"],
            ),
            "The Q3 figure in budget.csv should be 1090.\n\nDave",
        ),
    ),
    ("4-signed", None, signed_message()),
    (
        "6-news",
        None,
        headers(ERIN, USER, "Weekly news", "Tue, 24 Nov 2026 08:00:00 +0000", "news")
        + [
            "Content-Type: text/html; charset=utf-8",
            "",
            "<html><body><p>This week's news.</p>",
            f'<p><img src="https://images.{DOMAIN}/banner.png" alt="Banner"></p>',
            f'<p><a id="sign-in" href="https://sign-in.{DOMAIN}.invalid/">https://bank.{DOMAIN}/</a></p>',
            "</body></html>",
        ],
    ),
    (
        "5-lunch",
        None,
        text_message(
            headers(ALICE, USER, "Lunch on Friday", "Mon, 23 Nov 2026 12:15:00 +0000", "lunch"),
            "Shall we try the new place on Friday?\n\nAlice",
        ),
    ),
    (
        "7-travel",
        None,
        text_message(
            headers(FRANK, USER, "Travel plans", "Wed, 25 Nov 2026 07:30:00 +0000", "travel"),
            "The train leaves at nine on Monday.\n\nFrank",
        ),
    ),
    (
        "8-digest",
        None,
        headers(
            GRACE,
            USER,
            "Release digest",
            "Tue, 24 Nov 2026 18:00:00 +0000",
            "digest",
            ['Content-Type: multipart/alternative; boundary="synthetic-digest-boundary"'],
        )
        + [
            "",
            "--synthetic-digest-boundary",
            "Content-Type: text/plain; charset=utf-8",
            "",
            "Release digest (plain text version)",
            "",
            "Grace",
            "--synthetic-digest-boundary",
            "Content-Type: text/html; charset=utf-8",
            "",
            '<html><body><p class="html-only">Release digest (HTML version)</p><p>Grace</p></body></html>',
            "--synthetic-digest-boundary--",
        ],
    ),
]


def conversation(sender, subject, date, message_id, extra=()):
    return text_message(
        headers(sender, USER, subject, f"{date} +0000", message_id, extra),
        f"{subject}\n\nSynthetic conversation message.",
    )


CONVERSATIONS = [
    ("1-status", "S", conversation(ALICE, "[team] Status update", "Mon, 02 Nov 2026 09:00:00", "status")),
    ("2-status-aw", "S", conversation(BOB, "AW: [team] Status update", "Tue, 03 Nov 2026 09:00:00", "status-aw")),
    ("3-status-re", "S", conversation(CAROL, "Re[2]: Status update", "Thu, 05 Nov 2026 09:00:00", "status-re")),
    ("4-status-old", "S", conversation(DAVE, "Re: Status update", "Sun, 20 Sep 2026 09:00:00", "status-old")),
    ("5-hello-1", "S", conversation(ERIN, "Hello", "Mon, 02 Nov 2026 10:00:00", "hello-1")),
    ("6-hello-2", "S", conversation(FRANK, "Hello", "Wed, 04 Nov 2026 10:00:00", "hello-2")),
    ("7-minutes", "S", conversation(ALICE, "Minutes", "Mon, 09 Nov 2026 09:00:00", "minutes")),
    ("8-agenda", "S", conversation(BOB, "Agenda", "Mon, 09 Nov 2026 09:30:00", "agenda")),
    (
        "9-both",
        "S",
        conversation(
            CAROL,
            "Re: Minutes and agenda",
            "Tue, 10 Nov 2026 09:00:00",
            "both",
            [f"References: <minutes@{DOMAIN}> <agenda@{DOMAIN}>"],
        ),
    ),
]


OLDER = [
    (
        "1-plan-final",
        "S",
        conversation(
            CAROL,
            "Re: Quarterly plan (final)",
            "Wed, 25 Nov 2026 09:00:00",
            "plan-final",
            [f"In-Reply-To: <plan-2@{DOMAIN}>", f"References: <plan-1@{DOMAIN}> <plan-2@{DOMAIN}>"],
        ),
    ),
    ("2-old-lunch", "S", conversation(ALICE, "Old lunch notes", "Mon, 16 Nov 2026 12:00:00", "old-lunch")),
    ("3-malice", "S", conversation(MALICE, "Not from Alice", "Tue, 17 Nov 2026 12:00:00", "malice")),
]


def write_maildir(maildir, messages):
    for part in ("cur", "new", "tmp"):
        os.makedirs(os.path.join(maildir, part), exist_ok=True)
    for name, flags, lines in messages:
        if flags is None:
            path = os.path.join(maildir, "new", f"{name}.synthetic")
        else:
            path = os.path.join(maildir, "cur", f"{name}.synthetic:2,{flags}")
        with open(path, "wb") as message:
            message.write(("\r\n".join(lines) + "\r\n").encode("utf-8"))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    maildir = sys.argv[1]
    write_maildir(maildir, MESSAGES)
    for name, messages in ((".Conversations", CONVERSATIONS), (".Older", OLDER)):
        folder = os.path.join(maildir, name)
        write_maildir(folder, messages)
        open(os.path.join(folder, "maildirfolder"), "wb").close()


if __name__ == "__main__":
    main()
