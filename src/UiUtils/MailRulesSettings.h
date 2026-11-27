/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>

#include "UiUtils/MailRules.h"

class KConfigBase;

namespace UiUtils {

/** @short The mail rules as stored in the configuration

The rules are kept in their order in the MailRules group, one subgroup per rule, with readable entries: the scope, the
conditions as "field|comparison|value", the actions as "kind|value" and the fingerprint of the version last previewed.
Entries which cannot be read, such as a condition on an unknown field, are dropped.
*/
namespace MailRulesSettings {

QList<MailRules::Rule> load(const KConfigBase &config);
void save(KConfigBase &config, const QList<MailRules::Rule> &rules);

/** The rules of @arg accountId are removed, as when the account is removed; rules for every account stay */
void removeAccount(KConfigBase &config, const QString &accountId);

/** @short Keep rules pointing at a renamed folder: their scope and their Move actions follow @arg newName

Returns whether a rule changed.
*/
bool renameFolder(KConfigBase &config, const QString &accountId, const QString &oldName, const QString &newName);

}

}
