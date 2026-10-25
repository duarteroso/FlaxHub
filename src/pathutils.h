#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QDir>
#include <QString>

// The paths come from the settings as "~/...", and neither the file system nor git expands the "~".
inline QString expandHome(const QString &path)
{
    if (path == QLatin1String("~") || path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);

    return path;
}

#endif
