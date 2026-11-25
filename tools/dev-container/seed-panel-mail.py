#!/usr/bin/env python3
"""Seed a Maildir INBOX with synthetic messages that exercise the embedded panels.

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
]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    maildir = sys.argv[1]
    for part in ("cur", "new", "tmp"):
        os.makedirs(os.path.join(maildir, part), exist_ok=True)
    for name, flags, lines in MESSAGES:
        if flags is None:
            path = os.path.join(maildir, "new", f"{name}.synthetic")
        else:
            path = os.path.join(maildir, "cur", f"{name}.synthetic:2,{flags}")
        with open(path, "wb") as message:
            message.write(("\r\n".join(lines) + "\r\n").encode("utf-8"))


if __name__ == "__main__":
    main()
