/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.ki18n
import io.github.mikepadiernos.plasmaimpresario as Impresario

KCM.SimpleKCM {
    id: page

    property var cfg_accounts: []
    property var cfg_accountsDefault: []
    property var cfg_folders: ["INBOX"]
    property var cfg_foldersDefault: ["INBOX"]
    property alias cfg_showSender: showSender.checked
    property bool cfg_showSenderDefault: true
    property alias cfg_showSubject: showSubject.checked
    property bool cfg_showSubjectDefault: true
    property alias cfg_showAccount: showAccount.checked
    property bool cfg_showAccountDefault: false
    property alias cfg_showFolder: showFolder.checked
    property bool cfg_showFolderDefault: false
    property alias cfg_showDate: showDate.checked
    property bool cfg_showDateDefault: false
    property alias cfg_scrolling: scrolling.checked
    property bool cfg_scrollingDefault: true
    property alias cfg_speed: speed.value
    property int cfg_speedDefault: 40
    property alias cfg_pageInterval: pageInterval.value
    property int cfg_pageIntervalDefault: 5

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

        QQC2.TextField {
            Kirigami.FormData.label: page.tr.i18nc("@label:textbox", "Folders:")
            placeholderText: page.tr.i18nc("@info:placeholder", "Every indexed folder")
            text: page.cfg_folders.join(", ")
            onTextEdited: page.cfg_folders = text.split(",").map(name => name.trim()).filter(name => name.length > 0)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: page.tr.i18nc("@info:tooltip", "Folder names separated by commas, as the server names them")
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: showSender

            Kirigami.FormData.label: page.tr.i18nc("@label", "Show:")
            text: page.tr.i18nc("@option:check", "Sender")
        }
        QQC2.CheckBox {
            id: showSubject

            text: page.tr.i18nc("@option:check", "Subject")
        }
        QQC2.CheckBox {
            id: showAccount

            text: page.tr.i18nc("@option:check", "Account")
        }
        QQC2.CheckBox {
            id: showFolder

            text: page.tr.i18nc("@option:check", "Folder")
        }
        QQC2.CheckBox {
            id: showDate

            text: page.tr.i18nc("@option:check", "Date")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: mail.loaded && mail.sharing !== "subjects"
            type: Kirigami.MessageType.Information
            text: mail.sharing === "senders"
                  ? page.tr.i18nc("@info", "Plasma Impresario shares senders but not subjects. Change this under Settings, General in the application.")
                  : page.tr.i18nc("@info", "Plasma Impresario shares only unread counts, so the line shows one per account. Change this under Settings, General in the application.")
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.CheckBox {
            id: scrolling

            Kirigami.FormData.label: page.tr.i18nc("@label", "Motion:")
            text: page.tr.i18nc("@option:check", "Scroll continuously")
        }
        QQC2.SpinBox {
            id: speed

            Kirigami.FormData.label: page.tr.i18nc("@label:spinbox", "Speed:")
            enabled: scrolling.checked
            from: 10
            to: 200
            stepSize: 10
            textFromValue: (value, locale) => page.tr.i18nc("@item:valuesuffix logical pixels per second", "%1 px/s", value)
            valueFromText: (text, locale) => parseInt(text)
        }
        QQC2.SpinBox {
            id: pageInterval

            Kirigami.FormData.label: page.tr.i18nc("@label:spinbox", "Show each message for:")
            enabled: !scrolling.checked
            from: 1
            to: 60
            textFromValue: (value, locale) => page.tr.i18ncp("@item:valuesuffix", "%1 second", "%1 seconds", value)
            valueFromText: (text, locale) => parseInt(text)
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            text: page.tr.i18nc("@info", "With animations turned off in Plasma's settings, the line always shows one message at a time.")
        }
    }
}
