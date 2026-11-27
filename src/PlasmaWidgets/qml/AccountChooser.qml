/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.ki18n

/*
    The accounts a widget shows, chosen in its settings. With none chosen, every account is shown, including accounts
    added later. The list comes from the running application; IDs chosen earlier stay chosen while it is not running.
*/
ColumnLayout {
    id: root

    property QtObject client
    /* The chosen account IDs; empty chooses every account */
    property var chosen: []

    /* The user changed the choice to @p chosen */
    signal edited(var chosen)

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    spacing: Kirigami.Units.smallSpacing

    Repeater {
        objectName: "accounts"
        model: root.client ? root.client.allAccounts : []

        delegate: QQC2.CheckBox {
            id: box

            required property var modelData

            text: modelData.name
            checked: root.chosen.indexOf(modelData.id) >= 0
            onToggled: {
                const next = root.chosen.filter(id => id !== box.modelData.id);
                if (checked)
                    next.push(box.modelData.id);
                root.edited(next);
            }
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font: Kirigami.Theme.smallFont
        text: !root.client || !root.client.running
              ? root.tr.i18nc("@info", "Start Plasma Impresario to choose its accounts.")
              : root.tr.i18nc("@info", "With none chosen, every account is shown.")
    }
}
