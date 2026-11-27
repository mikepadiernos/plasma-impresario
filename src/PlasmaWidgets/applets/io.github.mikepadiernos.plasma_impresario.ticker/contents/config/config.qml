/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import org.kde.plasma.configuration
import org.kde.ki18n

ConfigModel {
    id: model

    readonly property KI18nContext tr: KI18nContext {
        translationDomain: "plasma-impresario"
    }

    ConfigCategory {
        name: model.tr.i18nc("@title:tab widget settings", "General")
        icon: "configure"
        source: "ConfigGeneral.qml"
    }
}
