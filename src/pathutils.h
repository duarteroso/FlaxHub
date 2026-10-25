#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QDir>
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

#endif
