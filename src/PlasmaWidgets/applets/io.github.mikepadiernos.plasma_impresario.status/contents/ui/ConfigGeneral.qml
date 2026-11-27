/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.ki18n
import io.github.mikepadiernos.plasmaimpresario as Impresario

KCM.SimpleKCM {
    id: page

    property var cfg_accounts: []
    property var cfg_accountsDefault: []

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    Impresario.MailStatusClient {
        id: mail
    }

    Kirigami.FormLayout {
        Impresario.AccountChooser {
            Kirigami.FormData.label: page.tr.i18nc("@label", "Accounts:")
            Kirigami.FormData.labelAlignment: Qt.AlignTop
            client: mail
            chosen: page.cfg_accounts
            onEdited: chosen => page.cfg_accounts = chosen
        }
    }
}
