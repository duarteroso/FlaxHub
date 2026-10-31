#include "installrules.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QStringList>

namespace InstallRules {

using Failure = InstallValidator::Failure;

bool isFullPath(const QString &path)
{
    return QDir::isAbsolutePath(path);
}

bool isEmptyFolder(const QString &path)
{
    return QFileInfo(path).isDir() && QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
}

bool isFreeFolder(const QString &path)
{
    return !QFileInfo::exists(path) || isEmptyFolder(path);
}

// A root has no folder above it to stay within. On Windows that is a drive such as "C:/", or a UNC server or share such
// as "//server/share". QDir::isRoot covers the drive, and the UNC rule is checked here as well.
static bool isRootFolder(const QString &folder)
{
    if (QDir(folder).isRoot())
        return true;

#if defined(Q_OS_WIN)
    // After the leading "//", a root has at most a server name and a share name.
    if (folder.startsWith(QStringLiteral("//")))
        return folder.mid(2).split(QLatin1Char('/'), Qt::SkipEmptyParts).size() <= 2;
#endif

    return false;
}

bool isSafeToRemove(const QString &path)
{
    if (!QDir::isAbsolutePath(path))
        return false;

    // A link is refused, since a delete through it empties the folder it points to. The link check is made on the cleaned
    // path, since a trailing "/" makes QFileInfo follow a link. A path that is not a folder is refused too.
    const QFileInfo info(path);
    if (QFileInfo(QDir::cleanPath(path)).isSymLink() || !info.isDir())
        return false;

    const QString folder = info.canonicalFilePath();
    if (folder.isEmpty() || isRootFolder(folder))
        return false;

    // Folder names ignore case on Windows and macOS, so a folder that differs from home only in case is home too.
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif
    const QString home = QDir::home().canonicalPath();
    return home.compare(folder, caseSensitivity) != 0 && !home.startsWith(folder + QLatin1Char('/'), caseSensitivity);
}

QString markerPath(const QString &path)
{
    return QDir(QDir::cleanPath(path)).filePath(QStringLiteral(".flaxhub-install"));
}

bool writeMarker(const QString &path)
{
    QFile marker(markerPath(path));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    const QByteArray text("Created by FlaxHub. Uninstall in FlaxHub deletes the folder this file is in.\n");
    if (marker.write(text) != text.size())
        return false;

    // close() flushes what is buffered and returns nothing, so its result is read from the file's error.
    const bool flushed = marker.flush();
    marker.close();
    return flushed && marker.error() == QFileDevice::NoError;
}

bool hasMarker(const QString &path)
{
    return QFileInfo(markerPath(path)).isFile();
}

bool mayUninstall(const QString &path, bool completeCheckout)
{
    return isSafeToRemove(path) && (hasMarker(path) || completeCheckout);
}

bool needsClone(const QList<Failure> &failures)
{
    return failures.contains(Failure::PathMissing) || failures.contains(Failure::NoGitCheckout);
}

bool needsClear(const QList<Failure> &failures)
{
    return failures.contains(Failure::NoGitCheckout) && !failures.contains(Failure::PathMissing)
        && !failures.contains(Failure::FolderEmpty);
}

} // namespace InstallRules
