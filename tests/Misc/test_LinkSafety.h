/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEST_LINKSAFETY_H
#define TEST_LINKSAFETY_H

#include <QObject>

/** @short Verify the privacy checks for links and remote content */
class LinkSafetyTest : public QObject {
  Q_OBJECT

private slots:
  void readsLinkTextHost_data();
  void readsLinkTextHost();
  void comparesSites_data();
  void comparesSites();
  void assessesLinks_data();
  void assessesLinks();
  void readsMailtoAddresses();
  void listsRemoteHosts();
};

#endif
