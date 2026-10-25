#ifndef VERSIONUTILS_H
#define VERSIONUTILS_H

#include <QString>
#include <QStringList>

// Compares part by part as numbers, so "1.10" is newer than "1.9".
inline bool isNewerVersion(const QString &a, const QString &b)
{
    const QStringList partsA = a.split('.');
    const QStringList partsB = b.split('.');
    for (int i = 0; i < qMin(partsA.size(), partsB.size()); ++i) {
        const int partA = partsA.at(i).toInt();
        const int partB = partsB.at(i).toInt();
        if (partA != partB)
            return partA > partB;
    }

    return partsA.size() > partsB.size();
}

#endif
