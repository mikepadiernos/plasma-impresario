/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_Imap_Workloads.h"

#include <QFile>
#include <QMap>
#include <QSignalSpy>

#include "Imap/Model/HeaderBackfill.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Streams/FakeSocket.h"
#include "UiUtils/AttachmentCollector.h"
#include "Utils/FakeCapabilitiesInjector.h"

using namespace Imap::Mailbox;

namespace {

int countDescendants(const QAbstractItemModel *model, const QModelIndex &parent)
{
    int count = 0;
    for (int row = 0; row < model->rowCount(parent); ++row)
        count += 1 + countDescendants(model, model->index(row, 0, parent));
    return count;
}

/** One conversation of @p size messages: a reply tree for most of it and a
 * chain of @p chain replies to replies at the end */
QByteArray conversation(int size, int chain)
{
    QMap<int, QList<int>> children;
    for (int uid = 2; uid <= size; ++uid)
        children[uid <= size - chain ? uid / 2 : uid - 1].append(uid);
    std::function<QByteArray(int)> format = [&](int uid) {
        QByteArray out = QByteArray::number(uid);
        const QList<int> kids = children.value(uid);
        if (kids.size() == 1) {
            out += ' ' + format(kids.first());
        } else if (kids.size() > 1) {
            out += ' ';
            for (const int kid : kids)
                out += '(' + format(kid) + ')';
        }
        return out;
    };
    return "* THREAD (" + format(1) + ")\r\n";
}

QByteArray attachmentStructure(int attachments)
{
    QByteArray out = "(\"text\" \"plain\" (\"charset\" \"utf-8\") NIL NIL "
                     "\"7bit\" 120 4 NIL NIL NIL NIL)";
    for (int i = 1; i <= attachments; ++i) {
        const QByteArray name = "file-" + QByteArray::number(i) + ".pdf";
        out += "(\"application\" \"pdf\" (\"name\" \"" + name
            + "\") NIL NIL \"base64\" 40000 NIL (\"attachment\" (\"filename\" \"" + name + "\")) NIL NIL)";
    }
    return out + " \"mixed\" (\"boundary\" \"sep\") NIL NIL NIL";
}

/** @short Answer the searches and header requests of the background loading as a server would

The newest @p recent messages count as recent; every UID is its own sequence number, as in the fixture.
*/
void serveBackfill(Streams::FakeSocket *socket, uint newest, uint recent)
{
    const QByteArray written = socket->writtenStuff();
    if (written.isEmpty())
        return;
    QByteArray answer;
    for (const QByteArray &line : written.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const int space = line.indexOf(' ');
        const QByteArray tag = line.left(space);
        const QByteArray command = line.mid(space + 1).trimmed();
        if (command.startsWith("UID SEARCH")) {
            answer += "* SEARCH";
            for (uint uid = newest - recent + 1; uid <= newest; ++uid)
                answer += ' ' + QByteArray::number(uid);
            answer += "\r\n";
        } else if (command.startsWith("UID FETCH ")) {
            const QByteArray set = command.mid(10, command.indexOf(' ', 10) - 10);
            for (const QByteArray &range : set.split(',')) {
                const int colon = range.indexOf(':');
                const uint first = range.left(colon < 0 ? range.size() : colon).toUInt();
                const uint last = colon < 0 ? first : range.mid(colon + 1).toUInt();
                for (uint uid = first; uid <= last; ++uid) {
                    const QByteArray number = QByteArray::number(uid);
                    answer += "* " + number + " FETCH (UID " + number
                        + " RFC822.SIZE 89 INTERNALDATE \"20-Nov-2026 10:00:00 +0000\" ENVELOPE (NIL \"Message "
                        + number
                        + "\" NIL NIL NIL NIL NIL NIL NIL NIL) BODYSTRUCTURE (\"text\" \"plain\" () NIL NIL NIL 19 2 "
                          "NIL "
                          "NIL NIL NIL))\r\n";
                }
            }
        }
        answer += tag + " OK done\r\n";
    }
    socket->fakeReading(answer);
}

} // namespace

void ImapWorkloadsTest::init()
{
    LibMailboxSync::init();
}

ImapWorkloadsTest::Sample ImapWorkloadsTest::sample()
{
    Sample s;
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly))
        return s;
    const QList<QByteArray> lines = status.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const auto value
            = [&line]() { return line.mid(line.indexOf(':') + 1).trimmed().split(' ').first().toLongLong(); };
        if (line.startsWith("VmRSS:"))
            s.rssKib = value();
        else if (line.startsWith("VmHWM:"))
            s.peakKib = value();
    }
    return s;
}

int ImapWorkloadsTest::scaled(int size)
{
    bool ok = false;
    const double scale = qEnvironmentVariable("IMPRESARIO_WORKLOAD_SCALE").toDouble(&ok);
    if (!ok || scale <= 0)
        return size;
    // Keep multiples of ten, which the fixture's flag pattern relies on
    return std::max(10, int(size * scale) / 10 * 10);
}

void ImapWorkloadsTest::begin()
{
    // Reset the peak so that it belongs to this workload; not every kernel allows
    // it
    QFile clear(QStringLiteral("/proc/self/clear_refs"));
    if (clear.open(QIODevice::WriteOnly))
        clear.write("5");
    m_before = sample();
    m_longestPassMs = -1;
    m_timer.start();
}

bool ImapWorkloadsTest::pump(const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer total;
    total.start();
    QElapsedTimer pass;
    while (!done()) {
        if (total.elapsed() > timeoutMs)
            return false;
        pass.start();
        QCoreApplication::processEvents();
        m_longestPassMs = std::max(m_longestPassMs, pass.elapsed());
    }
    return true;
}

void ImapWorkloadsTest::blocking(const std::function<void()> &work)
{
    QElapsedTimer pass;
    pass.start();
    work();
    m_longestPassMs = std::max(m_longestPassMs, pass.elapsed());
}

void ImapWorkloadsTest::finish(const char *name, int items, qint64 budgetMs, qint64 passBudgetMs)
{
    const qint64 elapsed = m_timer.elapsed();
    const Sample after = sample();
    const qint64 rssDelta = after.rssKib >= 0 && m_before.rssKib >= 0 ? after.rssKib - m_before.rssKib : -1;
    const QString line = QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\t%7")
                             .arg(QString::fromLatin1(name))
                             .arg(items)
                             .arg(elapsed)
                             .arg(m_longestPassMs)
                             .arg(rssDelta)
                             .arg(after.peakKib)
                             .arg(budgetMs);
    qInfo().noquote() << "WORKLOAD" << name << "items=" << items << "time_ms=" << elapsed
                      << "longest_pass_ms=" << m_longestPassMs << "rss_delta_kib=" << rssDelta
                      << "peak_kib=" << after.peakKib << "budget_ms=" << budgetMs;
    const QString report = qEnvironmentVariable("IMPRESARIO_WORKLOAD_REPORT");
    if (!report.isEmpty()) {
        QFile file(report);
        if (file.open(QIODevice::Append | QIODevice::Text))
            file.write(line.toUtf8() + '\n');
    }
    if (qEnvironmentVariableIntValue("IMPRESARIO_WORKLOAD_BUDGETS") == 1) {
        QVERIFY2(elapsed <= budgetMs,
            qPrintable(QStringLiteral("%1 took %2 ms, over its budget of %3 ms")
                    .arg(QString::fromLatin1(name))
                    .arg(elapsed)
                    .arg(budgetMs)));
        QVERIFY2(m_longestPassMs <= passBudgetMs,
            qPrintable(QStringLiteral("%1 kept the event loop busy for %2 ms, "
                                      "over its budget of %3 ms")
                    .arg(QString::fromLatin1(name))
                    .arg(m_longestPassMs)
                    .arg(passBudgetMs)));
    }
}

/** @short The first sync of a large folder: UIDs and flags of every message */
void ImapWorkloadsTest::testLargeMailboxSync()
{
    const int size = scaled(50000);
    begin();
    initialMessages(size);
    QCOMPARE(model->rowCount(msgListA), size);
    finish("sync-large-folder", size, 3000);
    QCOMPARE(idxA.data(RoleUnreadMessageCount).toInt(), size / 10);
    cEmpty();
}

/** @short The Unread quick filter over a large folder, on and off again */
void ImapWorkloadsTest::testFilterLargeList()
{
    const int size = scaled(50000);
    initialMessages(size);
    cEmpty();
    PrettyMsgListModel pretty(this);
    pretty.setSourceModel(threadingModel);
    QCOMPARE(pretty.rowCount(), size);

    begin();
    blocking([&pretty] { pretty.setFilters(PrettyMsgListModel::FilterUnread); });
    QCOMPARE(pretty.rowCount(), size / 10);
    blocking([&pretty] { pretty.setFilters(PrettyMsgListModel::NoFilter); });
    QCOMPARE(pretty.rowCount(), size);
    finish("filter-large-folder", size, 1500);
}

/** @short One conversation of thousands of messages, including a long chain of
 * replies, threaded and walked as an expanded list would be */
void ImapWorkloadsTest::testLongThread()
{
    const int size = scaled(2000);
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("THREAD=REFS"));
    threadingModel->setUserWantsThreading(true);
    initialMessages(size);
    const QByteArray thread = conversation(size, std::min(200, size / 2));
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));

    begin();
    SOCK->fakeReading(thread + t.last("OK thread\r\n"));
    QVERIFY(pump([this] { return threadingModel->rowCount() == 1; }));
    QCOMPARE(countDescendants(threadingModel, QModelIndex()), size);
    finish("long-conversation", size, 500);
    cEmpty();
}

/** @short Marking ten thousand messages as read, and the server's answer for
 * each */
void ImapWorkloadsTest::testBulkFlagChange()
{
    const int size = scaled(50000);
    const int bulk = size / 5;
    initialMessages(size);
    cEmpty();
    QModelIndexList messages;
    messages.reserve(bulk);
    for (int row = 0; row < bulk; ++row)
        messages << model->index(row, 0, msgListA);

    begin();
    model->markMessagesRead(messages, FLAG_ADD);
    cClient(t.mk(QStringLiteral("UID STORE 1:%1 +FLAGS (\\Seen)\r\n").arg(bulk).toUtf8()));
    QByteArray answer;
    answer.reserve(bulk * 32);
    for (int seq = 1; seq <= bulk; ++seq)
        answer += "* " + QByteArray::number(seq) + " FETCH (FLAGS (\\Seen))\r\n";
    SOCK->fakeReading(answer + t.last("OK stored\r\n"));
    const int expected = size / 10 - bulk / 10;
    QVERIFY(pump([this, expected] { return idxA.data(RoleUnreadMessageCount).toInt() == expected; }));
    finish("bulk-mark-read", bulk, 1000);
    cEmpty();
}

/** @short Another client removes thousands of messages, one EXPUNGE at a time,
 * as servers without QRESYNC report it */
void ImapWorkloadsTest::testBulkExpunge()
{
    const int size = scaled(50000);
    const int bulk = size / 5;
    initialMessages(size);
    cEmpty();

    QByteArray expunges;
    expunges.reserve(bulk * 14);
    for (int i = 0; i < bulk; ++i)
        expunges += "* 1 EXPUNGE\r\n";
    begin();
    SOCK->fakeReading(expunges);
    QVERIFY(pump([this, size, bulk] { return model->rowCount(msgListA) == size - bulk; }));
    finish("bulk-expunge", bulk, 8000);
    QCOMPARE(idxA.data(RoleUnreadMessageCount).toInt(), size / 10 - bulk / 10);
    QCOMPARE(model->index(0, 0, msgListA).data(RoleMessageUid).toInt(), bulk + 1);
    cEmpty();
}

/** @short The same removal reported as one VANISHED range, as servers with
 * QRESYNC do */
void ImapWorkloadsTest::testBulkVanished()
{
    const int size = scaled(50000);
    const int bulk = size / 5;
    initialMessages(size);
    cEmpty();
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("QRESYNC"));

    begin();
    SOCK->fakeReading("* VANISHED 1:" + QByteArray::number(bulk) + "\r\n");
    QVERIFY(pump([this, size, bulk] { return model->rowCount(msgListA) == size - bulk; }));
    // One VANISHED response is handled in a single pass that removes and
    // renumbers rows one at a time, so it blocks the event loop for its whole
    // duration; see the performance caveat (#506).
    finish("bulk-vanished", bulk, 3000, 1500);
    QCOMPARE(idxA.data(RoleUnreadMessageCount).toInt(), size / 10 - bulk / 10);
    QCOMPARE(model->index(0, 0, msgListA).data(RoleMessageUid).toInt(), bulk + 1);
    cEmpty();
}

/** @short A message with two hundred attachments: its structure and the
 * attachment list */
void ImapWorkloadsTest::testManyAttachments()
{
    const int attachments = scaled(200);
    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    const QModelIndex message = msgListB.model()->index(0, 0, msgListB);
    QCOMPARE(model->rowCount(message), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));

    begin();
    SOCK->fakeReading(
        "* 1 FETCH (UID 333 BODYSTRUCTURE (" + attachmentStructure(attachments) + "))\r\n" + t.last("OK fetched\r\n"));
    QVERIFY(pump([this, &message] { return model->rowCount(message) == 1; }));
    const QModelIndex root = model->index(0, 0, message);
    QCOMPARE(model->rowCount(root), attachments + 1);
    const QList<QPersistentModelIndex> found = UiUtils::collectAttachments(root);
    finish("many-attachments", attachments, 500);
    QCOMPARE(found.size(), attachments);
    QCOMPARE(found.last().data(RolePartFileName).toString(), QStringLiteral("file-%1.pdf").arg(attachments));
    cEmpty();
}

/** @short Opening a 20 MiB attachment: download, decoding and the memory it
 * takes */
void ImapWorkloadsTest::testLargeAttachment()
{
    const int mib = std::max(1, scaled(20 * 10) / 10);
    const int size = mib * 1024 * 1024;
    model->setProperty("impresario-imap-delayed-fetch-part", 0);
    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    const QModelIndex message = msgListB.model()->index(0, 0, msgListB);
    QCOMPARE(model->rowCount(message), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE ((\"text\" \"plain\" (\"charset\" "
            "\"utf-8\") NIL NIL \"7bit\" 12 1 NIL "
            "NIL NIL "
            "NIL)(\"application\" \"octet-stream\" (\"name\" \"big.bin\") NIL "
            "NIL \"base64\" "
        + QByteArray::number(size * 4 / 3)
        + " NIL (\"attachment\" (\"filename\" \"big.bin\")) NIL NIL) \"mixed\" "
          "(\"boundary\" \"sep\") NIL NIL NIL))\r\n"
        + t.last("OK fetched\r\n"));
    const QModelIndex part = model->index(1, 0, model->index(0, 0, message));
    QCOMPARE(part.data(RolePartFileName).toString(), QStringLiteral("big.bin"));

    QByteArray content(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i)
        content[i] = char((unsigned(i) * 131u) & 0xffu);
    // The data arrives as the server sends it; it is prepared first so that the
    // memory measured is the application's own
    QByteArray wire = "* 1 FETCH (UID 333 BODY[2] " + asLiteral(content.toBase64()) + ")\r\n";

    begin();
    QCOMPARE(part.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[2])\r\n"));
    SOCK->fakeReading(wire + t.last("OK fetched\r\n"));
    wire.clear();
    wire.squeeze();
    QVERIFY(pump([&part] { return part.data(RoleIsFetched).toBool(); }));
    const QByteArray decoded = part.data(RolePartData).toByteArray();
    finish("large-attachment", mib, 2000);
    QCOMPARE(decoded.size(), size);
    QVERIFY(decoded == content);
    cEmpty();
}

/** @short The headers of a large folder loading in the background: how soon the recent ones are there, and how long
 * the rest up to the limit takes, while the event loop stays free */
void ImapWorkloadsTest::testHeaderBackfill()
{
    const int size = scaled(50000);
    const int recent = std::min(500, size / 10);
    HeaderBackfill backfill(model);
    // The fixture expects a quiet connection right after the synchronization
    bool held = true;
    backfill.setOtherWorkPending([&held]() { return held; });
    initialMessages(size);
    cEmpty();
    QSignalSpy recentLoaded(&backfill, &HeaderBackfill::recentLoaded);
    QSignalSpy finished(&backfill, &HeaderBackfill::finished);
    auto *socket = SOCK;

    begin();
    held = false;
    QVERIFY(pump([&]() {
        serveBackfill(socket, size, recent);
        return !recentLoaded.isEmpty();
    }));
    finish("backfill-recent-headers", recent, 3000);
    QCOMPARE(backfill.recentCount(), recent);
    QVERIFY(model->index(size - 1, 0, msgListA).data(RoleIsFetched).toBool());

    begin();
    QVERIFY(pump([&]() {
        serveBackfill(socket, size, recent);
        return !finished.isEmpty();
    }));
    finish("backfill-older-headers", backfill.requestedCount() - recent, 60000);
    QCOMPARE(backfill.requestedCount(), std::min(size, backfill.messageLimit()));
    QVERIFY(pump([&]() {
        serveBackfill(socket, size, recent);
        return model->index(size - backfill.requestedCount(), 0, msgListA).data(RoleIsFetched).toBool();
    }));
    cEmpty();
}

QTEST_GUILESS_MAIN(ImapWorkloadsTest)
