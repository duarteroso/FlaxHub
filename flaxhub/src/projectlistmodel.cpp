#include "projectlistmodel.h"

#include "engineregistry.h"
#include "installrules.h"
#include "pathutils.h"
#include "projectcreator.h"
#include "settingsarray.h"

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

// The engine version a project file names. Empty when the file cannot be read, is not JSON, or names none.
QString minEngineVersion(const QString &projectFile)
{
    QFile file(projectFile);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("MinEngineVersion")).toString();
}

// The absolute path with symbolic links and "." and ".." resolved, so that two spellings of one folder give one path.
// A folder that does not exist yet is resolved through its nearest existing parent, since create() makes it.
QString canonicalPath(const QString &path)
{
    const QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    if (info.exists())
        return info.canonicalFilePath();

    const QString parent = info.absolutePath();
    if (parent == info.absoluteFilePath())
        return info.absoluteFilePath();

    return QDir(canonicalPath(parent)).filePath(info.fileName());
}

// Whether two canonical paths name the same folder. Windows and macOS folder names ignore case by default.
bool samePath(const QString &a, const QString &b)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

// Splits a command line at white space outside quotes. Unlike QProcess::splitCommand, the quotes stay in the argument,
// since the engine reads a value's quotes to find where the value ends. A quote opens only where a word or a value
// starts, so an apostrophe inside a word, as in don't, is an ordinary character.
QStringList splitKeepingQuotes(const QString &commandLine)
{
    QStringList arguments;
    QString current;
    QChar quote;
    for (const QChar c : commandLine) {
        if (quote.isNull() && c.isSpace()) {
            if (!current.isEmpty())
                arguments.append(current);
            current.clear();
            continue;
        }
        // A value starts after "=", as in -title="My game".
        const bool startsValue = current.isEmpty() || current.endsWith(QLatin1Char('='));
        if (quote.isNull() && startsValue && (c == QLatin1Char('"') || c == QLatin1Char('\'')))
            quote = c;
        else if (c == quote)
            quote = QChar();
        current += c;
    }
    if (!current.isEmpty())
        arguments.append(current);

    return arguments;
}

} // namespace

ProjectListModel::ProjectListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_ownedSettings(std::make_unique<QSettings>())
    , m_settings(m_ownedSettings.get())
{
    loadProjects();
}

ProjectListModel::ProjectListModel(QSettings *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
    loadProjects();
}

ProjectListModel::~ProjectListModel() = default;

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

EngineRegistry *ProjectListModel::installs() const
{
    return m_installs.data();
}

void ProjectListModel::setInstalls(EngineRegistry *installs)
{
    if (m_installs == installs)
        return;

    if (m_installs)
        disconnect(m_installs, nullptr, this, nullptr);

    m_installs = installs;

    // Only the signal that can change which engines are installed. Log and progress lines do not emit it.
    if (m_installs)
        connect(m_installs, &EngineRegistry::installedEnginesChanged, this, &ProjectListModel::refreshInstalled);

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

void ProjectListModel::open(const QString &path, const QString &editorArguments)
{
    const int row = rowFor(path);
    if (row < 0) {
        emit errorOccurred(qtTrId("main.projectlistmodel.open_title"),
                           qtTrId("main.projectlistmodel.open_not_listed").arg(collapseHome(path)));
        return;
    }

    Project &project = m_projects[row];
    const QString editor = m_installs ? m_installs->editorExecutable(project.engineVersion) : QString();
    if (editor.isEmpty()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.open_title"),
                           qtTrId("main.projectlistmodel.open_engine_missing")
                               .arg(project.engineVersion, collapseHome(path)));
        return;
    }

    // Quoted as create() quotes it, since the engine reads a path with spaces up to the closing quote. The editor
    // is a GUI program, so it is started detached and keeps running after the Hub closes.
    const QString projectFile = QDir(project.path).filePath(project.name + QStringLiteral(".flaxproj"));
    if (!QFileInfo::exists(projectFile)) {
        emit errorOccurred(qtTrId("main.projectlistmodel.open_title"),
                           qtTrId("main.projectlistmodel.open_missing").arg(collapseHome(projectFile)));
        return;
    }

    // The saved arguments follow the project. A shell would strip their quotes, but on Linux and macOS the engine joins
    // its arguments with spaces and reads the quotes itself (Main/MainUtil.h), so -title="My game" must keep them.
    QStringList arguments = {QStringLiteral("-project=\"%1\"").arg(projectFile)};
    arguments += splitKeepingQuotes(editorArguments);
    if (!QProcess::startDetached(editor, arguments)) {
        emit errorOccurred(qtTrId("main.projectlistmodel.open_title"),
                           qtTrId("main.projectlistmodel.open_editor_failed").arg(collapseHome(path)));
        return;
    }

    project.lastOpened = QDateTime::currentDateTime();
    emit dataChanged(index(row), index(row), {LastOpenedRole});
    saveProjects();
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
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_name_invalid"));
        return;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_name_slashes"));
        return;
    }
    if (expandHome(location).isEmpty()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_location_missing"));
        return;
    }
    if (!InstallRules::isFullPath(expandHome(location))) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_location_not_full"));
        return;
    }

    const QString editor = m_installs ? m_installs->editorExecutable(engineVersion) : QString();
    if (editor.isEmpty()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_engine_missing").arg(engineVersion));
        return;
    }

    // The editor makes the folder itself, so the Hub only checks that the target is free.
    Project project;
    project.name = sanitizedName(name);
    project.path = canonicalPath(QDir(expandHome(location)).absoluteFilePath(name));
    project.engineVersion = engineVersion;
    project.pinned = false;
    if (rowFor(project.path) >= 0) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.already_listed").arg(collapseHome(project.path)));
        return;
    }
    if (QFileInfo::exists(project.path) && !InstallRules::isEmptyFolder(project.path)) {
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"),
                           qtTrId("main.projectlistmodel.create_folder_not_empty").arg(collapseHome(project.path)));
        return;
    }

    const QString projectFile = QDir(project.path).filePath(project.name + QStringLiteral(".flaxproj"));
    ProjectCreator *creator = new ProjectCreator(this);
    setCreating(true);
    connect(creator, &ProjectCreator::created, this, [this, project]() {
        finishCreate(project);
    });
    connect(creator, &ProjectCreator::failed, this, [this](const QString &message) {
        setCreating(false);
        emit errorOccurred(qtTrId("main.projectlistmodel.create_title"), message);
    });
    creator->start(editor, project.path, projectFile);
}

void ProjectListModel::addExisting(const QString &folder)
{
    if (folder.trimmed().isEmpty()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.add_title"),
                           qtTrId("main.projectlistmodel.add_folder_missing"));
        return;
    }

    if (!InstallRules::isFullPath(expandHome(folder.trimmed()))) {
        emit errorOccurred(qtTrId("main.projectlistmodel.add_title"),
                           qtTrId("main.projectlistmodel.add_folder_not_full"));
        return;
    }

    const QString path = canonicalPath(expandHome(folder));
    if (!QFileInfo(path).isDir()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.add_title"),
                           qtTrId("main.projectlistmodel.add_not_folder").arg(collapseHome(path)));
        return;
    }
    if (rowFor(path) >= 0) {
        emit errorOccurred(qtTrId("main.projectlistmodel.add_title"),
                           qtTrId("main.projectlistmodel.already_listed").arg(collapseHome(path)));
        return;
    }

    const QStringList files = QDir(path).entryList({QStringLiteral("*.flaxproj")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        emit errorOccurred(qtTrId("main.projectlistmodel.add_title"),
                           qtTrId("main.projectlistmodel.add_no_project_file").arg(collapseHome(path)));
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

// Restores the projects that the last run saved. Folders are not checked here, so a project whose folder has gone
// stays listed, and open() reports it when the user tries to open it.
void ProjectListModel::loadProjects()
{
    SettingsArray::read(*m_settings, projectsGroup, [this](QSettings &settings) {
        Project project;
        project.name = settings.value("name").toString();
        project.path = settings.value("path").toString();
        project.engineVersion = settings.value("engineVersion").toString();
        project.lastOpened = settings.value("lastOpened").toDateTime();
        project.pinned = settings.value("pinned").toBool();
        if (project.path.isEmpty())
            return;

        // A list saved before paths were canonical may hold another spelling of a folder, which is then the same project.
        project.path = canonicalPath(project.path);
        if (rowFor(project.path) >= 0)
            return;

        m_projects.append(project);
    });
}

// Saves every project in the list, so the list comes back the same on the next run.
void ProjectListModel::saveProjects()
{
    const bool saved = SettingsArray::write(*m_settings, projectsGroup, m_projects, [](QSettings &settings, const Project &project) {
        settings.setValue("name", project.name);
        settings.setValue("path", project.path);
        settings.setValue("engineVersion", project.engineVersion);
        settings.setValue("lastOpened", project.lastOpened);
        settings.setValue("pinned", project.pinned);
        return true;
    });
    if (!saved) {
        emit errorOccurred(qtTrId("main.projectlistmodel.save_title"), qtTrId("main.projectlistmodel.save_failed"));
    }
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
        if (samePath(m_projects.at(row).path, path))
            return row;
    }

    return -1;
}

// Emits EngineInstalledRole for every project only when the set of installed engines has changed. The install list
// reports each change of state, and most of them leave the set as it was.
void ProjectListModel::refreshInstalled()
{
    const QStringList installed = m_installs ? m_installs->installedVersions() : QStringList();
    const QSet<QString> installedSet(installed.cbegin(), installed.cend());
    if (installedSet == m_installedVersions)
        return;

    m_installedVersions = installedSet;
    if (m_projects.isEmpty())
        return;

    emit dataChanged(index(0), index(m_projects.size() - 1), {EngineInstalledRole});
}
