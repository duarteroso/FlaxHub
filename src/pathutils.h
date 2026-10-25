#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QDir>
#include <QFileInfo>
#include <QString>

// The paths come from the settings as "~/...", and neither the file system nor git expands the "~".
inline QString expandHome(const QString &path)
{
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);

#if defined(Q_OS_WIN)
    // Windows paths are often typed with backslashes, so "~\" is the home folder there. Elsewhere a backslash is a
    // character of a file name.
    if (path.startsWith(QLatin1String("~\\")))
        return QDir::homePath() + QDir::fromNativeSeparators(path.mid(1));
#endif

    return path;
}

// The FlaxHub folder in the home folder, as a full path. The default folders for engines and projects are inside it.
inline QString hubFolder()
{
    return QDir::homePath() + QStringLiteral("/FlaxHub");
}

// The home folder shown as "~", the reverse of expandHome. Only the home folder and the paths inside it change, and
// only at a folder boundary, so "/home/user2" is left as it is. Saved paths are canonical, so the canonical home
// folder is tried too, since it differs where the home folder is reached through a symbolic link.
inline QString collapseHome(const QString &path)
{
#if defined(Q_OS_WIN)
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif

    const QString cleaned = QDir::cleanPath(path);
    const QString home = QDir::cleanPath(QDir::homePath());
    const QString canonicalHome = QDir::cleanPath(QFileInfo(home).canonicalFilePath());
    for (const QString &folder : {home, canonicalHome}) {
        if (folder.isEmpty())
            continue;

        if (cleaned.compare(folder, sensitivity) == 0)
            return QStringLiteral("~");
        if (cleaned.startsWith(folder + QLatin1Char('/'), sensitivity))
            return QStringLiteral("~") + cleaned.mid(folder.size());
    }

    return path;
}

#endif
