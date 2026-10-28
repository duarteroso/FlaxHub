#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The preferences the Settings view sets. Each one is saved in QSettings under the General group, with the key of
// its property, which is how the QML Settings type saved them before. Saved settings therefore still load.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString installDirectory READ installDirectory WRITE setInstallDirectory NOTIFY installDirectoryChanged)
    Q_PROPERTY(QString projectsDirectory READ projectsDirectory WRITE setProjectsDirectory NOTIFY projectsDirectoryChanged)
    Q_PROPERTY(QString editorArguments READ editorArguments WRITE setEditorArguments NOTIFY editorArgumentsChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);

    QString installDirectory() const;
    void setInstallDirectory(const QString &installDirectory);

    QString projectsDirectory() const;
    void setProjectsDirectory(const QString &projectsDirectory);

    // Extra arguments for the editor, after the project. A project's open passes them on.
    QString editorArguments() const;
    void setEditorArguments(const QString &editorArguments);

signals:
    // A setting could not be saved, or a folder setting was refused. Main.qml shows it.
    void errorOccurred(const QString &title, const QString &message);
    void themeChanged();
    void installDirectoryChanged();
    void projectsDirectoryChanged();
    void editorArgumentsChanged();

private:
    // Saves one value and reports a failed write through errorOccurred.
    void save(const char *key, const QString &value);

    QString m_theme;
    QString m_installDirectory;
    QString m_projectsDirectory;
    QString m_editorArguments;
};

#endif
