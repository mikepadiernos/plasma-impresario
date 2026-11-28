/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Gui/NextcloudBrowserDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>

#include "Gui/NextcloudHub.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

using Nextcloud::Client;
using Nextcloud::Entry;

namespace {

enum Column { Name, Size, Modified, Access };
constexpr int EntryRole = Qt::UserRole + 1;

QIcon entryIcon(const Entry &entry)
{
    if (entry.folder)
        return QIcon::fromTheme(QStringLiteral("folder"));
    QMimeDatabase mimes;
    QMimeType type = mimes.mimeTypeForName(entry.contentType);
    if (!type.isValid() || type.isDefault())
        type = mimes.mimeTypeForFile(entry.name, QMimeDatabase::MatchExtension);
    return QIcon::fromTheme(type.iconName(), QIcon::fromTheme(type.genericIconName()));
}

QString accessText(const Entry &entry)
{
    QStringList parts;
    const Nextcloud::Permissions &permissions = entry.permissions;
    if (permissions.isKnown()) {
        const bool writable = entry.folder ? permissions.canCreateFile() : permissions.canWrite();
        if (!writable)
            parts << i18nc("@info what the user may do with a file", "Read only");
    }
    if (permissions.isShared())
        parts << i18nc("@info a file someone else shared", "Shared with you");
    if (entry.hasLinkShare())
        parts << i18nc("@info a file anyone with the link can open", "Public link");
    return parts.join(i18nc("@info separator of access notes", ", "));
}

QString accessToolTip(const Entry &entry)
{
    QStringList lines;
    if (entry.permissions.isShared())
        lines << i18nc("@info:tooltip",
            "Someone else shared this with you; they "
            "decide what you may do with it.");
    if (entry.hasLinkShare())
        lines << i18nc("@info:tooltip", "Anyone who has the public link of this can open it.");
    if (entry.permissions.isKnown()) {
        if (entry.folder && !entry.permissions.canCreateFile())
            lines << i18nc("@info:tooltip", "You may not save files in this folder.");
        else if (!entry.folder && !entry.permissions.canWrite())
            lines << i18nc("@info:tooltip", "You may not change this file.");
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

NextcloudBrowserDialog::NextcloudBrowserDialog(NextcloudHub *hub, Mode mode, QWidget *parent)
    : QDialog(parent)
    , m_hub(hub)
    , m_mode(mode)
    , m_pages(new QStackedWidget(this))
    , m_accounts(new QComboBox(this))
    , m_up(new QToolButton(this))
    , m_pathLabel(new QLabel(this))
    , m_refresh(new QToolButton(this))
    , m_openInBrowser(new QToolButton(this))
    , m_tree(new QTreeWidget(this))
    , m_status(new QLabel(this))
    , m_retry(new QPushButton(
          UiUtils::SymbolicIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action:button", "Try Again"), this))
    , m_buttons(new QDialogButtonBox(this))
{
    switch (mode) {
    case Mode::Browse:
        setWindowTitle(i18nc("@title:window", "Nextcloud Files"));
        break;
    case Mode::ChooseFiles:
        setWindowTitle(i18nc("@title:window", "Attach from Nextcloud"));
        break;
    case Mode::ChooseFolder:
        setWindowTitle(i18nc("@title:window", "Save to Nextcloud"));
        break;
    }

    m_accounts->setObjectName(QStringLiteral("nextcloudAccounts"));
    m_accounts->setAccessibleName(i18nc("@label:listbox accessible name", "Nextcloud account"));
    m_up->setObjectName(QStringLiteral("nextcloudUp"));
    m_up->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("go-up")));
    m_up->setToolTip(i18nc("@info:tooltip", "Go to the folder which contains this one"));
    m_up->setAutoRaise(true);
    m_pathLabel->setObjectName(QStringLiteral("nextcloudPath"));
    m_pathLabel->setTextFormat(Qt::PlainText);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_refresh->setObjectName(QStringLiteral("nextcloudRefresh"));
    m_refresh->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("view-refresh")));
    m_refresh->setToolTip(i18nc("@info:tooltip", "List the folder again"));
    m_refresh->setAutoRaise(true);
    m_openInBrowser->setObjectName(QStringLiteral("nextcloudOpenInBrowser"));
    m_openInBrowser->setIcon(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("internet-web-browser")));
    m_openInBrowser->setToolTip(i18nc("@info:tooltip", "Open this folder in Nextcloud in the browser"));
    m_openInBrowser->setAutoRaise(true);

    m_tree->setObjectName(QStringLiteral("nextcloudFiles"));
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setHeaderLabels({i18nc("@title:column", "Name"), i18nc("@title:column", "Size"),
        i18nc("@title:column", "Modified"), i18nc("@title:column", "Access")});
    m_tree->header()->setSectionResizeMode(Name, QHeaderView::Stretch);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setSelectionMode(
        mode == Mode::ChooseFiles ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    m_status->setObjectName(QStringLiteral("nextcloudStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_retry->setObjectName(QStringLiteral("nextcloudRetry"));
    m_retry->hide();

    auto *bar = new QHBoxLayout;
    bar->addWidget(m_accounts);
    bar->addWidget(m_up);
    bar->addWidget(m_pathLabel, 1);
    bar->addWidget(m_refresh);
    bar->addWidget(m_openInBrowser);

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_retry);

    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->addLayout(bar);
    filesLayout->addWidget(m_tree, 1);
    filesLayout->addLayout(statusRow);

    auto *emptyPage = new QWidget;
    emptyPage->setObjectName(QStringLiteral("nextcloudEmpty"));
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    auto *emptyText = new QLabel(i18nc("@info",
                                     "No Nextcloud account is connected. Connect one to attach, save and "
                                     "share files on your Nextcloud server."),
        emptyPage);
    emptyText->setWordWrap(true);
    emptyText->setAlignment(Qt::AlignCenter);
    auto *connectButton = new QPushButton(
        UiUtils::SymbolicIcon::fromTheme(QStringLiteral("list-add")), i18nc("@action:button", "Connect…"), emptyPage);
    connectButton->setObjectName(QStringLiteral("nextcloudConnectAccount"));
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyText);
    emptyLayout->addWidget(connectButton, 0, Qt::AlignCenter);
    emptyLayout->addStretch();

    m_pages->addWidget(filesPage);
    m_pages->addWidget(emptyPage);

    if (mode == Mode::Browse) {
        m_buttons->setStandardButtons(QDialogButtonBox::Close);
    } else {
        m_buttons->setStandardButtons(QDialogButtonBox::Cancel);
        m_choose = mode == Mode::ChooseFiles
            ? new QPushButton(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("mail-attachment")),
                  i18nc("@action:button", "&Attach"), this)
            : new QPushButton(UiUtils::SymbolicIcon::fromTheme(QStringLiteral("document-save")),
                  i18nc("@action:button", "&Save Here"), this);
        m_choose->setObjectName(QStringLiteral("nextcloudChoose"));
        m_choose->setDefault(true);
        m_buttons->addButton(m_choose, QDialogButtonBox::ActionRole);
        connect(m_choose, &QPushButton::clicked, this, &NextcloudBrowserDialog::choose);
    }
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_pages, 1);
    layout->addWidget(m_buttons);

    m_opener = [this](const QUrl &url) {
        auto *job = new KIO::OpenUrlJob(url, QStringLiteral("text/html"));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        job->start();
    };

    connect(m_accounts, &QComboBox::activated, this, [this](int index) {
        const QString id = m_accounts->itemData(index).toString();
        if (id != m_accountId)
            setAccount(id);
    });
    connect(m_up, &QToolButton::clicked, this, &NextcloudBrowserDialog::goUp);
    connect(m_refresh, &QToolButton::clicked, this, &NextcloudBrowserDialog::refresh);
    connect(m_retry, &QPushButton::clicked, this, &NextcloudBrowserDialog::refresh);
    connect(m_openInBrowser, &QToolButton::clicked, this, [this] {
        if (m_hub)
            m_opener(m_hub->account(m_accountId).webFolderUrl(m_path));
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) { activate(item); });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &NextcloudBrowserDialog::updateWidgets);
    connect(connectButton, &QPushButton::clicked, this, &NextcloudBrowserDialog::connectRequested);
    if (m_hub)
        connect(m_hub, &NextcloudHub::accountsChanged, this, &NextcloudBrowserDialog::reloadAccounts);

    resize(640, 420);
    reloadAccounts();
}

NextcloudBrowserDialog::~NextcloudBrowserDialog()
{
    if (m_client && m_request)
        m_client->abort(m_request);
}

NextcloudBrowserDialog::Mode NextcloudBrowserDialog::mode() const
{
    return m_mode;
}

void NextcloudBrowserDialog::reloadAccounts()
{
    const QList<Nextcloud::Account> accounts = m_hub ? m_hub->accounts() : QList<Nextcloud::Account>();
    m_accounts->clear();
    for (const Nextcloud::Account &account : accounts)
        m_accounts->addItem(
            UiUtils::SymbolicIcon::fromTheme(QStringLiteral("folder-cloud")), account.title(), account.id);
    m_accounts->setVisible(accounts.size() > 1);
    m_pages->setCurrentIndex(accounts.isEmpty() ? 1 : 0);
    if (accounts.isEmpty()) {
        m_accountId.clear();
        m_listing = {};
        m_listed = false;
        m_tree->clear();
        updateWidgets();
        return;
    }
    const int index = m_accounts->findData(m_accountId);
    if (index >= 0) {
        m_accounts->setCurrentIndex(index);
        updateWidgets();
    } else {
        setAccount(accounts.constFirst().id);
    }
}

void NextcloudBrowserDialog::setAccount(const QString &id)
{
    const int index = m_accounts->findData(id);
    if (index < 0)
        return;
    m_accounts->setCurrentIndex(index);
    m_accountId = id;
    navigate(QStringLiteral("/"));
}

QString NextcloudBrowserDialog::accountId() const
{
    return m_accountId;
}

void NextcloudBrowserDialog::setPath(const QString &path)
{
    navigate(Nextcloud::cleanPath(path));
}

QString NextcloudBrowserDialog::path() const
{
    return m_path;
}

QList<Entry> NextcloudBrowserDialog::entries() const
{
    return m_listing.children;
}

QString NextcloudBrowserDialog::statusText() const
{
    return m_status->text();
}

void NextcloudBrowserDialog::setUrlOpener(UrlOpener opener)
{
    m_opener = std::move(opener);
}

QPushButton *NextcloudBrowserDialog::addChooseButton(
    const QIcon &icon, const QString &text, std::function<void(const QString &accountId, const QList<Entry> &)> chosen)
{
    auto *button = new QPushButton(icon, text, this);
    m_buttons->addButton(button, QDialogButtonBox::ActionRole);
    m_extraChoices << button;
    connect(button, &QPushButton::clicked, this, [this, chosen = std::move(chosen)] {
        const QList<Entry> files = selectedFiles();
        if (files.isEmpty())
            return;
        chosen(m_accountId, files);
        accept();
    });
    updateWidgets();
    return button;
}

void NextcloudBrowserDialog::refresh()
{
    navigate(m_path);
}

void NextcloudBrowserDialog::goUp()
{
    if (m_path != QLatin1String("/"))
        navigate(Nextcloud::parentPath(m_path));
}

void NextcloudBrowserDialog::navigate(const QString &path)
{
    if (!m_hub || m_accountId.isEmpty())
        return;
    if (m_client && m_request)
        m_client->abort(m_request);
    m_request = 0;
    const quint64 generation = ++m_generation;
    const QString id = m_accountId;
    m_path = Nextcloud::cleanPath(path);
    m_listed = false;
    m_loading = true;
    m_tree->clear();
    m_retry->hide();
    m_status->setText(i18nc("@info", "Loading…"));
    updateWidgets();
    m_hub->withClient(
        id, this,
        [this, generation, id](Client *client) {
            if (generation != m_generation)
                return;
            m_client = client;
            m_request = client->list(
                m_path,
                [this, generation](const Nextcloud::Listing &listing) {
                    if (generation != m_generation)
                        return;
                    m_request = 0;
                    showListing(listing);
                },
                [this, generation, id, client](Client::Failure failure, const QString &message) {
                    if (generation != m_generation)
                        return;
                    m_request = 0;
                    if (failure == Client::Failure::Certificate && m_hub && m_hub->askTrust(client, this)) {
                        refresh();
                        return;
                    }
                    if (failure == Client::Failure::Authentication && m_hub) {
                        // The app password was revoked or changed; read it again next
                        // time
                        m_hub->forgetClient(id);
                        m_client = nullptr;
                        showFailure(i18nc("@info",
                            "The server refused the app password of this "
                            "account. If it was "
                            "revoked, connect the account again on the "
                            "Integrations page of "
                            "the settings."));
                        return;
                    }
                    if (failure == Client::Failure::NotFound && m_path != QLatin1String("/")) {
                        showFailure(i18nc("@info", "The folder %1 no longer exists.", m_path));
                        return;
                    }
                    showFailure(message);
                });
        },
        [this, generation](const QString &message) {
            if (generation != m_generation)
                return;
            showFailure(message);
        });
}

void NextcloudBrowserDialog::showListing(const Nextcloud::Listing &listing)
{
    m_loading = false;
    m_listed = true;
    m_listing = listing;
    m_tree->clear();
    const QLocale locale;
    for (int i = 0; i < listing.children.size(); ++i) {
        const Entry &entry = listing.children.at(i);
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(Name, entry.name);
        item->setIcon(Name, entryIcon(entry));
        item->setData(Name, EntryRole, i);
        if (!entry.folder) {
            item->setText(Size, locale.formattedDataSize(entry.size));
            item->setTextAlignment(Size, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (entry.modified.isValid())
            item->setText(Modified, locale.toString(entry.modified.toLocalTime(), QLocale::ShortFormat));
        item->setText(Access, accessText(entry));
        item->setToolTip(Access, accessToolTip(entry));
    }
    for (int column : {Size, Modified, Access})
        m_tree->resizeColumnToContents(column);
    m_retry->hide();
    if (listing.children.isEmpty())
        m_status->setText(i18nc("@info", "This folder is empty."));
    else
        m_status->clear();
    updateWidgets();
}

void NextcloudBrowserDialog::showFailure(const QString &message)
{
    m_loading = false;
    m_listed = false;
    m_tree->clear();
    m_status->setText(message);
    m_retry->show();
    updateWidgets();
}

void NextcloudBrowserDialog::updateWidgets()
{
    const bool hasAccount = !m_accountId.isEmpty();
    const Nextcloud::Account account = m_hub ? m_hub->account(m_accountId) : Nextcloud::Account();
    m_pathLabel->setText(
        hasAccount ? i18nc("@label account, then folder", "%1: %2", account.title(), m_path) : QString());
    m_up->setEnabled(hasAccount && m_path != QLatin1String("/"));
    m_refresh->setEnabled(hasAccount);
    m_openInBrowser->setEnabled(hasAccount);

    const bool hasFiles = !selectedFiles().isEmpty();
    for (QPushButton *button : std::as_const(m_extraChoices))
        button->setEnabled(m_listed && hasFiles);
    if (!m_choose)
        return;
    if (m_mode == Mode::ChooseFiles) {
        m_choose->setEnabled(m_listed && hasFiles);
        return;
    }
    const Nextcloud::Permissions &permissions = m_listing.folder.permissions;
    const bool writable = !permissions.isKnown() || permissions.canCreateFile();
    m_choose->setEnabled(m_listed && writable);
    m_choose->setToolTip(
        m_listed && !writable ? i18nc("@info:tooltip", "You may not save files in this folder.") : QString());
}

QList<Entry> NextcloudBrowserDialog::selectedFiles() const
{
    QList<Entry> files;
    const QList<QTreeWidgetItem *> items = m_tree->selectedItems();
    for (const QTreeWidgetItem *item : items) {
        const Entry &entry = m_listing.children.value(item->data(Name, EntryRole).toInt());
        if (!entry.folder && !entry.path.isEmpty())
            files << entry;
    }
    return files;
}

void NextcloudBrowserDialog::activate(QTreeWidgetItem *item)
{
    if (!item)
        return;
    const int index = item->data(Name, EntryRole).toInt();
    if (index < 0 || index >= m_listing.children.size())
        return;
    const Entry entry = m_listing.children.at(index);
    if (entry.folder) {
        navigate(entry.path);
        return;
    }
    if (m_mode == Mode::ChooseFiles)
        choose();
}

void NextcloudBrowserDialog::choose()
{
    if (!m_listed)
        return;
    if (m_mode == Mode::ChooseFiles) {
        const QList<Entry> files = selectedFiles();
        if (files.isEmpty())
            return;
        emit filesChosen(m_accountId, files);
        accept();
    } else if (m_mode == Mode::ChooseFolder) {
        const Nextcloud::Permissions &permissions = m_listing.folder.permissions;
        if (permissions.isKnown() && !permissions.canCreateFile())
            return;
        emit folderChosen(m_accountId, m_path);
        accept();
    }
}

} // namespace Gui
