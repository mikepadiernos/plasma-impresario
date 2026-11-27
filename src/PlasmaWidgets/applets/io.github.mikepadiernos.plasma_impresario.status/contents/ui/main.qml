/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.ki18n
import io.github.mikepadiernos.plasmaimpresario as Impresario

/*
    The unread mail of Plasma Impresario's accounts: an icon with the count in the panel, and each account with its
    state when opened. The application decides what the widget learns; see Settings, General in the application.
*/
PlasmoidItem {
    id: root

    readonly property int unread: mail.totalUnread

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    Impresario.MailStatusClient {
        id: mail

        accountFilter: Plasmoid.configuration.accounts
    }

    Plasmoid.icon: unread > 0 ? "mail-unread-new" : "mail-read"
    Plasmoid.status: unread > 0 ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    toolTipMainText: tr.i18nc("@info:tooltip", "Mail")
    toolTipSubText: {
        if (!mail.running)
            return tr.i18nc("@info:tooltip", "Plasma Impresario is not running");
        if (mail.sharing === "nothing")
            return tr.i18nc("@info:tooltip", "Plasma Impresario does not share its mail status");
        if (unread < 0)
            return "";
        if (unread === 0)
            return tr.i18nc("@info:tooltip", "No unread mail");
        return tr.i18ncp("@info:tooltip", "%1 unread message", "%1 unread messages", unread);
    }

    compactRepresentation: MouseArea {
        id: compact

        property bool wasExpanded: false

        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: root.toolTipSubText.length > 0 ? root.toolTipSubText : root.toolTipMainText
        onPressed: wasExpanded = root.expanded
        onClicked: root.expanded = !wasExpanded

        Kirigami.Icon {
            anchors.fill: parent
            source: Plasmoid.icon
            active: compact.containsMouse
            opacity: mail.stale ? 0.6 : 1
        }

        Rectangle {
            visible: root.unread > 0
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.min(parent.height / 2, badgeLabel.implicitHeight)
            width: Math.max(height, badgeLabel.implicitWidth + Kirigami.Units.smallSpacing)
            radius: height / 2
            color: Kirigami.Theme.highlightColor

            QQC2.Label {
                id: badgeLabel

                anchors.centerIn: parent
                text: root.unread > 99 ? root.tr.i18nc("@info more than 99 unread", "99+") : root.unread
                color: Kirigami.Theme.highlightedTextColor
                font: Kirigami.Theme.smallFont
            }
        }
    }

    fullRepresentation: Impresario.StatusSummary {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 14
        Layout.preferredWidth: Kirigami.Units.gridUnit * 18
        client: mail
    }
}
