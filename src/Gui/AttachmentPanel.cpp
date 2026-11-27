/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AttachmentPanel.h"

#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <memory>

#include "Common/PrivateFiles.h"
#include "Imap/Model/DragAndDrop.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Network/FileDownloadManager.h"
#include "MailSafetyPrompt.h"
#include "UiUtils/AttachmentCollector.h"
#include "UiUtils/Formatting.h"
#include "Util.h"

namespace Gui {

namespace {

constexpr int PartRole = Qt::UserRole;

/** A list which drags its parts in the format understood by the composer */
class AttachmentList : public QListWidget
{
public:
    using QListWidget::QListWidget;

protected:
    QStringList mimeTypes() const override
    {
        return {Imap::MimeTypes::xTrojitaImapPart};
    }

    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        if (items.isEmpty())
            return nullptr;
        const auto part = items.constFirst()->data(PartRole).value<QPersistentModelIndex>();
        return part.isValid() ? Imap::Mailbox::mimeDataForDragAndDrop(part) : nullptr;
    }
};

QIcon iconForMimeType(const QString &rawMime)
{
    const QIcon fallback = QIcon::fromTheme(QStringLiteral("mail-attachment"));
    if (rawMime == QLatin1String("message/rfc822"))
        return QIcon::fromTheme(QStringLiteral("message-rfc822"), QIcon::fromTheme(QStringLiteral("mail-message"), fallback));
    const QMimeType type = QMimeDatabase().mimeTypeForName(rawMime);
    if (!type.isValid() || type.isDefault())
        return fallback;
    return QIcon::fromTheme(type.iconName(), QIcon::fromTheme(type.genericIconName(), fallback));
}

QString describeMimeType(const QString &rawMime)
{
    const QMimeType type = QMimeDatabase().mimeTypeForName(rawMime);
    return type.isValid() && !type.isDefault() ? type.comment() : rawMime;
}

QString displayName(const QModelIndex &part)
{
    const QString fileName = part.data(Imap::Mailbox::RolePartFileName).toString();
    if (!fileName.isEmpty())
        return fileName;
    if (part.data(Imap::Mailbox::RolePartMimeType).toString().compare(QLatin1String("message/rfc822"), Qt::CaseInsensitive) == 0)
        return i18nc("@item an attached email without a file name", "Attached message");
    return Imap::Network::FileDownloadManager::toRealFileName(part);
}

}

EmbeddedPanel::Descriptor AttachmentPanel::panelDescriptor()
{
    Descriptor descriptor;
    descriptor.id = QStringLiteral("Attachments");
    descriptor.configKey = QStringLiteral("AttachmentPanel");
    descriptor.title = i18nc("@title embedded panel", "Attachments");
    descriptor.autoHideToolTip = i18nc("@info:tooltip", "Show the attachments panel only while the current message has attachments");
    descriptor.defaultEnabled = true;
    descriptor.defaultPlacement = Placement::BelowReader;
    return descriptor;
}

AttachmentPanel::AttachmentPanel(QWidget *parent)
    : EmbeddedPanel(panelDescriptor(), QStringLiteral("attachmentPanel"), parent)
    , m_list(new AttachmentList(this))
    , m_refreshTimer(new QTimer(this))
{
    m_open = addHeaderButton(QStringLiteral("Open"), QStringLiteral("document-open"), i18nc("@action:button", "Open Attachment"));
    m_save = addHeaderButton(QStringLiteral("Save"), QStringLiteral("document-save-as"),
                             i18nc("@action:button", "Save Attachment As…"));
    connect(m_open, &QToolButton::clicked, this, &AttachmentPanel::openCurrent);
    connect(m_save, &QToolButton::clicked, this, &AttachmentPanel::saveCurrent);

    m_list->setObjectName(QStringLiteral("attachmentPanelList"));
    m_list->setAccessibleName(i18nc("@label", "Attachments of the current message"));
    m_list->setIconSize(QSize(22, 22));
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setTextElideMode(Qt::ElideMiddle);
    connect(m_list, &QListWidget::itemActivated, this, &AttachmentPanel::openCurrent);
    connect(m_list, &QListWidget::currentItemChanged, this, &AttachmentPanel::updateActions);
    connect(m_list, &QWidget::customContextMenuRequested, this, &AttachmentPanel::showContextMenu);
    setContentWidget(m_list);

    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &AttachmentPanel::refresh);

    refresh();
}

void AttachmentPanel::setNetworkAccessManager(Imap::Network::MsgPartNetAccessManager *manager)
{
    m_netAccess = manager;
    updateActions();
}

int AttachmentPanel::attachmentCount() const
{
    return m_attachments.size();
}

QList<QPersistentModelIndex> AttachmentPanel::attachments() const
{
    return m_attachments;
}

QWidget *AttachmentPanel::listView() const
{
    return m_list;
}

bool AttachmentPanel::hasContent() const
{
    return !m_attachments.isEmpty();
}

QWidget *AttachmentPanel::focusTarget() const
{
    return m_list;
}

void AttachmentPanel::setMessageModel(QAbstractItemModel *model)
{
    for (const auto &connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_modelConnections.clear();
    m_model = model;
    if (model) {
        // Parts arrive lazily (BODYSTRUCTURE, decryption), so follow every structural change.
        m_modelConnections << connect(model, &QAbstractItemModel::rowsInserted, this, &AttachmentPanel::scheduleRefresh)
                           << connect(model, &QAbstractItemModel::rowsRemoved, this, &AttachmentPanel::scheduleRefresh)
                           << connect(model, &QAbstractItemModel::modelReset, this, &AttachmentPanel::scheduleRefresh)
                           << connect(model, &QAbstractItemModel::layoutChanged, this, &AttachmentPanel::scheduleRefresh)
                           << connect(model, &QAbstractItemModel::dataChanged, this, &AttachmentPanel::scheduleRefresh)
                           << connect(model, &QObject::destroyed, this, [this]() { setMessageModel(nullptr); });
    }
    refresh();
}

void AttachmentPanel::scheduleRefresh()
{
    m_refreshTimer->start();
}

void AttachmentPanel::refresh()
{
    m_refreshTimer->stop();
    const auto attachments = UiUtils::collectAttachments(m_model.data());
    const bool changed = attachments != m_attachments || m_list->count() != attachments.size();
    const bool countChanged = attachments.size() != m_attachments.size();
    m_attachments = attachments;

    if (changed) {
        const QPersistentModelIndex previous = currentPart();
        m_list->clear();
        for (const auto &part : std::as_const(m_attachments)) {
            const QString rawMime = part.data(Imap::Mailbox::RolePartMimeType).toString().toLower();
            const QString type = describeMimeType(rawMime);
            const QString size = UiUtils::Formatting::prettySize(part.data(Imap::Mailbox::RolePartOctets).toULongLong());
            auto *item = new QListWidgetItem(iconForMimeType(rawMime),
                                             i18nc("@item attachment file name and size", "%1 (%2)", displayName(part), size),
                                             m_list);
            item->setData(PartRole, QVariant::fromValue(part));
            item->setToolTip(rawMime == type ? type : i18nc("@info:tooltip MIME description and type", "%1 (%2)", type, rawMime));
            item->setData(Qt::AccessibleDescriptionRole, i18nc("@info attachment type and size", "%1, %2", type, size));
            if (previous.isValid() && part == previous)
                m_list->setCurrentItem(item);
        }
        if (!m_list->currentItem() && m_list->count())
            m_list->setCurrentRow(0);
    }

    if (!m_attachments.isEmpty()) {
        showContent();
    } else if (!m_model) {
        showPlaceholder(i18nc("@info", "No message selected"));
    } else {
        const QVariant fetched = m_model->index(0, 0).data(Imap::Mailbox::RoleIsFetched);
        showPlaceholder(fetched.isValid() && !fetched.toBool() ? i18nc("@info", "Loading message…")
                                                               : i18nc("@info", "This message has no attachments"));
    }
    setTitle(m_attachments.isEmpty()
                         ? i18nc("@title embedded panel", "Attachments")
                         : i18ncp("@title embedded panel", "%1 Attachment", "%1 Attachments", m_attachments.size()));
    updateActions();
    updateVisibility();
    if (countChanged)
        emit attachmentCountChanged(m_attachments.size());
}

void AttachmentPanel::updateActions()
{
    const bool usable = currentPart().isValid() && m_netAccess;
    m_open->setEnabled(usable);
    m_save->setEnabled(usable);
}

QPersistentModelIndex AttachmentPanel::currentPart() const
{
    const auto *item = m_list->currentItem();
    return item ? item->data(PartRole).value<QPersistentModelIndex>() : QPersistentModelIndex();
}

void AttachmentPanel::showContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    QMenu menu(this);
    menu.addAction(themedIcon(QStringLiteral("document-open")), i18nc("@action:inmenu", "Open"), this, &AttachmentPanel::openCurrent);
    menu.addAction(themedIcon(QStringLiteral("document-save-as")), i18nc("@action:inmenu", "Save As…"), this,
                   &AttachmentPanel::saveCurrent);
    menu.addSeparator();
    auto *findCopies = menu.addAction(themedIcon(QStringLiteral("system-search")),
                                      i18nc("@action:inmenu", "Find Saved Copies…"), this,
                                      &AttachmentPanel::findSavedCopiesOfCurrent);
    findCopies->setObjectName(QStringLiteral("findSavedCopies"));
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void AttachmentPanel::findSavedCopiesOfCurrent()
{
    const QPersistentModelIndex part = currentPart();
    if (part.isValid())
        emit findSavedCopiesRequested(Imap::Network::FileDownloadManager::toRealFileName(part));
}

void AttachmentPanel::openCurrent()
{
    const QPersistentModelIndex part = currentPart();
    if (!part.isValid() || !m_netAccess)
        return;
    if (!MailSafetyPrompt::confirmOpening(this, MailSafetyPrompt::junkVerdictOfPart(part), UiUtils::MailSafety::ScanResult::NotScanned,
                                          Imap::Network::FileDownloadManager::toRealFileName(part)))
        return;

    auto *manager = new Imap::Network::FileDownloadManager(this, m_netAccess, part);
    auto fileName = std::make_shared<QString>();
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, manager, [this, fileName](QString *requested) {
        // The copy outlives the download; the other application needs it, and it is removed after a day
        QString error;
        *requested = Common::PrivateFiles::createOpenedAttachment(Common::PrivateFiles::openedAttachmentsFolder(),
                                                                  *requested, &error);
        if (requested->isEmpty())
            emit transferError(i18nc("@info", "Cannot prepare a copy of the attachment to open: %1", error));
        *fileName = *requested;
    });
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, [manager, fileName]() {
        // Read-only, so that the launched application does not attempt to modify it
        QFile::setPermissions(*fileName, QFile::ReadOwner);
        QDesktopServices::openUrl(QUrl::fromLocalFile(*fileName));
        manager->deleteLater();
    });
    connect(manager, &Imap::Network::FileDownloadManager::transferError, this, [this, manager, fileName](const QString &error) {
        Common::PrivateFiles::removeOpenedAttachment(Common::PrivateFiles::openedAttachmentsFolder(), *fileName);
        emit transferError(error);
        manager->deleteLater();
    });
    connect(manager, &Imap::Network::FileDownloadManager::cancelled, manager, &QObject::deleteLater);
    manager->downloadPart();
}

void AttachmentPanel::saveCurrent()
{
    const QPersistentModelIndex part = currentPart();
    if (!part.isValid() || !m_netAccess)
        return;

    auto *manager = new Imap::Network::FileDownloadManager(this, m_netAccess, part);
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, this, [this](QString *requested) {
        *requested = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Save Attachment"),
                                                  QDir(Util::savedFilesFolder()).filePath(Common::PrivateFiles::safeFileName(*requested)), QString(), nullptr,
                                                  QFileDialog::HideNameFilterDetails);
        Util::rememberSavedFile(*requested);
    });
    connect(manager, &Imap::Network::FileDownloadManager::transferError, this, &AttachmentPanel::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::cancelled, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, this, [this, manager]() {
        emit fileSaved(manager->savedFileName());
    });
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

}
