#!/usr/bin/env python3
# Copyright (C) 2026 Plasma Impresario contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Drive the installed Plasma Impresario through AT-SPI and the keyboard.

Usage: scenarios.py [--only NAME,...] OUT

Run it through tools/dev-container/e2e-scenarios.sh, which starts the X
display, the session and AT-SPI buses, the desktop stand-ins of
desktop_mocks.py and a synthetic Dovecot, and exports the variables below.
Every scenario writes screenshots to OUT and one line to OUT/report.tsv and
OUT/report.md. A scenario that fails leaves a dump of the accessible tree, and
the application is stopped and started again from the prepared profile before
the next one.

Environment:
  E2E_APP           the installed executable
  E2E_HOME          the application's home folder, recreated for each start
  E2E_PROFILE       a prepared profile with the synthetic account, copied into
                    E2E_HOME for every scenario except setup
  E2E_MAILDIR       the synthetic user's Maildir on the server
  E2E_MOCK_LOG      the event log of desktop_mocks.py
  E2E_SENT          the file the sendmail stand-in writes each message to
  E2E_IMAP_PORT     the server's IMAPS port
  E2E_USER, E2E_PASSWORD   the synthetic account

Every address, name and message is invented, on the reserved example.test
domain, so the screenshots can be published.
"""

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import traceback

import dbus
import pyatspi
from gi.repository import Atspi
from PIL import ImageGrab

APP_ID = "io.github.mikepadiernos.plasma_impresario"
DISPLAY = os.environ.get("DISPLAY", ":0")
WINDOW_SIZE = (1560, 940)  # on the 1600x1000 display of e2e-scenarios.sh


class Failure(Exception):
    pass


def no_cache():
    """Reads every property from the application, since nothing here runs the event loop that keeps a cache current"""
    pyatspi.Registry.getDesktop(0).set_cache_mask(Atspi.Cache.NONE)


def check(condition, message):
    if not condition:
        raise Failure(message)


def wait_for(probe, timeout, message, interval=0.25):
    """Calls probe until it returns something true and returns that, or fails after timeout seconds"""
    deadline = time.monotonic() + timeout
    last_error = None
    while True:
        try:
            result = probe()
            if result:
                return result
        except Exception as error:  # the tree changes under the probe while the app works
            last_error = error
        if time.monotonic() > deadline:
            detail = f" (last error: {last_error})" if last_error else ""
            raise Failure(f"timed out after {timeout} s: {message}{detail}")
        time.sleep(interval)


def xdotool(*arguments):
    subprocess.run(["xdotool", *arguments], check=True)


def key(*keys):
    xdotool("key", "--clearmodifiers", *keys)
    time.sleep(0.2)


def type_text(text):
    xdotool("type", "--delay", "15", "--clearmodifiers", text)
    time.sleep(0.2)


# Accessible tree helpers


def accessible_id(accessible):
    try:
        return accessible.accessibleId or ""
    except Exception:
        return ""


def showing(accessible):
    try:
        return accessible.getState().contains(pyatspi.STATE_SHOWING)
    except Exception:
        return False


def children(accessible):
    try:
        count = accessible.childCount
    except Exception:
        return []
    result = []
    for index in range(count):
        try:
            child = accessible.getChildAtIndex(index)
        except Exception:
            continue
        if child is not None:
            result.append(child)
    return result


def find_all(root, object_name=None, role=None, name=None, name_contains=None, visible=True, limit=None):
    """Breadth-first search below root; object_name matches the last part of the accessible id"""
    found = []
    queue = [root]
    while queue:
        accessible = queue.pop(0)
        if visible and accessible is not root and not showing(accessible):
            continue
        matches = True
        if object_name is not None:
            aid = accessible_id(accessible)
            matches = aid == object_name or aid.endswith("." + object_name)
        if matches and role is not None:
            matches = accessible.getRoleName() == role
        if matches and name is not None:
            matches = accessible.name == name
        if matches and name_contains is not None:
            matches = name_contains in (accessible.name or "")
        if matches and accessible is not root:
            found.append(accessible)
            if limit and len(found) >= limit:
                return found
        queue.extend(children(accessible))
    return found


def find(root, **criteria):
    found = find_all(root, limit=1, **criteria)
    return found[0] if found else None


def extents(accessible):
    box = accessible.queryComponent().getExtents(pyatspi.DESKTOP_COORDS)
    return box.x, box.y, box.width, box.height


def click(accessible, double=False):
    """Clicks the middle of a control with the pointer, as a person would"""
    x, y, width, height = extents(accessible)
    check(width > 0 and height > 0, f"{describe(accessible)} has no area on screen")
    xdotool("mousemove", "--sync", str(x + width // 2), str(y + height // 2))
    xdotool("click", "--repeat", "2" if double else "1", "1")
    time.sleep(0.3)


def press(accessible):
    """Runs a control's default action, as a screen reader would"""
    action = accessible.queryAction()
    check(action.nActions > 0, f"{describe(accessible)} has no action")
    action.doAction(0)
    time.sleep(0.3)


def checked(accessible):
    return accessible.getState().contains(pyatspi.STATE_CHECKED)


def set_checked(accessible, value):
    if checked(accessible) != value:
        press(accessible)
        wait_for(lambda: checked(accessible) == value, 3, f"{describe(accessible)} to change")


def fill(accessible, text, leave=True):
    """Focuses a text field, replaces its contents by typing, and leaves it with Tab"""
    accessible.queryComponent().grabFocus()
    wait_for(lambda: accessible.getState().contains(pyatspi.STATE_FOCUSED), 3, f"focus on {describe(accessible)}")
    key("ctrl+a")
    key("BackSpace")
    if text:
        type_text(text)
    if leave:
        key("Tab")


def text_of(accessible):
    try:
        text = accessible.queryText()
        return text.getText(0, text.characterCount)
    except Exception:
        return accessible.name or ""


def describe(accessible):
    try:
        return f"{accessible.getRoleName()} {accessible.name!r} ({accessible_id(accessible).split('.')[-1]})"
    except Exception:
        return "a vanished control"


def dump(root, path):
    with open(path, "w", encoding="utf-8") as out:
        def walk(accessible, depth):
            state = "" if showing(accessible) else " [hidden]"
            out.write(f"{'  ' * depth}{accessible.getRoleName()} name={accessible.name!r} "
                      f"id={accessible_id(accessible).split('.')[-1]!r}{state}\n")
            if depth < 60:
                for child in children(accessible):
                    walk(child, depth + 1)
        try:
            walk(root, 0)
        except Exception as error:
            out.write(f"dump stopped: {error}\n")


def rows(tree):
    """The text of each row of a list or tree view, its columns joined by |"""
    table = tree.queryTable()
    result = []
    for row in range(table.nRows):
        cells = []
        for column in range(table.nColumns):
            cell = table.getAccessibleAt(row, column)
            cells.append(cell.name if cell is not None else "")
        result.append(" | ".join(cell for cell in cells if cell))
    return result


def reveal(tree, cell):
    """Scrolls a list with the mouse wheel until cell is inside the visible area, without selecting rows"""
    def inside():
        try:
            x, y, width, height = extents(cell)
            tx, ty, twidth, theight = extents(tree)
        except Exception:
            return False
        return height > 0 and y >= ty and y + height <= ty + theight

    if inside():
        return cell
    tx, ty, twidth, theight = extents(tree)
    xdotool("mousemove", "--sync", str(tx + twidth // 2), str(ty + theight // 2))
    # Up to the top first, then down a notch at a time; the wheel scrolls without selecting rows
    xdotool("click", "--repeat", "40", "--delay", "10", "4")
    for _ in range(60):
        time.sleep(0.1)
        if inside():
            return cell
        xdotool("click", "5")
    raise Failure(f"could not scroll {describe(cell)} into view")


def row_cell(tree, text):
    """The first cell of the first row whose text contains text"""
    table = tree.queryTable()
    for row in range(table.nRows):
        for column in range(table.nColumns):
            cell = table.getAccessibleAt(row, column)
            if cell is not None and text in (cell.name or ""):
                return table.getAccessibleAt(row, 0) if showing(table.getAccessibleAt(row, 0)) else cell
    return None


# The application


class Session:
    def __init__(self, out):
        self.out = out
        self.process = None
        self.app = None
        self.scenario = None
        self.shots = []
        self.log = None

    def env(self):
        home = os.environ["E2E_HOME"]
        env = dict(os.environ)
        env.update({
            "HOME": home,
            "XDG_CONFIG_HOME": f"{home}/.config",
            "XDG_DATA_HOME": f"{home}/.local/share",
            "XDG_CACHE_HOME": f"{home}/.cache",
            "XDG_STATE_HOME": f"{home}/.local/state",
            "QT_QPA_PLATFORM": "xcb",
            "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
        })
        return env

    def reset_profile(self, empty=False):
        home = os.environ["E2E_HOME"]
        shutil.rmtree(home, ignore_errors=True)
        if empty:
            os.makedirs(f"{home}/.config/flaska.net")
            with open(f"{home}/.config/flaska.net/trojita.conf", "w", encoding="utf-8") as config:
                # The container has no Secret Service; passwords stay in the profile, like the live tests
                config.write("[plugin]\npassword=cleartextpassword\n")
        else:
            shutil.copytree(os.environ["E2E_PROFILE"], home, symlinks=True)

    def start(self, empty=False, reset=True):
        if reset:
            self.reset_profile(empty)
        log_path = os.path.join(self.out, "app.log")
        self.log = open(log_path, "a", encoding="utf-8")
        self.log.write(f"\n=== start for {self.scenario} at {time.strftime('%H:%M:%S')}\n")
        self.log.flush()
        self.process = subprocess.Popen([os.environ["E2E_APP"]], env=self.env(), stdout=self.log,
                                        stderr=subprocess.STDOUT, start_new_session=True)
        self.app = wait_for(self.find_app, 30, "the application to appear on the accessibility bus")
        return self.app

    def find_app(self):
        check(self.process.poll() is None, f"the application exited with {self.process.returncode}")
        desktop = pyatspi.Registry.getDesktop(0)
        for app in children(desktop):
            if accessible_id(app) == APP_ID and app.get_process_id() == self.process.pid:
                if find(app, role="frame", visible=False):
                    return app
        return None

    def stop(self, timeout=15):
        if self.process and self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGTERM)
            try:
                self.process.wait(timeout)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait()
        self.process = None
        self.app = None
        if self.log:
            self.log.close()
            self.log = None

    def running(self):
        return self.process is not None and self.process.poll() is None

    def window(self, title_contains="Plasma Impresario"):
        return find(self.app, role="frame", name_contains=title_contains)

    def main_window(self):
        window = wait_for(lambda: find(self.app, object_name="Gui::MainWindow", role="frame"), 20,
                          "the main window")
        self.enlarge(window)
        return window

    def enlarge(self, window):
        """Gives the main window room for every pane, so rows need less scrolling.

        Under Xvfb and Openbox, the main window's X window is replaced when the first message opens, and the
        new one has the size from before; so this checks the size each time.
        """
        if extents(window)[2] >= WINDOW_SIZE[0]:
            return
        found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(self.process.pid)],
                               capture_output=True, text=True).stdout.split()
        best, area = None, 0
        for candidate in found:
            geometry = subprocess.run(["xdotool", "getwindowgeometry", "--shell", candidate],
                                      capture_output=True, text=True).stdout
            values = dict(line.split("=", 1) for line in geometry.split() if "=" in line)
            size = int(values.get("WIDTH", 0)) * int(values.get("HEIGHT", 0))
            if size > area:
                best, area = candidate, size
        if best is not None:
            xdotool("windowmove", "--sync", best, "20", "20")
            xdotool("windowsize", "--sync", best, *map(str, WINDOW_SIZE))
            wait_for(lambda: extents(window)[2] >= WINDOW_SIZE[0], 5, "the main window to grow")

    def screenshot(self, label, accessible=None):
        """Saves the window of accessible, or the whole display, as OUT/<scenario>-<label>.png"""
        name = f"{self.scenario}-{label}.png"
        bbox = None
        if accessible is not None:
            try:
                if accessible_id(accessible).endswith("Gui::MainWindow"):
                    self.enlarge(accessible)
                x, y, width, height = extents(accessible)
                if width > 0 and height > 0:
                    bbox = (max(x, 0), max(y, 0), x + width, y + height)
            except Exception:
                bbox = None
        time.sleep(0.4)  # let the last repaint reach the display
        ImageGrab.grab(bbox=bbox, xdisplay=DISPLAY).save(os.path.join(self.out, name))
        self.shots.append(name)
        return name

    def events(self, kind=None):
        """The events desktop_mocks.py recorded"""
        result = []
        with open(os.environ["E2E_MOCK_LOG"], encoding="utf-8") as log:
            for line in log:
                event = json.loads(line)
                if kind is None or event["kind"] == kind:
                    result.append(event)
        return result

    # Common steps

    def message_list(self):
        return wait_for(lambda: find(self.app, object_name="messageListView", role="tree"), 20, "the message list")

    def folders(self):
        return wait_for(lambda: find(self.app, object_name="Gui::MailBoxTreeView", visible=True)
                        or find(self.app, role="tree", name_contains="Folders"), 20, "the folder tree")

    def open_folder(self, name):
        tree = self.folders()
        item = wait_for(lambda: self.folder_item(name), 30, f"the {name} folder in the folder tree")
        if not item.getState().contains(pyatspi.STATE_SELECTED):
            click(item)
        wait_for(lambda: self.folder_item(name).getState().contains(pyatspi.STATE_SELECTED), 10,
                 f"the {name} folder to open")

    def folder_item(self, name):
        tree = self.folders()
        return next((cell for cell in find_all(tree, role="tree item") + find_all(tree, role="table cell")
                     if (cell.name or "").split(" (")[0] == name), None)

    def open_message(self, subject):
        tree = self.message_list()
        cell = wait_for(lambda: row_cell(tree, subject), 30, f"the message {subject!r} in the list")
        click(reveal(tree, cell))
        return cell


# Scenarios


def scenario_setup(session):
    """A first start with an empty profile: the account settings open and are filled in by keyboard"""
    session.start(empty=True)
    dialog = wait_for(lambda: find(session.app, object_name="ImapPage", visible=False) and
                      next((frame for frame in find_all(session.app, role="dialog")
                            if find(frame, object_name="ImapPage", visible=False)), None),
                      30, "the account settings to open on the first start")
    session.screenshot("settings", dialog)

    def page(tab):
        tab_item = wait_for(lambda: find(dialog, role="page tab", name=tab), 10, f"the {tab} tab")
        click(tab_item)
        wait_for(lambda: tab_item.getState().contains(pyatspi.STATE_SELECTED), 5, f"the {tab} tab to open")

    # An identity, through its own dialog
    page("Identities")
    press(wait_for(lambda: find(dialog, object_name="addButton"), 10, "the Add identity button"))
    identity = wait_for(lambda: next((frame for frame in find_all(session.app, role="dialog")
                                      if find(frame, object_name="emailLineEdit")), None),
                        10, "the identity dialog")
    fill(find(identity, object_name="realNameLineEdit"), "Panels User")
    fill(find(identity, object_name="emailLineEdit"), os.environ["E2E_USER"])
    session.screenshot("identity", identity)
    press(find(identity, role="button", name="OK"))

    page("IMAP")
    fill(find(dialog, object_name="imapHost"), "127.0.0.1")
    fill(find(dialog, object_name="imapPort"), os.environ["E2E_IMAP_PORT"])
    fill(find(dialog, object_name="imapUser"), os.environ["E2E_USER"])
    password = find(dialog, object_name="imapPass")
    wait_for(lambda: password.getState().contains(pyatspi.STATE_ENABLED), 10, "the password field")
    fill(password, os.environ["E2E_PASSWORD"])
    set_checked(find(dialog, object_name="imapNeedsNetwork"), False)
    set_checked(find(dialog, object_name="imapUseSystemProxy"), False)
    session.screenshot("imap", dialog)

    # Saving validates every page, and the network SMTP method needs a server
    page("SMTP")
    fill(find(dialog, object_name="smtpHost"), "127.0.0.1")
    session.screenshot("smtp", dialog)
    press(find(dialog, role="button", name="Save"))

    # The disposable server's certificate is new, so the application asks before trusting it
    trusted = False
    deadline = time.monotonic() + 30
    inbox = None
    while time.monotonic() < deadline and inbox is None:
        for frame in find_all(session.app, role="dialog") + find_all(session.app, role="alert"):
            yes = find(frame, role="button", name="Yes")
            if yes is not None and not trusted:
                session.screenshot("certificate", frame)
                press(yes)
                trusted = True
        inbox = find(session.folders(), name_contains="INBOX")
        time.sleep(0.5)
    check(inbox is not None, "INBOX did not appear after saving the account")
    session.open_folder("INBOX")
    wait_for(lambda: row_cell(session.message_list(), "Quarterly plan"), 30, "the seeded messages")
    session.screenshot("inbox", session.main_window())
    config = os.path.join(os.environ["E2E_HOME"], ".config/flaska.net")
    saved = "".join(open(os.path.join(config, name), encoding="utf-8").read()
                    for name in os.listdir(config) if name.endswith(".conf"))
    check("127.0.0.1" in saved and os.environ["E2E_USER"] in saved, "the account was not saved to the profile")
    return "account created from the first-start settings; certificate " + \
        ("confirmed" if trusted else "not asked about")


def scenario_reading(session):
    """Opening a message with attachments fills the reader and the embedded panels"""
    window = session.main_window()
    session.open_folder("INBOX")
    session.open_message("Quarterly plan")
    attachments = wait_for(lambda: find(session.app, object_name="attachmentPanelList"), 20,
                           "the attachments panel list")
    names = wait_for(lambda: [child.name for child in children(attachments)]
                     if len(children(attachments)) >= 2 else None, 30, "two attachments in the panel")
    check(any("plan.pdf" in name for name in names), f"plan.pdf is not listed: {names}")
    check(any("budget.csv" in name for name in names), f"budget.csv is not listed: {names}")
    button = find(session.app, object_name="readerAttachments")
    check(button is not None and "2" in button.name, f"the reader's attachment button reads {button and button.name!r}")
    session.screenshot("message", window)
    return f"attachments listed: {', '.join(names)}"


def scenario_hostile(session):
    """HTML mail with a remote image and a misleading link opens with remote content blocked"""
    window = session.main_window()
    session.open_folder("INBOX")
    session.open_message("Weekly news")
    bar = wait_for(lambda: find(session.app, object_name="remoteContentMessage", visible=True), 30,
                   "the remote content notice")
    load = find(session.app, role="button", name="Load Remote Content")
    check(load is not None, "the Load Remote Content button is missing")
    session.screenshot("blocked", window)
    return f"notice shown: {bar.name!r}"


def scenario_search(session):
    """The quick search narrows the list and clearing it brings every message back"""
    window = session.main_window()
    session.open_folder("INBOX")
    tree = session.message_list()
    before = wait_for(lambda: rows(tree) if len(rows(tree)) >= 6 else None, 30, "the INBOX messages")
    search = find(session.app, object_name="quickSearch")
    fill(search, "Budget", leave=False)
    key("Return")
    matching = wait_for(lambda: (lambda current: current if 0 < len(current) < len(before) else None)(rows(tree)),
                        30, "the list to narrow to the matching messages")
    check(all("Budget" in row or "budget" in row.lower() for row in matching) or
          any("Budget numbers" in row for row in matching), f"unexpected results: {matching}")
    session.screenshot("results", window)
    fill(search, "", leave=False)
    key("Return")
    wait_for(lambda: len(rows(tree)) == len(before), 30, "every message to return after clearing the search")
    return f"{len(matching)} of {len(before)} messages matched"


def scenario_delete_undo(session):
    """Moving a message to Trash offers Undo, which brings it back"""
    window = session.main_window()
    session.open_folder("INBOX")
    tree = session.message_list()
    session.open_message("Lunch on Friday")
    wait_for(lambda: find(session.app, object_name="readerTrash"), 10, "the Move to Trash button")
    press(find(session.app, object_name="readerTrash"))
    wait_for(lambda: row_cell(tree, "Lunch on Friday") is None, 30, "the message to leave the list")
    undo = wait_for(lambda: find(session.app, role="button", name="Undo"), 30, "the Undo button")
    session.screenshot("moved", window)
    press(undo)
    wait_for(lambda: row_cell(tree, "Lunch on Friday"), 30, "the message to come back after Undo")
    session.screenshot("restored", window)
    return "moved to Trash and restored with Undo"


def scenario_compose(session):
    """A new message typed in the composer goes to the sendmail stand-in and to Sent"""
    session.main_window()
    sent = os.environ["E2E_SENT"]
    if os.path.exists(sent):
        os.remove(sent)
    key("ctrl+n")
    composer = wait_for(lambda: find(session.app, object_name="ComposeWidget"), 20, "the composer")
    recipient = wait_for(lambda: find(composer, role="text", name="To address"), 10, "a recipient field")
    fill(recipient, "alice@example.test")
    fill(find(composer, object_name="subject"), "Scenario check-in")
    body = wait_for(lambda: find(composer, object_name="mailText"), 10, "the message body")
    body.queryComponent().grabFocus()
    type_text("Hello Alice, this message was typed by the scenario runner.")
    session.screenshot("composer", composer)
    send = wait_for(lambda: find(composer, role="button", name="Send"), 10, "the Send button")
    press(send)
    wait_for(lambda: os.path.exists(sent) and b"Scenario check-in" in open(sent, "rb").read(), 60,
             "the message to reach the sendmail stand-in")
    sent_dir = os.path.join(os.environ["E2E_MAILDIR"], ".Sent")
    wait_for(lambda: any(b"Scenario check-in" in open(os.path.join(sent_dir, part, name), "rb").read()
                         for part in ("cur", "new") for name in os.listdir(os.path.join(sent_dir, part))),
             60, "a copy in the Sent folder")
    return "sent through sendmail with a copy in Sent"


def tray_item(session):
    registered = [event["item"] for event in session.events("tray-registered")]
    check(registered, "no tray item registered with the StatusNotifierWatcher")
    service, _, path = registered[-1].partition("/")
    return dbus.SessionBus().get_object(service, "/" + path)


def main_window_showing(session):
    window = find(session.app, object_name="Gui::MainWindow", role="frame", visible=False)
    return window is not None and showing(window)


def scenario_tray(session):
    """The tray icon registers with the desktop, shows the unread count, and hides and shows the window"""
    window = session.main_window()
    item = wait_for(lambda: tray_item(session), 20, "the tray icon")
    properties = dbus.Interface(item, "org.freedesktop.DBus.Properties")
    tooltip = properties.Get("org.kde.StatusNotifierItem", "ToolTip")
    title = str(tooltip[2]) + " " + str(tooltip[3])
    check("Plasma Impresario" in title, f"unexpected tray tooltip {title!r}")
    # The unread count is a suffix of the application name, after a separator
    check("unread" not in title or " - " in title, f"the tray tooltip runs its parts together: {title!r}")
    session.screenshot("before", window)
    activate = dbus.Interface(item, "org.kde.StatusNotifierItem")
    activate.Activate(10, 10)
    wait_for(lambda: not main_window_showing(session), 10, "the window to hide on activation")
    activate.Activate(10, 10)
    wait_for(lambda: main_window_showing(session), 10, "the window to come back on activation")
    session.screenshot("shown", session.main_window())
    return f"tooltip {title.strip()!r}; activation hid and showed the window"


def scenario_notifications(session):
    """Mail delivered while the window is hidden raises a desktop notification"""
    session.main_window()
    session.open_folder("INBOX")
    item = wait_for(lambda: tray_item(session), 20, "the tray icon")
    activate = dbus.Interface(item, "org.kde.StatusNotifierItem")
    activate.Activate(10, 10)
    wait_for(lambda: not main_window_showing(session), 10, "the window to hide")
    before = len(session.events("notify"))
    deliver(os.environ["E2E_MAILDIR"], "scenario-arrival", "Harbor tour tickets",
            "Henry Example <henry@example.test>")
    note = wait_for(lambda: session.events("notify")[before:], 90, "a new-mail notification")[-1]
    check("Harbor tour tickets" in note["summary"] + note["body"] or "Henry" in note["summary"] + note["body"]
          or "new" in note["summary"].lower(), f"unexpected notification {note}")
    activate.Activate(10, 10)
    wait_for(lambda: main_window_showing(session), 10, "the window to come back")
    session.screenshot("arrived", session.main_window())
    return f"notification {note['summary']!r}: {note['body']!r}"


def deliver(maildir, name, subject, sender):
    message = (f"From: {sender}\r\nTo: {os.environ['E2E_USER']}\r\nSubject: {subject}\r\n"
               f"Date: {time.strftime('%a, %d %b %Y %H:%M:%S +0000', time.gmtime())}\r\n"
               f"Message-ID: <{name}@example.test>\r\n\r\nTwo seats are reserved for Saturday.\r\n")
    temporary = os.path.join(maildir, "tmp", name)
    with open(temporary, "w", encoding="utf-8") as out:
        out.write(message)
    os.chown(temporary, 2525, 2525)
    os.rename(temporary, os.path.join(maildir, "new", name))


def scenario_restart(session):
    """Quitting with the keyboard and starting again brings back the account and the last folder"""
    session.main_window()
    session.open_folder("Conversations")
    key("ctrl+q")
    try:
        status = session.process.wait(30)
    except subprocess.TimeoutExpired:
        raise Failure("the application did not quit within 30 s of Ctrl+Q")
    check(status == 0, f"the application exited with {status}")
    session.stop()
    session.start(reset=False)
    window = session.main_window()
    wait_for(lambda: session.folder_item("Conversations").getState().contains(pyatspi.STATE_SELECTED), 30,
             "the Conversations folder to be restored")
    restored = wait_for(lambda: rows(session.message_list()), 30, "the restored folder's messages")
    check(any("Status update" in row for row in restored), f"the list does not show Conversations: {restored[:3]}")
    session.screenshot("restored", window)
    return f"exit status 0; Conversations reopened with {len(restored)} messages"


SCENARIOS = [
    ("setup", scenario_setup),
    ("reading", scenario_reading),
    ("hostile", scenario_hostile),
    ("search", scenario_search),
    ("delete-undo", scenario_delete_undo),
    ("compose", scenario_compose),
    ("tray", scenario_tray),
    ("notifications", scenario_notifications),
    ("restart", scenario_restart),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("out")
    parser.add_argument("--only", default="")
    arguments = parser.parse_args()
    os.makedirs(arguments.out, exist_ok=True)
    no_cache()
    only = [name for name in arguments.only.split(",") if name]
    session = Session(arguments.out)
    results = []
    for name, run in SCENARIOS:
        if only and name not in only:
            continue
        session.scenario = name
        session.shots = []
        started = time.monotonic()
        # setup starts its own empty profile; the rest share a running app from the prepared profile
        try:
            if name == "setup":
                session.stop()
            elif not session.running():
                session.start()
            detail = run(session)
            status = "pass"
        except Exception as error:
            status = "fail"
            detail = str(error) if isinstance(error, Failure) else f"{type(error).__name__}: {error}"
            with open(os.path.join(arguments.out, f"{name}-error.txt"), "w", encoding="utf-8") as out:
                traceback.print_exc(file=out)
            try:
                if session.running():
                    session.screenshot("failure")
                    dump(session.app, os.path.join(arguments.out, f"{name}-tree.txt"))
            except Exception:
                pass
            # Reset: the next scenario starts from the prepared profile
            session.stop()
        if name == "setup":
            session.stop()
        seconds = time.monotonic() - started
        results.append((name, status, seconds, list(session.shots), detail))
        print(f"{status:4}  {name:14} {seconds:6.1f} s  {detail}", flush=True)
    session.stop()
    write_reports(arguments.out, results)
    return 0 if results and all(result[1] == "pass" for result in results) else 1


def write_reports(out, results):
    with open(os.path.join(out, "report.tsv"), "w", encoding="utf-8") as tsv:
        tsv.write("scenario\tresult\tseconds\tscreenshots\tdetail\n")
        for name, status, seconds, shots, detail in results:
            tsv.write(f"{name}\t{status}\t{seconds:.1f}\t{','.join(shots)}\t{detail}\n")
    with open(os.path.join(out, "report.md"), "w", encoding="utf-8") as md:
        md.write("| Scenario | Result | Seconds | Screenshots | Detail |\n| --- | --- | --- | --- | --- |\n")
        for name, status, seconds, shots, detail in results:
            links = ", ".join(f"[{shot}]({shot})" for shot in shots)
            md.write(f"| {name} | {status} | {seconds:.1f} | {links} | {detail.replace('|', '/')} |\n")


if __name__ == "__main__":
    sys.exit(main())
