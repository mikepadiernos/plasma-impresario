/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.ki18n

/*
    The unread mail of each account and its state, as the status widget shows them when opened.

    Everything comes from the client, a MailStatusClient or an object with the same properties. When the data are not
    current, a notice says why; the counts stay visible for reference.
*/
ColumnLayout {
    id: root

    property QtObject client

    /* Why the data are missing or not current; empty when they are current */
    readonly property string notice: {
        if (!client)
            return "";
        if (!client.running)
            return client.loaded ? tr.i18nc("@info", "Plasma Impresario is not running. The counts below may be out of date.")
                                 : tr.i18nc("@info", "Plasma Impresario is not running.");
        if (client.sharing === "nothing")
            return tr.i18nc("@info", "Plasma Impresario does not share its mail status. Choose what to share under Settings, General.");
        if (client.loaded && client.stale)
            return tr.i18nc("@info", "Plasma Impresario is not answering. The counts below may be out of date.");
        if (client.loaded && client.accounts.length === 0)
            return tr.i18nc("@info", "No accounts to show.");
        return "";
    }

    function stateText(state) {
        switch (state) {
        case "connecting":
            return tr.i18nc("@info account state", "Connecting");
        case "online":
            return tr.i18nc("@info account state", "Online");
        case "offline":
            return tr.i18nc("@info account state", "Offline");
        case "signInNeeded":
            return tr.i18nc("@info account state", "Sign-in needed");
        case "failed":
            return tr.i18nc("@info account state", "Connection failed");
        default:
            return tr.i18nc("@info account state", "Not connected yet");
        }
    }

    function unreadText(unread) {
        return unread < 0 ? tr.i18nc("@info unread count not known", "Unknown")
                          : tr.i18ncp("@info", "%1 unread", "%1 unread", unread);
    }

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    spacing: Kirigami.Units.smallSpacing

    Kirigami.Heading {
        objectName: "heading"
        Layout.fillWidth: true
        level: 3
        wrapMode: Text.Wrap
        text: !root.client || root.client.totalUnread < 0 ? tr.i18nc("@title", "Mail")
              : root.client.totalUnread === 0 ? tr.i18nc("@title", "No unread mail")
              : tr.i18ncp("@title", "%1 unread message", "%1 unread messages", root.client.totalUnread)
    }

    Kirigami.InlineMessage {
        objectName: "notice"
        Layout.fillWidth: true
        visible: root.notice.length > 0
        text: root.notice
        type: root.client && root.client.running && root.client.stale && root.client.loaded
              ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
        actions: [
            Kirigami.Action {
                visible: root.client !== null && !root.client.running
                icon.name: "system-run"
                text: tr.i18nc("@action:button", "Start Plasma Impresario")
                onTriggered: root.client.start()
            }
        ]
    }

    QQC2.BusyIndicator {
        objectName: "loading"
        Layout.alignment: Qt.AlignHCenter
        visible: root.client !== null && root.client.running && !root.client.loaded
        running: visible
    }

    Repeater {
        objectName: "accounts"
        model: root.client ? root.client.accounts : []

        delegate: QQC2.ItemDelegate {
            id: row

            required property var modelData

            Layout.fillWidth: true
            enabled: root.client.running
            opacity: root.client.stale ? 0.6 : 1
            Accessible.name: root.tr.i18nc("@info:accessible account name, unread count, state", "%1, %2, %3",
                                      modelData.name, root.unreadText(modelData.unread), root.stateText(modelData.state))
            onClicked: root.client.showAccount(modelData.id)

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: root.stateText(row.modelData.state)
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }

                QQC2.Label {
                    text: row.modelData.unread < 0 ? "–" : row.modelData.unread
                    font.bold: row.modelData.unread > 0
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            objectName: "compose"
            icon.name: "mail-message-new"
            text: tr.i18nc("@action:button", "Compose")
            enabled: root.client !== null && root.client.running
            onClicked: root.client.compose()
        }
        QQC2.Button {
            objectName: "open"
            icon.name: "io.github.mikepadiernos.plasma_impresario"
            text: tr.i18nc("@action:button", "Open Plasma Impresario")
            enabled: root.client !== null
            onClicked: root.client.start()
        }
    }
}
