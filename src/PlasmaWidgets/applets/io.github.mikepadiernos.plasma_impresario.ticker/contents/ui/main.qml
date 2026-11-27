/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import io.github.mikepadiernos.plasmaimpresario as Impresario

/*
    A line of the newest unread mail of Plasma Impresario, for a panel or the desktop. Senders and subjects appear only
    when the application shares them, and only from accounts with an offline search index; otherwise the line shows the
    unread count of each account.
*/
PlasmoidItem {
    id: root

    Impresario.MailStatusClient {
        id: mail

        wantsMessages: true
        accountFilter: Plasmoid.configuration.accounts
        folderFilter: Plasmoid.configuration.folders
    }

    preferredRepresentation: fullRepresentation
    Plasmoid.backgroundHints: PlasmaCore.Types.DefaultBackground | PlasmaCore.Types.ConfigurableBackground
    Plasmoid.status: mail.totalUnread > 0 ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    fullRepresentation: Impresario.MailTicker {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 8
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        Layout.fillWidth: true
        Layout.minimumHeight: implicitHeight
        client: mail
        showSender: Plasmoid.configuration.showSender
        showSubject: Plasmoid.configuration.showSubject
        showAccount: Plasmoid.configuration.showAccount
        showFolder: Plasmoid.configuration.showFolder
        showDate: Plasmoid.configuration.showDate
        scrolling: Plasmoid.configuration.scrolling
        speed: Plasmoid.configuration.speed
        pageInterval: Plasmoid.configuration.pageInterval * 1000
    }
}
