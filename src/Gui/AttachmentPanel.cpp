/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "AttachmentPanel.h"

#include <KLocalizedString>
#include <QAbstractItemModel>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QStackedWidget>
#include <QTemporaryFile>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>

#include "Common/Paths.h"
#include "Imap/Model/DragAndDrop.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Network/FileDownloadManager.h"
#include "UiUtils/AttachmentCollector.h"
#include "UiUtils/Formatting.h"

namespace Gui {

namespace {

constexpr int PartRole = Qt::UserRole;
constexpr int MaximumRowsInSizeHint = 3;

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

QIcon themedIcon(const QString &name)
{
    return QIcon::fromTheme(name + QLatin1String("-symbolic"), QIcon::fromTheme(name));
}

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

AttachmentPanel::AttachmentPanel(QWidget *parent)
    : QFrame(parent)
    , m_title(new QLabel(this))
    , m_stack(new QStackedWidget(this))
    , m_placeholder(new QLabel(m_stack))
    , m_list(new AttachmentList(m_stack))
    , m_open(new QToolButton(this))
    , m_save(new QToolButton(this))
    , m_refreshTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("attachmentPanel"));
    setAccessibleName(i18nc("@title embedded panel", "Attachments"));
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

    m_title->setObjectName(QStringLiteral("attachmentPanelTitle"));
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);

    const auto setUpButton = [](QToolButton *button, const QString &objectName, const QString &icon, const QString &text) {
        button->setObjectName(objectName);
        button->setIcon(themedIcon(icon));
        button->setText(text);
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setAutoRaise(true);
        button->setIconSize(QSize(16, 16));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    };
    setUpButton(m_open, QStringLiteral("attachmentPanelOpen"), QStringLiteral("document-open"),
                i18nc("@action:button", "Open Attachment"));
    setUpButton(m_save, QStringLiteral("attachmentPanelSave"), QStringLiteral("document-save-as"),
                i18nc("@action:button", "Save Attachment As…"));
    connect(m_open, &QToolButton::clicked, this, &AttachmentPanel::openCurrent);
    connect(m_save, &QToolButton::clicked, this, &AttachmentPanel::saveCurrent);

    m_placeholder->setObjectName(QStringLiteral("attachmentPanelPlaceholder"));
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setEnabled(false);
    m_placeholder->setWordWrap(true);

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

    m_stack->addWidget(m_placeholder);
    m_stack->addWidget(m_list);
    m_stack->setMinimumHeight(fontMetrics().height() * 2);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(m_title);
    header->addStretch();
    header->addWidget(m_open);
    header->addWidget(m_save);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(m_stack, 1);
    m_title->setBuddy(m_list);

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

bool AttachmentPanel::isPanelEnabled() const
{
    return m_enabled;
}

bool AttachmentPanel::autoHide() const
{
    return m_autoHide;
}

bool AttachmentPanel::shouldBeShown() const
{
    return m_enabled && (!m_autoHide || !m_attachments.isEmpty());
}

int AttachmentPanel::attachmentCount() const
{
    return m_attachments.size();
}

QList<QPersistentModelIndex> AttachmentPanel::attachments() const
{
    return m_attachments;
}

QSize AttachmentPanel::sizeHint() const
{
    QSize hint = QFrame::sizeHint();
    const int rows = qBound(1, m_list->count(), MaximumRowsInSizeHint);
    const int rowHeight = m_list->count() ? m_list->sizeHintForRow(0) : fontMetrics().height() * 2;
    const QMargins margins = contentsMargins() + layout()->contentsMargins();
    hint.setHeight(margins.top() + margins.bottom() + qMax(m_title->sizeHint().height(), m_open->sizeHint().height())
                   + layout()->spacing() + rows * rowHeight + 2 * m_list->frameWidth());
    return hint;
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

void AttachmentPanel::setPanelEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    updateVisibility();
}

void AttachmentPanel::setAutoHide(bool autoHide)
{
    if (m_autoHide == autoHide)
        return;
    m_autoHide = autoHide;
    updateVisibility();
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

    if (!m_model) {
        m_placeholder->setText(i18nc("@info", "No message selected"));
    } else {
        const QVariant fetched = m_model->index(0, 0).data(Imap::Mailbox::RoleIsFetched);
        m_placeholder->setText(fetched.isValid() && !fetched.toBool() ? i18nc("@info", "Loading message…")
                                                                       : i18nc("@info", "This message has no attachments"));
    }
    m_stack->setCurrentWidget(m_attachments.isEmpty() ? static_cast<QWidget *>(m_placeholder) : m_list);
    m_title->setText(m_attachments.isEmpty()
                         ? i18nc("@title embedded panel", "Attachments")
                         : i18ncp("@title embedded panel", "%1 Attachment", "%1 Attachments", m_attachments.size()));
    updateActions();
    if (changed)
        updateGeometry();
    updateVisibility();
}

void AttachmentPanel::updateActions()
{
    const bool usable = currentPart().isValid() && m_netAccess;
    m_open->setEnabled(usable);
    m_save->setEnabled(usable);
}

void AttachmentPanel::updateVisibility()
{
    // A never-shown child reports isHidden() without being explicitly hidden, so track what was applied.
    const bool show = shouldBeShown();
    if (m_appliedVisibility == int(show))
        return;
    m_appliedVisibility = int(show);
    setVisible(show);
    emit relevanceChanged(show);
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
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}

void AttachmentPanel::openCurrent()
{
    const QPersistentModelIndex part = currentPart();
    if (!part.isValid() || !m_netAccess)
        return;

    auto *manager = new Imap::Network::FileDownloadManager(this, m_netAccess, part);
    auto fileName = std::make_shared<QString>();
    connect(manager, &Imap::Network::FileDownloadManager::fileNameRequested, manager, [fileName](QString *requested) {
        // Keep the file after the QTemporaryFile object goes away; the external application needs it.
        QTemporaryFile file(QDir::tempPath() + QLatin1String("/plasma-impresario-attachment-XXXXXX-")
                            + requested->replace(QLatin1Char('/'), QLatin1Char('_')));
        file.setAutoRemove(false);
        *requested = file.open() ? file.fileName() : QString();
        *fileName = *requested;
    });
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, [manager, fileName]() {
        // Read-only, so that the launched application does not attempt to modify it
        QFile::setPermissions(*fileName, QFile::ReadOwner);
        QDesktopServices::openUrl(QUrl::fromLocalFile(*fileName));
        manager->deleteLater();
    });
    connect(manager, &Imap::Network::FileDownloadManager::transferError, this, [this, manager, fileName](const QString &error) {
        if (!fileName->isEmpty())
            QFile::remove(*fileName);
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
        static QDir lastDir(Common::writablePath(Common::LOCATION_DOWNLOAD));
        if (!lastDir.exists())
            lastDir = QDir(Common::writablePath(Common::LOCATION_DOWNLOAD));
        *requested = QFileDialog::getSaveFileName(this, i18nc("@title:window", "Save Attachment"), lastDir.filePath(*requested),
                                                  QString(), nullptr, QFileDialog::HideNameFilterDetails);
        if (!requested->isEmpty())
            lastDir = QFileInfo(*requested).absoluteDir();
    });
    connect(manager, &Imap::Network::FileDownloadManager::transferError, this, &AttachmentPanel::transferError);
    connect(manager, &Imap::Network::FileDownloadManager::transferError, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::cancelled, manager, &QObject::deleteLater);
    connect(manager, &Imap::Network::FileDownloadManager::succeeded, manager, &QObject::deleteLater);
    manager->downloadPart();
}

}
