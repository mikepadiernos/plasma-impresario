#!/usr/bin/env python3
# Copyright (C) 2026 Plasma Impresario contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Seed a synthetic Maildir for the live workload test.

Usage: seed-workload-mail.py MAILDIR MESSAGES CONVERSATION ATTACHMENTS ATTACHMENT_MIB

- INBOX: MESSAGES short messages, read except every tenth one.
- Conversation: one reply chain of CONVERSATION messages with In-Reply-To and
  References, as a long mailing-list thread would have.
- Attachments: "Synthetic many attachments" with ATTACHMENTS small files, and
  "Synthetic large attachment" with one file of ATTACHMENT_MIB MiB.

Every address, subject and body is synthetic. Files are written straight into
cur/ so that the server indexes them on its first look.
"""

import base64
import os
import sys


def maildir(path):
    for part in ("cur", "new", "tmp"):
        os.makedirs(os.path.join(path, part), exist_ok=True)


def folder(root, name):
    path = os.path.join(root, "." + name)
    maildir(path)
    open(os.path.join(path, "maildirfolder"), "w").close()
    return path


def write(path, name, flags, data):
    with open(os.path.join(path, "cur", f"{name}.synthetic:2,{flags}"), "wb") as out:
        out.write(data)


def headers(subject, message_id, day, extra=""):
    return (
        "From: Synthetic Sender <sender@example.test>\r\n"
        "To: workload@example.test\r\n"
        f"Subject: {subject}\r\n"
        f"Date: {day}\r\n"
        f"Message-ID: <{message_id}@example.test>\r\n"
        f"{extra}"
    )


def date(i):
    # One message a minute from 1 June 2026, oldest first
    minutes = i % (183 * 1440)
    days = minutes // 1440
    months = (("Jun", 30), ("Jul", 31), ("Aug", 31), ("Sep", 30), ("Oct", 31), ("Nov", 30))
    for month, length in months:
        if days < length:
            break
        days -= length
    return f"{days + 1:02d} {month} 2026 {(minutes // 60) % 24:02d}:{minutes % 60:02d}:00 +0000"


def b64_lines(data):
    return base64.encodebytes(data).replace(b"\n", b"\r\n")


def main():
    root = sys.argv[1]
    messages, conversation, attachments, mib = map(int, sys.argv[2:6])
    maildir(root)

    body = b"Content-Type: text/plain\r\n\r\nA synthetic message for the workload test.\r\n"
    for i in range(messages):
        flags = "" if i % 10 == 9 else "S"
        data = headers(f"Synthetic workload {i + 1}", f"workload-{i + 1}", date(i)).encode() + body
        write(root, f"workload-{i + 1:06d}", flags, data)

    thread = folder(root, "Conversation")
    references = []
    for i in range(conversation):
        extra = ""
        if references:
            extra = f"In-Reply-To: {references[-1]}\r\nReferences: {' '.join(references[-20:])}\r\n"
        subject = "Synthetic conversation" if i == 0 else "Re: Synthetic conversation"
        message_id = f"conversation-{i + 1}"
        data = headers(subject, message_id, date(i), extra).encode() + body
        write(thread, f"conversation-{i + 1:05d}", "S", data)
        references.append(f"<{message_id}@example.test>")

    files = folder(root, "Attachments")
    parts = [b'MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary="part"\r\n\r\n',
             b"--part\r\nContent-Type: text/plain\r\n\r\nMany files.\r\n"]
    for i in range(attachments):
        parts.append(
            f"--part\r\nContent-Type: application/octet-stream\r\n"
            f'Content-Disposition: attachment; filename="file-{i + 1:03d}.bin"\r\n'
            f"Content-Transfer-Encoding: base64\r\n\r\n".encode()
            + b64_lines(bytes([i % 256]) * 2048))
    parts.append(b"--part--\r\n")
    write(files, "many-attachments", "S",
          headers("Synthetic many attachments", "many-attachments", date(0)).encode() + b"".join(parts))

    large = (b'MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary="part"\r\n\r\n'
             b"--part\r\nContent-Type: text/plain\r\n\r\nOne large file.\r\n"
             b"--part\r\nContent-Type: application/octet-stream\r\n"
             b'Content-Disposition: attachment; filename="large.bin"\r\n'
             b"Content-Transfer-Encoding: base64\r\n\r\n"
             + b64_lines(os.urandom(mib * 1024 * 1024))
             + b"--part--\r\n")
    write(files, "large-attachment", "S",
          headers("Synthetic large attachment", "large-attachment", date(1)).encode() + large)


if __name__ == "__main__":
    main()
