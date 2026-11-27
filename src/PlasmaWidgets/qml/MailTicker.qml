/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.ki18n

/*
    A line of unread mail which scrolls or pages through the newest messages, as the ticker widget shows it.

    The entries are the unread messages when the application shares them, otherwise the unread count of each account.
    Which parts of a message are shown is chosen here, but the application decides what it shares: a part it does not
    share is left out whatever is chosen.

    Scrolling stops while the pointer is over the line, so that an entry can be clicked, and gives way to paging when it
    has the keyboard focus or when animations are turned off in Plasma's settings. Left and Right step through the
    entries; Enter opens the one shown.
*/
Item {
    id: root

    property QtObject client

    property bool showSender: true
    property bool showSubject: true
    property bool showAccount: false
    property bool showFolder: false
    property bool showDate: false

    /* Scroll continuously rather than page */
    property bool scrolling: true
    /* Scrolling speed, in logical pixels per second */
    property int speed: 40
    /* How long each entry is shown when paging, in milliseconds */
    property int pageInterval: 5000

    /* The entry shown when paging, and the one Enter opens */
    property int currentIndex: 0

    /* Whether the line scrolls now */
    readonly property bool moving: scrolling && Kirigami.Units.longDuration > 0 && !activeFocus && entries.length > 0
    readonly property bool paused: hover.hovered

    readonly property var entries: {
        if (!client)
            return [];
        if (!client.running)
            return [notice(tr.i18nc("@info", "Plasma Impresario is not running"))];
        if (client.sharing === "nothing")
            return [notice(tr.i18nc("@info", "Plasma Impresario does not share its mail status"))];
        if (!client.loaded)
            return [notice(tr.i18nc("@info", "Loading…"))];
        const result = [];
        for (const message of client.messages)
            result.push({ id: message.id, account: message.account, text: entryText(message) });
        // Accounts without an offline search index give only their counts
        if (result.length === 0) {
            for (const account of client.accounts) {
                if (account.unread > 0)
                    result.push({ id: "", account: account.id,
                                  text: tr.i18ncp("@info account name and its unread count", "%2: %1 unread",
                                                  "%2: %1 unread", account.unread, account.name) });
            }
        }
        if (result.length === 0)
            return [notice(tr.i18nc("@info", "No unread mail"))];
        return result;
    }

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    function notice(text) {
        return { id: "", account: "", text: text };
    }

    /* The text of an unread message, with the parts chosen and shared */
    function entryText(message) {
        const parts = [];
        if (showAccount && message.accountName)
            parts.push(message.accountName);
        if (showSender && message.sender)
            parts.push(message.sender);
        if (showSubject && message.subject !== undefined)
            parts.push(message.subject.length > 0 ? message.subject : tr.i18nc("@info", "(No subject)"));
        if (showFolder && message.folder)
            parts.push(message.folder);
        if (showDate && message.sent) {
            const day = message.sent.split("-");
            const date = new Date(Number(day[0]), Number(day[1]) - 1, Number(day[2]));
            parts.push(Qt.formatDate(date, Qt.locale(), Locale.ShortFormat));
        }
        if (parts.length === 0)
            return tr.i18nc("@info an unread message whose details are not shown", "Unread message");
        return parts.join(" · ");
    }

    /* Open the entry at @p index: the message, the account, or the application */
    function open(index) {
        if (!client || index < 0 || index >= entries.length)
            return;
        const entry = entries[index];
        if (entry.id)
            client.openMessage(entry.id);
        else if (entry.account)
            client.showAccount(entry.account);
        else
            client.start();
    }

    function step(delta) {
        if (entries.length === 0)
            return;
        currentIndex = (currentIndex + delta + entries.length) % entries.length;
    }

    onEntriesChanged: {
        if (currentIndex >= entries.length)
            currentIndex = 0;
        if (scroll.running)
            scroll.restart();
    }

    implicitWidth: Kirigami.Units.gridUnit * 16
    implicitHeight: page.implicitHeight
    activeFocusOnTab: true
    opacity: client && client.stale && client.loaded && client.running ? 0.6 : 1

    Accessible.role: Accessible.Button
    Accessible.name: entries.length > 0 ? entries[Math.min(currentIndex, entries.length - 1)].text : ""
    Accessible.description: tr.i18nc("@info:accessible", "Unread mail. Left and Right step through the messages; Enter opens one.")
    Accessible.onPressAction: open(currentIndex)

    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)
    Keys.onReturnPressed: open(currentIndex)
    Keys.onEnterPressed: open(currentIndex)
    Keys.onSpacePressed: open(currentIndex)

    HoverHandler {
        id: hover
    }

    Item {
        id: scroller

        anchors.fill: parent
        visible: root.moving
        clip: true

        Row {
            id: strip

            objectName: "strip"
            anchors.verticalCenter: parent.verticalCenter
            spacing: Kirigami.Units.gridUnit * 2

            Repeater {
                model: root.moving ? root.entries : []

                delegate: QQC2.Label {
                    id: entryLabel

                    required property var modelData
                    required property int index

                    text: modelData.text

                    TapHandler {
                        onTapped: root.open(entryLabel.index)
                    }
                }
            }
        }

        NumberAnimation {
            id: scroll

            target: strip
            property: "x"
            from: scroller.width
            to: -strip.width
            duration: Math.max(1, (scroller.width + strip.width) / Math.max(1, root.speed) * 1000)
            loops: Animation.Infinite
            running: root.moving
            paused: running && root.paused
        }
    }

    QQC2.Label {
        id: page

        objectName: "page"
        anchors.fill: parent
        visible: !root.moving
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: root.entries.length > 0 ? root.entries[Math.min(root.currentIndex, root.entries.length - 1)].text : ""

        TapHandler {
            onTapped: root.open(root.currentIndex)
        }
    }

    Timer {
        objectName: "pager"
        interval: Math.max(1000, root.pageInterval)
        repeat: true
        running: !root.moving && !root.paused && !root.activeFocus && root.entries.length > 1
        onTriggered: root.step(1)
    }

    Rectangle {
        anchors.fill: parent
        visible: root.activeFocus
        color: "transparent"
        border.width: 1
        border.color: Kirigami.Theme.focusColor
        radius: Kirigami.Units.cornerRadius
    }
}
