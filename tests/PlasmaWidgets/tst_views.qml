/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick
import QtTest
import io.github.mikepadiernos.plasmaimpresario

/* The shared views of the Plasma widgets, driven by a fake client which records what is asked of it */
Item {
    id: top

    width: 400
    height: 300

    Component {
        id: fakeClient

        QtObject {
            property bool running: true
            property bool stale: false
            property bool loaded: true
            property string sharing: "subjects"
            property var accounts: []
            property int totalUnread: -1
            property var messages: []
            property var calls: []

            function openMessage(id) { calls.push("open " + id); }
            function compose() { calls.push("compose"); }
            function showAccount(id) { calls.push("account " + id); }
            function start() { calls.push("start"); }
            function refresh() { calls.push("refresh"); }
        }
    }

    Component {
        id: tickerComponent

        MailTicker {
            width: 300
            height: 30
            scrolling: false
        }
    }

    Component {
        id: summaryComponent

        StatusSummary {
            width: 300
        }
    }

    readonly property var work: ({ id: "work", name: "Work", unread: 2, state: "online" })
    readonly property var home: ({ id: "home", name: "Home", unread: 0, state: "offline" })
    readonly property var first: ({ id: "work/1", account: "work", accountName: "Work", folder: "INBOX",
                                    sender: "Ada", subject: "Plans", sent: "2026-11-27" })
    readonly property var second: ({ id: "work/2", account: "work", accountName: "Work", folder: "INBOX",
                                     sender: "Grace", subject: "", sent: "" })

    TestCase {
        name: "MailTicker"
        when: windowShown

        function ticker(properties) {
            const client = createTemporaryObject(fakeClient, top, properties || {});
            const view = createTemporaryObject(tickerComponent, top, { client: client });
            verify(view);
            return view;
        }

        function texts(view) {
            return view.entries.map(entry => entry.text);
        }

        function test_notices_data() {
            return [
                { tag: "not running", client: { running: false }, text: "Plasma Impresario is not running", call: "start" },
                { tag: "sharing nothing", client: { sharing: "nothing" }, text: "Plasma Impresario does not share its mail status", call: "start" },
                { tag: "loading", client: { loaded: false }, text: "Loading…", call: "start" },
                { tag: "no unread mail", client: { accounts: [top.home] }, text: "No unread mail", call: "start" },
            ];
        }

        function test_notices(data) {
            const view = ticker(data.client);
            compare(texts(view), [data.text]);
            view.open(0);
            compare(view.client.calls, [data.call]);
        }

        function test_withoutClient() {
            const view = createTemporaryObject(tickerComponent, top);
            compare(view.entries.length, 0);
            view.open(0);
        }

        function test_messages() {
            const view = ticker({ messages: [top.first, top.second], accounts: [top.work] });
            compare(texts(view), ["Ada · Plans", "Grace · (No subject)"]);
            view.open(1);
            compare(view.client.calls, ["open work/2"]);
        }

        function test_entryText() {
            const view = ticker();
            view.showAccount = true;
            view.showFolder = true;
            compare(view.entryText(top.first), "Work · Ada · Plans · INBOX");
            view.showSender = false;
            view.showSubject = false;
            view.showAccount = false;
            view.showFolder = false;
            compare(view.entryText(top.first), "Unread message");
            view.showDate = true;
            compare(view.entryText(top.first), Qt.formatDate(new Date(2026, 10, 27), Qt.locale(), Locale.ShortFormat));
            compare(view.entryText(top.second), "Unread message");
            // A subject the application does not share is left out, an empty one is kept
            view.showDate = false;
            view.showSender = true;
            view.showSubject = true;
            const withoutSubject = Object.assign({}, top.first);
            delete withoutSubject.subject;
            compare(view.entryText(withoutSubject), "Ada");
        }

        function test_counts() {
            const view = ticker({ sharing: "counts", accounts: [top.work, top.home] });
            compare(texts(view), ["Work: 2 unread"]);
            view.open(0);
            compare(view.client.calls, ["account work"]);
        }

        function test_keys() {
            const view = ticker({ messages: [top.first, top.second] });
            view.forceActiveFocus();
            verify(view.activeFocus);
            verify(!view.moving);
            compare(view.currentIndex, 0);
            keyClick(Qt.Key_Right);
            compare(view.currentIndex, 1);
            compare(findChild(view, "page").text, "Grace · (No subject)");
            keyClick(Qt.Key_Right);
            compare(view.currentIndex, 0);
            keyClick(Qt.Key_Left);
            compare(view.currentIndex, 1);
            keyClick(Qt.Key_Return);
            keyClick(Qt.Key_Space);
            compare(view.client.calls, ["open work/2", "open work/2"]);
            compare(view.Accessible.name, "Grace · (No subject)");
        }

        function test_paging() {
            const view = ticker({ messages: [top.first, top.second] });
            view.pageInterval = 1000;
            verify(findChild(view, "pager").running);
            tryCompare(view, "currentIndex", 1, 3000);
            tryCompare(view, "currentIndex", 0, 3000);
        }

        function test_fewerEntries() {
            const view = ticker({ messages: [top.first, top.second] });
            view.currentIndex = 1;
            view.client.messages = [top.first];
            compare(view.currentIndex, 0);
            verify(!findChild(view, "pager").running);
        }

        function test_scrolling() {
            const view = ticker({ messages: [top.first] });
            view.scrolling = true;
            // Scrolls unless animations are off in the environment, and pages while focused
            if (view.moving) {
                verify(findChild(view, "strip").visible);
                view.forceActiveFocus();
                verify(!view.moving);
                verify(findChild(view, "page").visible);
            }
        }
    }

    TestCase {
        name: "StatusSummary"
        when: windowShown

        function summary(properties) {
            const client = createTemporaryObject(fakeClient, top, properties || {});
            const view = createTemporaryObject(summaryComponent, top, { client: client });
            verify(view);
            return view;
        }

        function test_heading_data() {
            return [
                { tag: "unknown", total: -1, text: "Mail" },
                { tag: "none", total: 0, text: "No unread mail" },
                { tag: "one", total: 1, text: "1 unread message" },
                { tag: "several", total: 3, text: "3 unread messages" },
            ];
        }

        function test_heading(data) {
            const view = summary({ totalUnread: data.total });
            compare(findChild(view, "heading").text, data.text);
        }

        function test_notice_data() {
            return [
                { tag: "current", client: { accounts: [top.work] }, text: "" },
                { tag: "not running", client: { running: false, loaded: false }, text: "Plasma Impresario is not running." },
                { tag: "not running, old counts", client: { running: false, accounts: [top.work] },
                  text: "Plasma Impresario is not running. The counts below may be out of date." },
                { tag: "sharing nothing", client: { sharing: "nothing" },
                  text: "Plasma Impresario does not share its mail status. Choose what to share under Settings, General." },
                { tag: "not answering", client: { stale: true, accounts: [top.work] },
                  text: "Plasma Impresario is not answering. The counts below may be out of date." },
                { tag: "no accounts", client: {}, text: "No accounts to show." },
                { tag: "loading", client: { loaded: false }, text: "" },
            ];
        }

        function test_notice(data) {
            const view = summary(data.client);
            compare(view.notice, data.text);
            compare(findChild(view, "notice").visible, data.text.length > 0);
        }

        function test_states() {
            const view = summary();
            compare(view.stateText("online"), "Online");
            compare(view.stateText("signInNeeded"), "Sign-in needed");
            compare(view.stateText("something else"), "Not connected yet");
            compare(view.unreadText(-1), "Unknown");
            compare(view.unreadText(1), "1 unread");
            compare(view.unreadText(4), "4 unread");
        }

        function test_loading() {
            const view = summary({ loaded: false });
            verify(findChild(view, "loading").visible);
            view.client.loaded = true;
            verify(!findChild(view, "loading").visible);
        }

        function test_buttons() {
            const view = summary({ accounts: [top.work] });
            const compose = findChild(view, "compose");
            verify(compose.enabled);
            mouseClick(compose);
            mouseClick(findChild(view, "open"));
            compare(view.client.calls, ["compose", "start"]);
            view.client.running = false;
            verify(!compose.enabled);
        }
    }
}
