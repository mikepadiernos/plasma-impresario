# Archived Trojitá documents

**[Documentation](../README.md)** · **[Application](../../README.md)** ·
**[Installation](../INSTALL.md)**

This directory keeps documents from the Trojitá project that Plasma Impresario
was forked from: the user handbook, research, protocol notes and talks.

> [!NOTE]
>
> These documents are historical. They describe Trojitá, not current Plasma
> Impresario behavior. The current source, build configuration and
> [documentation index](../README.md) define present behavior.

## Installed documentation

The build installs these two files, so they still reach users:

| File                             | Contents                                                       |
| -------------------------------- | -------------------------------------------------------------- |
| [`index.docbook`](index.docbook) | The Trojitá handbook, installed when KF6 DocTools is available |
| [`trojita.1`](trojita.1)         | The `trojita` man page                                         |

## Notes and specifications

| File                                                 | Contents                                                       |
| ---------------------------------------------------- | -------------------------------------------------------------- |
| [`user-spec.txt`](user-spec.txt)                     | Original description of the Trojitá IMAP client and its goals  |
| [`rfcs`](rfcs)                                       | List of supported IMAP and mail RFCs                           |
| [`server-capabilities`](server-capabilities)         | IMAP capability strings observed on various servers            |
| [`idle-considerations.txt`](idle-considerations.txt) | Notes on IMAP IDLE and server keepalive behavior               |
| [`ImapMailbox.txt`](ImapMailbox.txt)                 | Early design sketch of the mailbox, cache and parser interface |
| [`memory-usage.txt`](memory-usage.txt)               | Memory measurements with large mailboxes                       |

## Research and talks

| Path                                                                   | Contents                                                                             |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| [`thesis/`](thesis/)                                                   | LaTeX bachelor's thesis "Trojita: A Qt IMAP Client"                                  |
| [`masters/`](masters/)                                                 | LaTeX master's thesis "IMAP extension for mobile devices", with `build.sh`           |
| [`master-abstract.txt`](master-abstract.txt)                           | Abstract of the master's thesis                                                      |
| [`proposed-extensions/`](proposed-extensions/)                         | Internet-Draft sources for the IMAP SUBMIT and INCTHREAD extensions, with `build.sh` |
| [`talks/`](talks/)                                                     | LaTeX slides from OpenMobility 2012, the 2012 master's defense and Akademy 2015      |
| [`2011-desktop-summit-abstract.txt`](2011-desktop-summit-abstract.txt) | Abstract of the 2011 Desktop Summit talk                                             |
