#include "appsettings.h"

#include "installlayout.h"
#include "pathutils.h"

#include <QDir>
#include <QSettings>

namespace {

// The group the values are saved under. The QML Settings type saved them under the same group.
constexpr const char *settingsGroup = "General";

// Each value is written as soon as it changes, so a change is not lost if the app is closed later.
bool saveValue(const char *key, const QString &value)
{
    QSettings settings;
    settings.beginGroup(settingsGroup);
    settings.setValue(key, value);
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

// A folder setting is saved as typed, with a "~" for the home folder, so only the expanded form is checked.
bool isFullFolder(const QString &folder)
{
    return QDir::isAbsolutePath(expandHome(folder));
}

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    settings.beginGroup(settingsGroup);
    m_installDirectory = settings.value("installDirectory", InstallLayout::defaultInstallDirectory()).toString();
    m_projectsDirectory = settings.value("projectsDirectory", hubFolder() + QStringLiteral("/Projects")).toString();
    m_editorArguments = settings.value("editorArguments").toString();
}

QString AppSettings::installDirectory() const
{
    return m_installDirectory;
}

void AppSettings::setInstallDirectory(const QString &installDirectory)
{
    if (m_installDirectory == installDirectory)
        return;

    if (!isFullFolder(installDirectory)) {
        emit errorOccurred(qtTrId("main.appsettings.install_folder_title"),
                           qtTrId("main.appsettings.install_folder_not_full_path"));
        return;
    }

    m_installDirectory = installDirectory;
    save("installDirectory", installDirectory);
    emit installDirectoryChanged();
}

QString AppSettings::projectsDirectory() const
{
    return m_projectsDirectory;
}

void AppSettings::setProjectsDirectory(const QString &projectsDirectory)
{
    if (m_projectsDirectory == projectsDirectory)
        return;

    if (!isFullFolder(projectsDirectory)) {
        emit errorOccurred(qtTrId("main.appsettings.projects_folder_title"),
                           qtTrId("main.appsettings.projects_folder_not_full_path"));
        return;
    }

    m_projectsDirectory = projectsDirectory;
    save("projectsDirectory", projectsDirectory);
    emit projectsDirectoryChanged();
}

QString AppSettings::editorArguments() const
{
    return m_editorArguments;
}

void AppSettings::setEditorArguments(const QString &editorArguments)
{
    if (m_editorArguments == editorArguments)
        return;

    m_editorArguments = editorArguments;
    save("editorArguments", editorArguments);
    emit editorArgumentsChanged();
}

void AppSettings::save(const char *key, const QString &value)
{
    if (!saveValue(key, value))
        emit errorOccurred(qtTrId("main.appsettings.not_saved_title"), qtTrId("main.appsettings.not_saved"));
}
