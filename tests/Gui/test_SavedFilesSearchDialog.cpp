/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Gui/SavedFilesSearchDialog.h"

#include <KLocalizedString>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using Gui::SavedFilesSearchDialog;
using UiUtils::FileSearch::Coverage;

namespace {

/** Write Baloo's settings where KConfig finds baloofilerc in test mode */
bool writeBalooConfig(const QByteArray &contents)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (!QDir().mkpath(dir))
        return false;
    QFile file(dir + QStringLiteral("/baloofilerc"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

QPushButton *button(SavedFilesSearchDialog &dialog, const char *name)
{
    return dialog.findChild<QPushButton *>(QString::fromLatin1(name));
}

}

class SavedFilesSearchDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("Saved/Private")));
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral(".hidden/Saved")));
        m_saved = QDir::cleanPath(m_dir.filePath(QStringLiteral("Saved")));
    }

    void cleanup()
    {
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/baloofilerc"));
    }

    void explainsTheCoverage()
    {
        QVERIFY(writeBalooConfig("[General]\nfolders=" + m_dir.path().toUtf8() + "\nexclude folders=" +
                                 m_saved.toUtf8() + "/Private\n"));
        SavedFilesSearchDialog dialog;
        dialog.setSearchWorkers(true, true);
        dialog.setSearchText(QStringLiteral("  report.pdf "));
        dialog.setFolder(m_saved);
        QCOMPARE(dialog.searchText(), QStringLiteral("report.pdf"));
        QCOMPARE(dialog.folder(), m_saved);
        QCOMPARE(dialog.status().coverage, Coverage::Covered);
        QCOMPARE(dialog.statusText(), i18nc("@info", "File Search is set to index this folder, which does not guarantee that every "
                                                     "file is indexed yet: files saved in the last few minutes may be missing. A live "
                                                     "search finds them right away."));
        auto *index = button(dialog, "savedFilesSearchIndex");
        auto *live = button(dialog, "savedFilesSearchLive");
        QVERIFY(!index->isHidden());
        QVERIFY(index->isEnabled());
        QVERIFY(index->isDefault());
        QVERIFY(live->isEnabled());
        QVERIFY(!live->isDefault());
        QCOMPARE(live->text(), i18nc("@action:button search through the files of a folder", "Live Search"));

        // An excluded folder can only be searched live
        dialog.setFolder(m_saved + QStringLiteral("/Private"));
        QCOMPARE(dialog.status().coverage, Coverage::Excluded);
        QVERIFY(dialog.statusText().contains(QDir::toNativeSeparators(m_saved + QStringLiteral("/Private"))));
        QVERIFY(!index->isEnabled());
        QVERIFY(live->isEnabled());
        QVERIFY(live->isDefault());

        dialog.setFolder(m_dir.filePath(QStringLiteral(".hidden/Saved")));
        QCOMPARE(dialog.status().coverage, Coverage::Hidden);
        QVERIFY(dialog.statusText().contains(QDir::toNativeSeparators(QDir::cleanPath(m_dir.filePath(QStringLiteral(".hidden"))))));
        QVERIFY(!index->isEnabled());

        // System Settings may change Baloo's settings while the dialog is open
        QVERIFY(writeBalooConfig("[Basic Settings]\nIndexing-Enabled=false\n"));
        dialog.setFolder(m_saved);
        QCOMPARE(dialog.status().coverage, Coverage::Disabled);
        QCOMPARE(dialog.statusText(), i18nc("@info", "File Search is turned off, so this folder is not indexed. A live search looks "
                                                     "through it instead, or turn on File Search in System Settings."));
        QVERIFY(!index->isEnabled());

        QVERIFY(writeBalooConfig("[General]\nfolders=" + m_saved.toUtf8() + "\nonly basic indexing=true\n"));
        dialog.setFolder(m_saved);
        QCOMPARE(dialog.status().coverage, Coverage::NamesOnly);
        QVERIFY(index->isEnabled());

        QVERIFY(writeBalooConfig("[General]\nfolders=/nowhere-near\n"));
        dialog.setFolder(m_saved);
        QCOMPARE(dialog.status().coverage, Coverage::NotIncluded);
        QCOMPARE(dialog.statusText(), i18nc("@info", "File Search is not set to index this folder. A live search looks through it "
                                                     "instead, or add the folder to File Search in System Settings."));
        QVERIFY(!index->isEnabled());

        // An indexed folder inside can be searched in the index, but only a live search covers the whole folder
        QVERIFY(writeBalooConfig("[General]\nfolders=" + m_saved.toUtf8() + "/Private\n"));
        dialog.setFolder(m_saved);
        QCOMPARE(dialog.status().coverage, Coverage::NotIncluded);
        QVERIFY(dialog.statusText().contains(QDir::toNativeSeparators(m_saved + QStringLiteral("/Private"))));
        QVERIFY(index->isEnabled());
        QVERIFY(!index->isDefault());
        QVERIFY(live->isDefault());

        // Without Baloo's KIO worker, the button is gone
        dialog.setSearchWorkers(false, true);
        QCOMPARE(dialog.status().coverage, Coverage::NotInstalled);
        QVERIFY(index->isHidden());
        QVERIFY(live->isDefault());
    }

    void needsTextAndAFolder()
    {
        QVERIFY(writeBalooConfig("[General]\nfolders=" + m_saved.toUtf8() + "\n"));
        SavedFilesSearchDialog dialog;
        dialog.setSearchWorkers(true, true);
        dialog.setFolder(m_saved);
        auto *index = button(dialog, "savedFilesSearchIndex");
        auto *live = button(dialog, "savedFilesSearchLive");
        QVERIFY(dialog.searchText().isEmpty());
        QVERIFY(!index->isEnabled());
        QVERIFY(!live->isEnabled());
        dialog.findChild<QLineEdit *>(QStringLiteral("savedFilesText"))->setText(QStringLiteral("invoice"));
        QVERIFY(index->isEnabled());
        QVERIFY(live->isEnabled());

        const QString missing = m_dir.filePath(QStringLiteral("Gone"));
        dialog.setFolder(missing);
        QCOMPARE(dialog.statusText(), i18nc("@info %1 is a folder", "The folder %1 does not exist.", QDir::toNativeSeparators(missing)));
        QVERIFY(!index->isEnabled());
        QVERIFY(!live->isEnabled());

        dialog.findChild<QLineEdit *>(QStringLiteral("savedFilesFolder"))->clear();
        QCOMPARE(dialog.statusText(), i18nc("@info", "Choose the folder in which you saved the files."));
        QVERIFY(!live->isEnabled());

        // Searching while the buttons are off does nothing
        bool opened = false;
        dialog.setUrlOpener([&opened](const QUrl &) { opened = true; });
        dialog.searchIndex();
        dialog.searchLive();
        QVERIFY(!opened);
    }

    void opensTheSearchInTheFileManager()
    {
        QVERIFY(writeBalooConfig("[General]\nfolders=" + m_saved.toUtf8() + "\n"));
        QList<QUrl> opened;
        {
            SavedFilesSearchDialog dialog;
            dialog.setSearchWorkers(true, true);
            dialog.setUrlOpener([&opened](const QUrl &url) { opened << url; });
            dialog.setSearchText(QStringLiteral("report.pdf"));
            dialog.setFolder(m_saved);
            dialog.show();
            QTest::mouseClick(button(dialog, "savedFilesSearchIndex"), Qt::LeftButton);
            QCOMPARE(opened.size(), 1);
            QCOMPARE(opened.last().scheme(), QStringLiteral("baloosearch"));
            const QUrlQuery query(opened.last());
            const QJsonObject json = QJsonDocument::fromJson(query.queryItemValue(QStringLiteral("json"), QUrl::FullyDecoded).toUtf8()).object();
            QCOMPARE(json.value(QStringLiteral("searchString")).toString(), QStringLiteral("report.pdf"));
            QCOMPARE(json.value(QStringLiteral("includeFolder")).toString(), m_saved);
            QCOMPARE(query.queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded),
                     i18nc("@title search results in the file manager, %1 is the searched text", "Saved files matching “%1”",
                           QStringLiteral("report.pdf")));
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QVERIFY(!dialog.isVisible());

            dialog.show();
            QTest::mouseClick(button(dialog, "savedFilesSearchLive"), Qt::LeftButton);
            QCOMPARE(opened.size(), 2);
            QCOMPARE(opened.last().scheme(), QStringLiteral("filenamesearch"));
            const QUrlQuery live(opened.last());
            QCOMPARE(live.queryItemValue(QStringLiteral("search"), QUrl::FullyDecoded), QStringLiteral("report.pdf"));
            QCOMPARE(QUrl(live.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded)), QUrl::fromLocalFile(m_saved));
        }

        // Without the live search worker, the folder itself opens, even without text
        SavedFilesSearchDialog dialog;
        dialog.setSearchWorkers(true, false);
        dialog.setUrlOpener([&opened](const QUrl &url) { opened << url; });
        dialog.setFolder(m_saved);
        auto *live = button(dialog, "savedFilesSearchLive");
        QCOMPARE(live->text(), i18nc("@action:button", "Open Folder"));
        QVERIFY(live->isEnabled());
        QVERIFY(dialog.statusText().endsWith(i18nc("@info", "Live search is not installed, so the folder opens in your "
                                                            "file manager instead.")));
        dialog.searchLive();
        QCOMPARE(opened.size(), 3);
        QCOMPARE(opened.last(), QUrl::fromLocalFile(m_saved));
    }

private:
    QTemporaryDir m_dir;
    QString m_saved;
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    SavedFilesSearchDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_SavedFilesSearchDialog.moc"
