#!/usr/bin/env python3
# Copyright (C) 2026 Plasma Impresario contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stand-ins for the Plasma desktop services the end-to-end scenarios check.

Usage: desktop_mocks.py LOG

Runs on the session bus until terminated and appends one JSON object per event
to LOG:

  org.freedesktop.Notifications   every Notify and CloseNotification call, as a
                                  notification server such as Plasma's would
                                  receive it
  org.kde.StatusNotifierWatcher   every tray item that registers or goes away,
                                  with a host registered so that Qt offers its
                                  StatusNotifierItem tray icon

Nothing is shown; the scenarios read LOG and talk to the registered tray item
over D-Bus themselves.
"""

import json
import sys
import time

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

NOTIFICATIONS = "org.freedesktop.Notifications"
WATCHER = "org.kde.StatusNotifierWatcher"
PROPERTIES = "org.freedesktop.DBus.Properties"


class Log:
    def __init__(self, path):
        self.path = path

    def write(self, **event):
        event["time"] = time.time()
        with open(self.path, "a", encoding="utf-8") as log:
            log.write(json.dumps(event) + "\n")


class Notifications(dbus.service.Object):
    def __init__(self, bus, log):
        super().__init__(bus, "/org/freedesktop/Notifications")
        self.log = log
        self.next_id = 1

    @dbus.service.method(NOTIFICATIONS, out_signature="as")
    def GetCapabilities(self):
        return ["actions", "body", "body-markup", "icon-static", "persistence"]

    @dbus.service.method(NOTIFICATIONS, out_signature="ssss")
    def GetServerInformation(self):
        return ("Plasma Impresario scenario mock", "example.test", "1.0", "1.2")

    @dbus.service.method(NOTIFICATIONS, in_signature="susssasa{sv}i", out_signature="u")
    def Notify(self, app_name, replaces_id, app_icon, summary, body, actions, hints, timeout):
        notification_id = int(replaces_id) or self.next_id
        if not replaces_id:
            self.next_id += 1
        self.log.write(kind="notify", id=notification_id, app=str(app_name), icon=str(app_icon),
                       summary=str(summary), body=str(body), actions=[str(a) for a in actions],
                       desktop_entry=str(hints.get("desktop-entry", "")))
        return dbus.UInt32(notification_id)

    @dbus.service.method(NOTIFICATIONS, in_signature="u")
    def CloseNotification(self, notification_id):
        self.log.write(kind="close", id=int(notification_id))
        self.NotificationClosed(notification_id, 3)

    @dbus.service.signal(NOTIFICATIONS, signature="uu")
    def NotificationClosed(self, notification_id, reason):
        pass

    @dbus.service.signal(NOTIFICATIONS, signature="us")
    def ActionInvoked(self, notification_id, action_key):
        pass


class Watcher(dbus.service.Object):
    def __init__(self, bus, log):
        super().__init__(bus, "/StatusNotifierWatcher")
        self.bus = bus
        self.log = log
        self.items = []
        bus.add_signal_receiver(self.owner_changed, "NameOwnerChanged", "org.freedesktop.DBus")

    def owner_changed(self, name, old, new):
        gone = [item for item in self.items if not new and item.split("/")[0] in (name, old)]
        for item in gone:
            self.items.remove(item)
            self.log.write(kind="tray-unregistered", item=item)
            self.StatusNotifierItemUnregistered(item)

    @dbus.service.method(WATCHER, in_signature="s", sender_keyword="sender")
    def RegisterStatusNotifierItem(self, service, sender=None):
        # Qt passes its own bus name; others pass an object path on the caller's connection
        item = service if not service.startswith("/") else sender + service
        if "/" not in item:
            item += "/StatusNotifierItem"
        if item not in self.items:
            self.items.append(item)
        self.log.write(kind="tray-registered", item=item, sender=str(sender))
        self.StatusNotifierItemRegistered(item)

    @dbus.service.method(WATCHER, in_signature="s")
    def RegisterStatusNotifierHost(self, service):
        self.StatusNotifierHostRegistered()

    @dbus.service.signal(WATCHER, signature="s")
    def StatusNotifierItemRegistered(self, item):
        pass

    @dbus.service.signal(WATCHER, signature="s")
    def StatusNotifierItemUnregistered(self, item):
        pass

    @dbus.service.signal(WATCHER)
    def StatusNotifierHostRegistered(self):
        pass

    def properties(self):
        return {
            "RegisteredStatusNotifierItems": dbus.Array(self.items, signature="s"),
            "IsStatusNotifierHostRegistered": dbus.Boolean(True),
            "ProtocolVersion": dbus.Int32(0),
        }

    @dbus.service.method(PROPERTIES, in_signature="ss", out_signature="v")
    def Get(self, interface, name):
        return self.properties()[name]

    @dbus.service.method(PROPERTIES, in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        return self.properties() if interface == WATCHER else {}


def main():
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    log = Log(sys.argv[1])
    keep = [Notifications(bus, log), Watcher(bus, log)]
    names = [dbus.service.BusName(NOTIFICATIONS, bus), dbus.service.BusName(WATCHER, bus)]
    log.write(kind="ready")
    GLib.MainLoop().run()
    del keep, names


if __name__ == "__main__":
    main()
