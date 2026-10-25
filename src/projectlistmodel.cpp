#include "projectlistmodel.h"

#include "installlistmodel.h"
#include "pathutils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStringList>

namespace {

constexpr const char *projectsGroup = "Projects";

// The editor makes a new project only in a folder that does not exist or has nothing in it.
bool isEmptyFolder(const QString &path)
{
    return QFileInfo(path).isDir() && QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
}

// The engine version a project file names. Empty when the file cannot be read, is not JSON, or names none.
QString minEngineVersion(const QString &projectFile)
{
    QFile file(projectFile);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("MinEngineVersion")).toString();
}

} // namespace

ProjectListModel::ProjectListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadProjects();
}

int ProjectListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_projects.size();
}

QVariant ProjectListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_projects.size())
        return {};

    const Project &project = m_projects.at(index.row());
    switch (role) {
    case NameRole: return project.name;
    case PathRole: return project.path;
    case EngineVersionRole: return project.engineVersion;
    case LastOpenedRole: return project.lastOpened;
    case PinnedRole: return project.pinned;
    case EngineInstalledRole: return m_installs && m_installs->isInstalled(project.engineVersion);
    }

    return {};
}

bool ProjectListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_projects.size())
        return false;

    Project &project = m_projects[index.row()];
    switch (role) {
    case EngineVersionRole:
        project.engineVersion = value.toString();
        emit dataChanged(index, index, {EngineVersionRole, EngineInstalledRole});
        saveProjects();
        return true;
    case PinnedRole:
        project.pinned = value.toBool();
        emit dataChanged(index, index, {PinnedRole});
        saveProjects();
        return true;
    }

    return false;
}

Qt::ItemFlags ProjectListModel::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> ProjectListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {EngineVersionRole, "engineVersion"},
        {LastOpenedRole, "lastOpened"},
        {PinnedRole, "pinned"},
        {EngineInstalledRole, "engineInstalled"},
    };
}

InstallListModel *ProjectListModel::installs() const
{
    return m_installs;
}

void ProjectListModel::setInstalls(InstallListModel *installs)
{
    if (m_installs == installs)
        return;

    if (m_installs)
        disconnect(m_installs, nullptr, this, nullptr);

    m_installs = installs;

    if (m_installs) {
        connect(m_installs, &QAbstractItemModel::dataChanged, this, &ProjectListModel::refreshInstalled);
        connect(m_installs, &QAbstractItemModel::rowsInserted, this, &ProjectListModel::refreshInstalled);
        connect(m_installs, &QAbstractItemModel::rowsRemoved, this, &ProjectListModel::refreshInstalled);
        connect(m_installs, &QAbstractItemModel::modelReset, this, &ProjectListModel::refreshInstalled);
    }

    emit installsChanged();
    refreshInstalled();
}

void ProjectListModel::remove(const QString &path)
{
    const int row = rowFor(path);
    if (row < 0)
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_projects.removeAt(row);
    endRemoveRows();
    saveProjects();
}

void ProjectListModel::setPinned(const QString &path, bool pinned)
{
    const int row = rowFor(path);
    if (row < 0)
        return;

    setData(index(row), pinned, PinnedRole);
}

bool ProjectListModel::open(const QString &path)
{
    for (int row = 0; row < m_projects.size(); ++row) {
        Project &project = m_projects[row];
        if (project.path != path)
            continue;

        const QString editor = m_installs ? m_installs->editorExecutable(project.engineVersion) : QString();
        if (editor.isEmpty()) {
            qWarning() << "Engine" << project.engineVersion << "is not installed, so" << path << "cannot be opened";
            return false;
        }

        // Quoted as create() quotes it, since the engine reads a path with spaces up to the closing quote. The editor
        // is a GUI program, so it is started detached and keeps running after the Hub closes.
        const QString projectFile = QDir(project.path).filePath(project.name + QStringLiteral(".flaxproj"));
        if (!QFileInfo::exists(projectFile)) {
            qWarning() << "The project file" << projectFile << "is missing, so" << path << "cannot be opened";
            return false;
        }

        const QStringList arguments = {QStringLiteral("-project=\"%1\"").arg(projectFile)};
        if (!QProcess::startDetached(editor, arguments)) {
            qWarning() << "Could not start" << editor << "for" << path;
            return false;
        }

        project.lastOpened = QDateTime::currentDateTime();
        emit dataChanged(index(row), index(row), {LastOpenedRole});
        saveProjects();
        return true;
    }

    return false;
}

bool ProjectListModel::creating() const
{
    return m_creating;
}

QString ProjectListModel::sanitizedName(const QString &folderName) const
{
    // The editor names the project after its folder, keeping only letters and digits (LoadProduct in Editor.cpp).
    QString name;
    for (const QChar c : folderName) {
        if (c.isLetterOrNumber())
            name += c;
    }
    return name;
}

void ProjectListModel::create(const QString &folderName, const QString &location, const QString &engineVersion)
{
    if (m_creating)
        return;

    const QString name = folderName.trimmed();
    if (sanitizedName(name).isEmpty()) {
        emit createFailed(QStringLiteral("Use at least one letter or digit in the name."));
        return;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        emit createFailed(QStringLiteral("The name cannot contain slashes."));
        return;
    }
    if (expandHome(location).isEmpty()) {
        emit createFailed(QStringLiteral("Choose a location for the project."));
        return;
    }

    const QString editor = m_installs ? m_installs->editorExecutable(engineVersion) : QString();
    if (editor.isEmpty()) {
        emit createFailed(QStringLiteral("Engine %1 is not installed.").arg(engineVersion));
        return;
    }

    // The editor makes the folder itself, so the Hub only checks that the target is free.
    Project project;
    project.name = sanitizedName(name);
    project.path = QDir(expandHome(location)).absoluteFilePath(name);
    project.engineVersion = engineVersion;
    project.pinned = false;
    const QString projectFile = QDir(project.path).filePath(project.name + QStringLiteral(".flaxproj"));
    const bool existed = QFileInfo::exists(project.path);
    if (existed && !isEmptyFolder(project.path)) {
        emit createFailed(QStringLiteral("%1 already exists and is not empty.").arg(project.path));
        return;
    }

    // The engine joins its arguments into one command line and does not strip quotes, so the quotes stay in the
    // argument. The engine then reads a path with spaces up to the closing quote, as the editor does for itself.
    const QStringList arguments = {
        QStringLiteral("-new"),
        QStringLiteral("-exit"),
        QStringLiteral("-headless"),
        QStringLiteral("-std"),
        QStringLiteral("-project=\"%1\"").arg(project.path),
    };

    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());
    m_createOutput.clear();
    setCreating(true);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        m_createOutput += process->readAllStandardOutput();
    });
    connect(process, &QProcess::finished, this, [this, process, project, projectFile, existed](int exitCode, QProcess::ExitStatus exitStatus) {
        m_createOutput += process->readAllStandardOutput();
        process->deleteLater();

        if (exitStatus == QProcess::CrashExit)
            failCreate(project.path, existed, QStringLiteral("FlaxEditor crashed."));
        else if (exitCode != 0)
            failCreate(project.path, existed, QStringLiteral("FlaxEditor exited with code %1.").arg(exitCode));
        else if (!QFileInfo::exists(projectFile))
            failCreate(project.path, existed, QStringLiteral("FlaxEditor did not create %1.").arg(projectFile));
        else
            finishCreate(project);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, project, existed](QProcess::ProcessError error) {
        // Any other error is followed by finished, which reports it.
        if (error != QProcess::FailedToStart)
            return;

        process->deleteLater();
        failCreate(project.path, existed, QStringLiteral("Could not start FlaxEditor: ") + process->errorString());
    });

    process->start(editor, arguments);
}

void ProjectListModel::addExisting(const QString &folder)
{
    if (folder.trimmed().isEmpty()) {
        emit addFailed(QStringLiteral("Choose a folder for the project."));
        return;
    }

    const QString path = QDir::cleanPath(QFileInfo(expandHome(folder)).absoluteFilePath());
    if (!QFileInfo(path).isDir()) {
        emit addFailed(QStringLiteral("%1 is not a folder.").arg(path));
        return;
    }
    if (rowFor(path) >= 0) {
        emit addFailed(QStringLiteral("%1 is already in the list.").arg(path));
        return;
    }

    const QStringList files = QDir(path).entryList({QStringLiteral("*.flaxproj")}, QDir::Files);
    if (files.isEmpty()) {
        emit addFailed(QStringLiteral("No .flaxproj file found in %1.").arg(path));
        return;
    }

    // The editor names the project after its .flaxproj file, so the file's base name is the name that open() needs.
    const QString projectFile = QDir(path).filePath(files.first());
    QString engineVersion = minEngineVersion(projectFile);
    if (engineVersion.isEmpty() && m_installs)
        engineVersion = m_installs->preferredVersion();

    Project project;
    project.name = QFileInfo(files.first()).completeBaseName();
    project.path = path;
    project.engineVersion = engineVersion;
    project.lastOpened = QDateTime::currentDateTime();
    project.pinned = false;
    insertProject(project);
}

void ProjectListModel::setCreating(bool creating)
{
    if (m_creating == creating)
        return;

    m_creating = creating;
    emit creatingChanged();
}

void ProjectListModel::finishCreate(const Project &project)
{
    Project added = project;
    added.lastOpened = QDateTime::currentDateTime();
    insertProject(added);

    setCreating(false);
    emit created(added.path);
}

void ProjectListModel::failCreate(const QString &projectDir, bool existed, const QString &message)
{
    // A folder the editor made is removed again, so a retry is not blocked by it.
    if (!existed)
        QDir(projectDir).removeRecursively();

    // The last line the editor printed usually says why it failed, so it follows the message.
    const QStringList lines = QString::fromUtf8(m_createOutput).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString reason = lines.isEmpty() ? QString() : lines.last().trimmed();

    setCreating(false);
    emit createFailed(reason.isEmpty() ? message : message + QLatin1Char(' ') + reason);
}

// Restores the projects that the last run saved. Folders are not checked here, so a project whose folder has gone
// stays listed, and open() warns when the user tries to open it.
void ProjectListModel::loadProjects()
{
    QSettings settings;
    const int count = settings.beginReadArray(projectsGroup);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Project project;
        project.name = settings.value("name").toString();
        project.path = settings.value("path").toString();
        project.engineVersion = settings.value("engineVersion").toString();
        project.lastOpened = settings.value("lastOpened").toDateTime();
        project.pinned = settings.value("pinned").toBool();
        if (project.path.isEmpty() || rowFor(project.path) >= 0)
            continue;

        m_projects.append(project);
    }
    settings.endArray();
}

// Saves every project in the list, so the list comes back the same on the next run.
void ProjectListModel::saveProjects() const
{
    QSettings settings;
    settings.remove(projectsGroup);
    settings.beginWriteArray(projectsGroup);
    for (int i = 0; i < m_projects.size(); ++i) {
        const Project &project = m_projects.at(i);
        settings.setArrayIndex(i);
        settings.setValue("name", project.name);
        settings.setValue("path", project.path);
        settings.setValue("engineVersion", project.engineVersion);
        settings.setValue("lastOpened", project.lastOpened);
        settings.setValue("pinned", project.pinned);
    }
    settings.endArray();
}

// Puts a project at the top of the list, where the newest ones go, and saves it.
void ProjectListModel::insertProject(const Project &project)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_projects.prepend(project);
    endInsertRows();
    saveProjects();
}

int ProjectListModel::rowFor(const QString &path) const
{
    for (int row = 0; row < m_projects.size(); ++row) {
        if (m_projects.at(row).path == path)
            return row;
    }

    return -1;
}

void ProjectListModel::refreshInstalled()
{
    if (m_projects.isEmpty())
        return;

    emit dataChanged(index(0), index(m_projects.size() - 1), {EngineInstalledRole});
}
