#include "filemanager.h"
#include "pathutils.h"

#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>

namespace {

// The folder to show for path: path itself when it is a folder, its parent when it is a file, or the nearest
// parent that exists when it is missing. Empty when nothing above path exists.
QString folderToShow(const QString &path)
{
    QFileInfo info(QDir::cleanPath(path));
    if (info.isFile())
        return info.absolutePath();

    while (!info.isDir()) {
        const QString parent = info.absolutePath();
        if (parent == info.filePath())
            return {};

        info.setFile(parent);
    }

    return info.absoluteFilePath();
}

} // namespace

FileManager::FileManager(QObject *parent)
    : QObject(parent)
{
}

QString FileManager::revealLabel() const
{
#if defined(Q_OS_MACOS)
    return qtTrId("main.filemanager.reveal_finder");
#elif defined(Q_OS_WIN)
    return qtTrId("main.filemanager.reveal_explorer");
#else
    return qtTrId("main.filemanager.reveal_default");
#endif
}

bool FileManager::reveal(const QString &path)
{
    if (path.isEmpty()) {
        qWarning() << "Cannot show an empty path in the file manager";
        emit errorOccurred(qtTrId("main.filemanager.cannot_show_title"), qtTrId("main.filemanager.no_folder"));
        return false;
    }

    const QString folder = folderToShow(path);
    if (folder.isEmpty()) {
        qWarning() << "No existing folder to show for" << path;
        emit errorOccurred(qtTrId("main.filemanager.cannot_show_title"),
                           qtTrId("main.filemanager.no_folder_exists").arg(path));
        return false;
    }

    // Qt hands the folder to Finder, Explorer or the desktop's file manager, whichever the platform uses.
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder))) {
        qWarning() << "Could not open" << folder << "in the file manager";
        emit errorOccurred(qtTrId("main.filemanager.cannot_show_title"),
                           qtTrId("main.filemanager.open_failed").arg(folder));
        return false;
    }

    return true;
}

QString FileManager::toLocalPath(const QUrl &url) const
{
    return url.toLocalFile();
}

QUrl FileManager::folderUrl(const QString &path) const
{
    const QString folder = folderToShow(expandHome(path));
    return QUrl::fromLocalFile(folder.isEmpty() ? QDir::homePath() : folder);
}

void FileManager::copyPath(const QString &path) const
{
    QGuiApplication::clipboard()->setText(path);
}
