/* Copyright (C) 2026 Plasma Impresario contributors
   SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDBusArgument>
#include <QFileDialog>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class QDBusConnection;
class QWidget;

namespace Gui {

/** @short Choosing files and folders to open, through the desktop's file chooser

Asks the XDG desktop portal's file chooser first, so that whichever program serves it on the desktop, such as Plasma
or Plasma Opus, shows its dialog. When no portal answers, Qt's file dialog appears, which Plasma's platform theme
shows as the KDE dialog. The window that asked is disabled while the portal's dialog is open.
*/
class FileChooser
{
public:
    enum class Mode {
        File,
        Files,
        Folder,
    };

    struct Filter {
        QString name;
        QStringList patterns;
    };

    struct Request {
        QString title;
        Mode mode = Mode::File;
        /** Where the dialog starts; the portal is told only about local folders */
        QUrl folder;
        QList<Filter> filters;
        /** Options for Qt's file dialog when no portal answers */
        QFileDialog::Options fallbackOptions;
        /** Offer only local files in Qt's file dialog; the portal returns local files anyway */
        bool localOnly = false;
    };

    enum class Outcome {
        Chosen,
        Cancelled,
        /** No portal file chooser answered, or it cannot do what was asked */
        Unavailable,
    };

    struct Result {
        Outcome outcome = Outcome::Unavailable;
        QList<QUrl> urls;
    };

    /** @short Choose through the portal, or through Qt's file dialog when no portal answers

    Returns no URLs when nothing was chosen.
    */
    static QList<QUrl> choose(QWidget *parent, const Request &request);
    /** @short The local path of a single file or folder chosen by choose(), or an empty string */
    static QString chooseLocalPath(QWidget *parent, const Request &request);

    /** @short Ask the portal file chooser served at @p service on @p bus, waiting for its answer

    An empty @p service addresses the peer of a peer-to-peer connection.
    */
    static Result askPortal(QWidget *parent, const Request &request, QDBusConnection bus,
                            const QString &service);

    // Exposed for the tests
    static QString portalService();
    static QVariantMap portalOptions(const Request &request, const QString &handleToken);
    static Result portalResult(uint response, const QVariantMap &results);
    static QString requestPath(const QString &uniqueName, const QString &handleToken);
    static QString fallbackFilter(const QList<Filter> &filters);
};

/** @short One rule of a portal filter: 0 for a glob pattern, 1 for a MIME type */
struct PortalFilterRule {
    uint type = 0;
    QString pattern;
};

struct PortalFilter {
    QString name;
    QList<PortalFilterRule> rules;
};

using PortalFilters = QList<PortalFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule);
QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilter &filter);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilter &filter);

}

Q_DECLARE_METATYPE(Gui::PortalFilterRule)
Q_DECLARE_METATYPE(Gui::PortalFilter)
