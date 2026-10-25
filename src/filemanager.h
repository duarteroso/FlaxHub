#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Opens folders in the platform's file manager. A path to a file opens the folder that holds it, so a saved
// path never launches the file itself. Also converts between the paths the settings save and the URLs that
// folder pickers use.
class FileManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString revealLabel READ revealLabel CONSTANT)

public:
    explicit FileManager(QObject *parent = nullptr);

    // The menu text for showing a folder in the file manager on this platform.
    QString revealLabel() const;

    Q_INVOKABLE bool reveal(const QString &path);

    // The local path for a URL that a folder picker returned.
    Q_INVOKABLE QString toLocalPath(const QUrl &url) const;

    // The URL a folder picker should open at for path. A "~" is expanded, and a missing path falls back to the
    // nearest existing folder, or to the home folder when none exists.
    Q_INVOKABLE QUrl folderUrl(const QString &path) const;

signals:
    // reveal() failed. Main.qml shows it.
    void errorOccurred(const QString &title, const QString &message);
};

#endif
