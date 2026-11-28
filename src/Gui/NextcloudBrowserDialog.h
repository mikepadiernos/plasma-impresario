/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QDialog>
#include <QPointer>

#include "Nextcloud/Client.h"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Gui {

class NextcloudHub;

/**
 * @short Browse the files of a connected Nextcloud account, or choose files or
 * a folder there
 *
 * The dialog does not block the window. It lists one folder at a time; going
 * elsewhere stops the listing which is still running. Each entry shows what the
 * user may do with it and whether it is shared with them or through a public
 * link. Without a connected account, it offers to connect one.
 */
class NextcloudBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode {
        Browse,
        /** Choose one or more files, such as to attach them */
        ChooseFiles,
        /** Choose a folder in which files may be created, such as to save to */
        ChooseFolder,
    };
    using UrlOpener = std::function<void(const QUrl &)>;

    NextcloudBrowserDialog(NextcloudHub *hub, Mode mode, QWidget *parent = nullptr);
    ~NextcloudBrowserDialog() override;

    Mode mode() const;
    /** @short Show the account @arg id, at its root folder */
    void setAccount(const QString &id);
    QString accountId() const;
    void setPath(const QString &path);
    QString path() const;
    /** @short The entries of the folder shown, once listed */
    QList<Nextcloud::Entry> entries() const;
    QString statusText() const;
    /** @short How Open in Browser opens pages; in the default browser unless
     * replaced */
    void setUrlOpener(UrlOpener opener);
    /** @short Add a button which chooses the selected files in another way, such
     * as to insert links to them */
    QPushButton *addChooseButton(const QIcon &icon, const QString &text,
        std::function<void(const QString &accountId, const QList<Nextcloud::Entry> &)> chosen);

public slots:
    void refresh();
    void goUp();

signals:
    void filesChosen(const QString &accountId, const QList<Nextcloud::Entry> &files);
    void folderChosen(const QString &accountId, const QString &path);
    /** @short Connect… was clicked because no account is connected */
    void connectRequested();

private:
    void reloadAccounts();
    void navigate(const QString &path);
    void showListing(const Nextcloud::Listing &listing);
    void showFailure(const QString &message);
    void updateWidgets();
    void activate(QTreeWidgetItem *item);
    void choose();
    QList<Nextcloud::Entry> selectedFiles() const;

    QPointer<NextcloudHub> m_hub;
    Mode m_mode;
    QStackedWidget *m_pages;
    QComboBox *m_accounts;
    QToolButton *m_up;
    QLabel *m_pathLabel;
    QToolButton *m_refresh;
    QToolButton *m_openInBrowser;
    QTreeWidget *m_tree;
    QLabel *m_status;
    QPushButton *m_retry;
    QDialogButtonBox *m_buttons;
    QPushButton *m_choose = nullptr;
    QList<QPushButton *> m_extraChoices;

    QString m_accountId;
    QString m_path = QStringLiteral("/");
    Nextcloud::Listing m_listing;
    bool m_listed = false;
    bool m_loading = false;
    QPointer<Nextcloud::Client> m_client;
    Nextcloud::Client::RequestId m_request = 0;
    quint64 m_generation = 0;
    UrlOpener m_opener;
};

} // namespace Gui
