#include "installlistmodel.h"
#include "installlayout.h"
#include "installvalidator.h"
#include "pathutils.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QtConcurrentRun>

#include <algorithm>

namespace {

// GitHub returns at most 100 tags per page, so a shorter page is the last one.
constexpr int tagsPerPage = 100;

// The newest versions. The Add engine dialog lists these first and the rest under Legacy.
constexpr int recentVersionCount = 4;

// The settings group that holds the installs saved between runs.
constexpr const char *installsGroup = "Installs";

// Each platform has its own launcher for the engine's generate script in the root of the clone.
QString generateScriptName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("GenerateProjectFiles.bat");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("GenerateProjectFiles.command");
#else
    return QStringLiteral("GenerateProjectFiles.sh");
#endif
}

// Maps one of git's clone phases, at a percentage of its own, onto the clone step's bar. Receiving
// the objects is most of the download. The last phases check the files out, and for a Git LFS
// repository that is a large download of its own ("Filtering content").
double cloneStepFraction(const QString &phase, int percent)
{
    const double share = percent / 100.0;
    if (phase == QLatin1String("Receiving objects"))
        return 0.8 * share;
    if (phase == QLatin1String("Resolving deltas"))
        return 0.8 + 0.1 * share;

    return 0.9 + 0.1 * share;
}

// A failed install can be tried again, so it counts as installable.
bool isInstallable(InstallListModel::InstallState state)
{
    return state == InstallListModel::Available || state == InstallListModel::Failed;
}

// A running install is cloning, generating or building.
bool isRunning(InstallListModel::InstallState state)
{
    return state == InstallListModel::Cloning || state == InstallListModel::Generating
        || state == InstallListModel::Building;
}

// Only an install whose folder was not found can be pointed at another one. Other failed checks have no
// missing folder to locate.
bool isRelocatable(InstallListModel::InstallState state, bool pathMissing)
{
    return state == InstallListModel::Invalid && pathMissing;
}

// A missing folder can be cloned into only when its parent folder exists. Otherwise Locate folder is the only way
// to fix the install.
bool isFixable(InstallListModel::InstallState state, bool pathMissing, const QString &path)
{
    if (state != InstallListModel::Invalid)
        return false;

    return !pathMissing || QFileInfo(path).absoluteDir().exists();
}

// The path comes from the saved settings, so it is checked before anything is deleted. The folder must be
// absolute, and it must be neither a filesystem root nor the home folder.
bool isSafeToRemove(const QString &path)
{
    const QDir dir(path);
    if (!dir.isAbsolute() || dir.isRoot())
        return false;

    return dir.canonicalPath() != QDir::home().canonicalPath();
}

// Uninstall and Fix delete a folder on a worker thread, since a cloned engine has thousands of files. A folder
// that is already gone counts as removed.
bool removeFolder(const QString &path)
{
    if (!isSafeToRemove(path))
        return false;

    return !QFileInfo::exists(path) || QDir(path).removeRecursively();
}

using Failure = InstallValidator::Failure;

// A clone needs a folder that is missing or empty. A folder that is not a complete checkout is cleared first (see
// needsClear), so a clone is needed for either.
bool needsClone(const QList<Failure> &failures)
{
    return failures.contains(Failure::PathMissing) || failures.contains(Failure::NoGitCheckout);
}

// Clearing deletes the files in a folder that has some but is not a complete checkout, so Fix asks before it.
bool needsClear(const QList<Failure> &failures)
{
    return failures.contains(Failure::NoGitCheckout) && !failures.contains(Failure::PathMissing)
        && !failures.contains(Failure::FolderEmpty);
}

// Release tags are plain dotted numbers such as "1.10" or "1.8.6512.2".
bool isVersionTag(const QString &tag)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d+(\\.\\d+)*$"));
    return pattern.match(tag).hasMatch();
}

// Compares part by part as numbers, so "1.10" is newer than "1.9".
bool isNewerVersion(const QString &a, const QString &b)
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

} // namespace

InstallListModel::InstallListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadInstalls();
}

int InstallListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_installs.size();
}

QVariant InstallListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_installs.size())
        return {};

    const Install &install = m_installs.at(index.row());
    switch (role) {
    case VersionRole: return install.version;
    case ChannelRole: return install.channel;
    case StateRole: return static_cast<int>(install.state);
    case ProgressRole: return install.progress;
    case PathRole: return install.path;
    case IsDefaultRole: return install.isDefault;
    case LogRole: return install.log;
    case RecentRole: return index.row() < recentVersionCount;
    case StepRole: return static_cast<int>(install.step);
    case StepProgressRole: return install.stepProgress;
    case InstallableRole: return isInstallable(install.state);
    case ProblemRole: return install.problem;
    case RelocatableRole: return isRelocatable(install.state, install.pathMissing);
    case FixableRole: return isFixable(install.state, install.pathMissing, install.path);
    }

    return {};
}

QHash<int, QByteArray> InstallListModel::roleNames() const
{
    return {
        {VersionRole, "version"},
        {ChannelRole, "channel"},
        {StateRole, "state"},
        {ProgressRole, "progress"},
        {PathRole, "path"},
        {IsDefaultRole, "isDefault"},
        {LogRole, "log"},
        {RecentRole, "recent"},
        {StepRole, "step"},
        {StepProgressRole, "stepProgress"},
        {InstallableRole, "installable"},
        {ProblemRole, "problem"},
        {RelocatableRole, "relocatable"},
        {FixableRole, "fixable"},
    };
}

bool InstallListModel::isInstalled(const QString &version) const
{
    const int row = rowFor(version);
    return row >= 0 && m_installs.at(row).state == Installed;
}

bool InstallListModel::canInstall(const QString &version) const
{
    const int row = rowFor(version);
    return row >= 0 && isInstallable(m_installs.at(row).state);
}

QString InstallListModel::editorExecutable(const QString &version) const
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Installed)
        return {};

    return QDir(m_installs.at(row).path).filePath(InstallLayout::editorRelativePath());
}

QString InstallListModel::preferredVersion() const
{
    // The list is sorted newest first, so the first installed engine is the newest one.
    int newest = -1;
    for (int row = 0; row < m_installs.size(); ++row) {
        const Install &install = m_installs.at(row);
        if (install.state != Installed)
            continue;
        if (install.isDefault)
            return install.version;
        if (newest < 0)
            newest = row;
    }

    return newest >= 0 ? m_installs.at(newest).version : QString();
}

void InstallListModel::startInstall(const QString &version, const QString &path)
{
    const int row = rowFor(version);
    if (row < 0)
        return;

    Install &install = m_installs[row];
    if (!isInstallable(install.state))
        return;

    // A failed generate or build keeps its clone, so the retry runs that step again instead of cloning.
    // The install's path is already set, so the path argument is not needed.
    if (install.state == Failed && install.step == GenerateStep) {
        beginGenerate(version);
        return;
    }
    if (install.state == Failed && install.step == BuildStep) {
        beginBuild(version);
        return;
    }

    install.state = Cloning;
    install.progress = 0.0;
    install.step = CloneStep;
    install.stepProgress = 0.0;
    install.path = expandHome(path.isEmpty() ? "~/FlaxHub/Engines/" + version : path);
    install.log = "Cloning FlaxEngine " + version + "...\n";
    install.logLineIsProgress = false;
    emit dataChanged(index(row), index(row));
    updateBusy();

    cloneRepository(version, install.path);
}

void InstallListModel::uninstall(const QString &version)
{
    const int row = rowFor(version);
    if (row < 0)
        return;

    const InstallState state = m_installs.at(row).state;
    if (state == Invalid) {
        resetToAvailable(row);
        saveInstalls();
        return;
    }
    if (state != Installed && state != Failed)
        return;

    Install &install = m_installs[row];
    install.state = Removing;
    install.stepProgress = -1.0;
    emit dataChanged(index(row), index(row));

    // The worker reads only the path, so it does not touch the model.
    const QString path = install.path;
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, version]() {
        watcher->deleteLater();
        finishUninstall(version, watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        return removeFolder(path);
    }));
}

// The delete has finished. A removed folder takes its entry out of the Installs view, which is the same as
// the entry going back to Available. A folder that could not be deleted is Invalid, so the user can see
// the problem and remove the entry from the list.
void InstallListModel::finishUninstall(const QString &version, bool removed)
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Removing)
        return;

    if (removed) {
        resetToAvailable(row);
    } else {
        Install &install = m_installs[row];
        install.state = Invalid;
        install.problem = QStringLiteral("Could not delete the folder");
        emit dataChanged(index(row), index(row));
    }
    saveInstalls();
}

// Clears an install back to Available, as if it had never been installed. The version stays in the list,
// so it can be installed again.
void InstallListModel::resetToAvailable(int row)
{
    Install &install = m_installs[row];
    install.state = Available;
    install.progress = 0.0;
    install.step = CloneStep;
    install.stepProgress = 0.0;
    install.path.clear();
    install.isDefault = false;
    install.log.clear();
    install.problem.clear();
    install.pathMissing = false;
    emit dataChanged(index(row), index(row));
}

// The folder the user located is checked the same way as at startup. An install that passes is Installed
// again, and one that fails stays Invalid with the reason for the new folder.
void InstallListModel::relocate(const QString &version, const QString &path)
{
    const int row = rowFor(version);
    if (row < 0)
        return;

    Install &install = m_installs[row];
    if (!isRelocatable(install.state, install.pathMissing))
        return;

    const QList<Failure> failures = InstallValidator::validate(path, version);
    install.path = path;
    install.state = failures.isEmpty() ? Installed : Invalid;
    install.pathMissing = failures.contains(Failure::PathMissing);
    install.problem = InstallValidator::describe(failures);
    emit dataChanged(index(row), index(row));
    saveInstalls();
}

// Whether Fix would clear the folder. The check runs now, so the view can ask the user before fix deletes anything.
bool InstallListModel::fixClearsFolder(const QString &version) const
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Invalid)
        return false;

    const Install &install = m_installs.at(row);
    return needsClear(InstallValidator::validate(install.path, version));
}

// All four checks run again, since the folder may have changed since startup. Then only the steps that the failed
// checks need run, in order: clone, generate, build. The install becomes Installed only when finishInstall passes.
// A folder that has files but is not a complete checkout is cleared only when clearConfirmed is set.
bool InstallListModel::fix(const QString &version, bool clearConfirmed)
{
    const int row = rowFor(version);
    // Fix waits for any other install to finish, so two installs never write into their folders at once.
    if (row < 0 || m_installs.at(row).state != Invalid || m_busy)
        return false;

    Install &install = m_installs[row];
    const QList<Failure> failures = InstallValidator::validate(install.path, version);
    install.pathMissing = failures.contains(Failure::PathMissing);
    install.problem = InstallValidator::describe(failures);
    install.state = failures.isEmpty() ? Installed : Invalid;
    emit dataChanged(index(row), index(row));

    if (failures.isEmpty()) {
        saveInstalls();
        return false;
    }

    // Nothing is changed when the fix is not allowed, and the problem text above shows the current reasons.
    if (!isFixable(install.state, install.pathMissing, install.path) || (needsClear(failures) && !clearConfirmed))
        return false;

    // The log starts with the reasons, so the run shows what it is repairing.
    install.log = QStringLiteral("Fixing FlaxEngine %1...\n").arg(version) + install.problem + QLatin1Char('\n');
    install.logLineIsProgress = false;

    if (!needsClone(failures)) {
        if (failures.contains(Failure::NoGenerateOutput))
            beginGenerate(version);
        else
            beginBuild(version);
        return true;
    }

    const QString path = install.path;
    const bool clear = needsClear(failures);
    if (clear)
        install.log += QStringLiteral("Clearing the folder...\n");
    install.state = Cloning;
    install.step = CloneStep;
    install.progress = 0.0;
    // Clearing reports no fraction, so the bar is indeterminate until the clone reports one.
    install.stepProgress = clear ? -1.0 : 0.0;
    emit dataChanged(index(row), index(row));
    updateBusy();

    if (!clear) {
        cloneRepository(version, path);
        return true;
    }

    // Clearing runs on the worker thread, as uninstall does. The clone starts once the folder is gone.
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, version, path]() {
        watcher->deleteLater();
        if (watcher->result())
            cloneRepository(version, path);
        else
            failInstall(version, QStringLiteral("Could not clear the folder"));
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        return removeFolder(path);
    }));
    return true;
}

void InstallListModel::setDefault(const QString &version)
{
    const int target = rowFor(version);
    if (target < 0 || m_installs.at(target).state != Installed)
        return;

    bool changed = false;
    for (int row = 0; row < m_installs.size(); ++row) {
        Install &install = m_installs[row];
        const bool isDefault = row == target;
        if (install.isDefault != isDefault) {
            install.isDefault = isDefault;
            changed = true;
            emit dataChanged(index(row), index(row), {IsDefaultRole});
        }
    }

    if (changed)
        saveInstalls();
}

int InstallListModel::rowFor(const QString &version) const
{
    for (int row = 0; row < m_installs.size(); ++row) {
        if (m_installs.at(row).version == version)
            return row;
    }

    return -1;
}

// Restores the installs that the last run saved. Each folder is checked again, since it may have been
// moved or deleted. A folder that fails the check stays in the list as Invalid, so the user can see it.
void InstallListModel::loadInstalls()
{
    QSettings settings;
    const int count = settings.beginReadArray(installsGroup);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString version = settings.value("version").toString();
        const QString path = settings.value("path").toString();
        if (version.isEmpty() || path.isEmpty() || rowFor(version) >= 0)
            continue;

        Install install{version, "Stable", Installed, 1.0, path, settings.value("isDefault").toBool(), {}, BuildStep, -1.0};
        const QList<Failure> failures = InstallValidator::validate(path, version);
        if (!failures.isEmpty()) {
            install.state = Invalid;
            install.problem = InstallValidator::describe(failures);
            install.pathMissing = failures.contains(Failure::PathMissing);
        }
        m_installs.append(install);
    }
    settings.endArray();

    std::sort(m_installs.begin(), m_installs.end(), [](const Install &a, const Install &b) {
        return isNewerVersion(a.version, b.version);
    });
}

// Saves the installs that are on disk, or that were and may be again. Running and failed installs are
// left out, because they have no finished folder to return to. A removal in progress is kept, so a save
// during it does not drop the entry. If the app quits mid-delete, the entry is checked again on the next run.
void InstallListModel::saveInstalls() const
{
    QSettings settings;
    settings.remove(installsGroup);
    settings.beginWriteArray(installsGroup);
    int saved = 0;
    for (const Install &install : m_installs) {
        if (install.state != Installed && install.state != Invalid && install.state != Removing)
            continue;

        settings.setArrayIndex(saved++);
        settings.setValue("version", install.version);
        settings.setValue("path", install.path);
        settings.setValue("isDefault", install.isDefault);
    }
    settings.endArray();
}

// Clones the release tag into path. --depth 1 fetches only that tagged commit, not the history.
void InstallListModel::cloneRepository(const QString &version, const QString &path)
{
    const QStringList arguments = {"clone", "--progress", "--depth", "1", "--branch", version,
                                   "https://github.com/FlaxEngine/FlaxEngine.git", path};
    runStep(version, "git", arguments, {}, [this, version]() {
        finishClone(version);
    });
}

// Runs one step's program and adds its output to the install's log, which also updates the step's bar.
// A zero exit calls onSuccess. Any other exit, or a program that will not start, fails the install.
void InstallListModel::runStep(const QString &version, const QString &program, const QStringList &arguments,
                               const QString &workingDirectory, const std::function<void()> &onSuccess)
{
    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    // Without this, git waits on the terminal for credentials when it cannot read the repository.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process->setProcessEnvironment(environment);

    const QString name = QFileInfo(program).fileName();
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, version]() {
        appendOutput(version, process->readAllStandardOutput());
    });
    connect(process, &QProcess::finished, this, [this, process, version, name, onSuccess](int exitCode, QProcess::ExitStatus exitStatus) {
        appendOutput(version, process->readAllStandardOutput());
        process->deleteLater();

        if (exitStatus == QProcess::NormalExit && exitCode == 0)
            onSuccess();
        else
            failInstall(version, QStringLiteral("%1 exited with code %2").arg(name).arg(exitCode));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, version, name](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;

        failInstall(version, QStringLiteral("Could not start %1: ").arg(name) + process->errorString());
        process->deleteLater();
    });

    process->start(program, arguments);
}

// Adds a step's output to the log. Git redraws its progress line with a carriage return, so that line
// is replaced by whatever comes after it instead of becoming a new line.
void InstallListModel::appendOutput(const QString &version, const QByteArray &output)
{
    const int row = rowFor(version);
    if (row < 0)
        return;

    Install &install = m_installs[row];
    const QString text = QString::fromUtf8(output);
    for (const QChar c : text) {
        if (c == '\r') {
            install.logLineIsProgress = true;
            continue;
        }
        if (install.logLineIsProgress && c != '\n')
            install.log.truncate(install.log.lastIndexOf('\n') + 1);
        install.logLineIsProgress = false;
        install.log.append(c);
    }

    // Only git's clone reports a percentage for the step's bar. The generate script reports none.
    if (install.state == Cloning) {
        // The last progress line in this output is the most recent one.
        static const QRegularExpression progressPattern(
            QStringLiteral("(Receiving objects|Resolving deltas|Filtering content|Updating files):\\s+(\\d+)%"));
        QRegularExpressionMatchIterator matches = progressPattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            install.stepProgress = cloneStepFraction(match.captured(1), match.captured(2).toInt());
        }

        // The clone is the first of the three steps, so it fills the first third of the install's progress.
        install.progress = install.stepProgress / 3.0;
    }
    emit dataChanged(index(row), index(row));
}

// The clone is done, so the generate step starts.
void InstallListModel::finishClone(const QString &version)
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Cloning)
        return;

    beginGenerate(version);
}

// Runs the engine's generate script in the cloned folder. The log keeps what came before, so a retry shows
// the clone's output as well.
void InstallListModel::beginGenerate(const QString &version)
{
    const int row = rowFor(version);
    Install &install = m_installs[row];
    install.state = Generating;
    install.step = GenerateStep;
    install.progress = 1.0 / 3.0;
    install.stepProgress = -1.0;
    if (install.logLineIsProgress)
        install.log += '\n';
    install.logLineIsProgress = false;
    install.log += "Generating project files...\n";
    emit dataChanged(index(row), index(row));
    updateBusy();

    const QString script = QDir(install.path).filePath(generateScriptName());
    runStep(version, script, {}, install.path, [this, version]() {
        finishGenerate(version);
    });
}

// The generate script has finished. The build step starts unless the editor is already there, which only a
// Fix that regenerated the project files can find. A fresh clone never has it, so its build always runs.
void InstallListModel::finishGenerate(const QString &version)
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Generating)
        return;

    if (InstallValidator::validate(m_installs.at(row).path, version).contains(Failure::NoBinary))
        beginBuild(version);
    else
        finishInstall(version);
}

// Runs the engine's build tool for the editor in the Release configuration. The generate step only builds the
// C# bindings, so this is the step that compiles FlaxEditor. The tool reports no fraction, so the bar is
// indeterminate.
void InstallListModel::beginBuild(const QString &version)
{
    const int row = rowFor(version);
    Install &install = m_installs[row];
    install.state = Building;
    install.step = BuildStep;
    install.progress = 2.0 / 3.0;
    install.stepProgress = -1.0;
    if (install.logLineIsProgress)
        install.log += '\n';
    install.logLineIsProgress = false;
    install.log += "Building...\n";
    emit dataChanged(index(row), index(row));
    updateBusy();

    // Without -log the tool writes its output to a log file only, so there would be nothing for runStep to show.
    const QStringList arguments = {
        QStringLiteral("-build"),
        QStringLiteral("-log"),
        QStringLiteral("-platform=%1").arg(InstallLayout::buildPlatform()),
        QStringLiteral("-arch=%1").arg(InstallLayout::buildArchitecture()),
        QStringLiteral("-configuration=%1").arg(InstallLayout::buildConfiguration()),
        QStringLiteral("-buildtargets=FlaxEditor"),
    };
    const QString tool = QDir(install.path).filePath(InstallLayout::buildToolRelativePath());
    runStep(version, tool, arguments, install.path, [this, version]() {
        finishBuild(version);
    });
}

// The build tool exited with 0, so the install's checks run.
void InstallListModel::finishBuild(const QString &version)
{
    const int row = rowFor(version);
    if (row < 0 || m_installs.at(row).state != Building)
        return;

    finishInstall(version);
}

// The steps are done, so every check runs once more. The install is Installed only when they all pass, so an
// install is never kept without its editor. Otherwise it is Invalid with the reasons, and Fix can run again.
void InstallListModel::finishInstall(const QString &version)
{
    const int row = rowFor(version);
    if (row < 0)
        return;

    Install &install = m_installs[row];
    const QList<Failure> failures = InstallValidator::validate(install.path, version);
    install.pathMissing = failures.contains(Failure::PathMissing);
    install.problem = InstallValidator::describe(failures);
    install.progress = 1.0;
    if (install.logLineIsProgress)
        install.log += '\n';
    install.logLineIsProgress = false;
    if (failures.isEmpty()) {
        install.state = Installed;
        install.log += "Done.\n";
    } else {
        install.state = Invalid;
        install.log += "The install did not pass its checks:\n" + install.problem + '\n';
    }
    emit dataChanged(index(row), index(row));
    saveInstalls();
    updateBusy();
}

// Stops a running install on the step it is on. The log keeps the reason.
void InstallListModel::failInstall(const QString &version, const QString &message)
{
    const int row = rowFor(version);
    if (row < 0 || !isRunning(m_installs.at(row).state))
        return;

    Install &install = m_installs[row];
    install.state = Failed;
    if (install.logLineIsProgress)
        install.log += '\n';
    install.logLineIsProgress = false;
    install.log += message + '\n';
    emit dataChanged(index(row), index(row));
    updateBusy();
}

InstallListModel::VersionFetchState InstallListModel::versionFetchState() const
{
    return m_versionFetchState;
}

bool InstallListModel::busy() const
{
    return m_busy;
}

// Emits busyChanged only when the set of running installs goes from none to some, or back.
void InstallListModel::updateBusy()
{
    bool busy = false;
    for (const Install &install : m_installs)
        busy = busy || isRunning(install.state);

    if (m_busy == busy)
        return;

    m_busy = busy;
    emit busyChanged();
}

void InstallListModel::fetchAvailableVersions()
{
    if (m_versionFetchState == Fetching || m_versionFetchState == Fetched)
        return;

    setVersionFetchState(Fetching);
    requestTagPage(1, {});
}

void InstallListModel::setVersionFetchState(VersionFetchState state)
{
    if (m_versionFetchState == state)
        return;

    m_versionFetchState = state;
    emit versionFetchStateChanged();
}

// Tags are paged, so each page is requested in turn until a short one arrives.
void InstallListModel::requestTagPage(int page, const QStringList &tags)
{
    const QString url = QStringLiteral("https://api.github.com/repos/FlaxEngine/FlaxEngine/tags?per_page=%1&page=%2")
                            .arg(tagsPerPage)
                            .arg(page);
    QNetworkRequest request{QUrl(url)};
    // GitHub rejects API requests that have no User-Agent.
    request.setRawHeader("User-Agent", "FlaxHub");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, page, tags]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Could not fetch engine versions:" << reply->errorString();
            setVersionFetchState(FetchFailed);
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if (!document.isArray()) {
            qWarning() << "Could not read engine versions: GitHub did not return a tag list";
            setVersionFetchState(FetchFailed);
            return;
        }

        const QJsonArray pageTags = document.array();
        QStringList allTags = tags;
        for (const QJsonValue &tag : pageTags)
            allTags.append(tag.toObject().value("name").toString());

        if (pageTags.size() < tagsPerPage)
            finishVersionFetch(allTags);
        else
            requestTagPage(page + 1, allTags);
    });
}

void InstallListModel::finishVersionFetch(const QStringList &tags)
{
    QStringList versions;
    for (const QString &tag : tags) {
        if (isVersionTag(tag))
            versions.append(tag);
    }
    std::sort(versions.begin(), versions.end(), isNewerVersion);

    // Each version goes in at its place in the newest-first order, so the restored installs keep their
    // place among the fetched ones.
    bool inserted = false;
    for (const QString &version : versions) {
        if (rowFor(version) >= 0)
            continue;

        const auto position = std::find_if(m_installs.cbegin(), m_installs.cend(), [&version](const Install &install) {
            return isNewerVersion(version, install.version);
        });
        const int row = static_cast<int>(position - m_installs.cbegin());
        beginInsertRows(QModelIndex(), row, row);
        m_installs.insert(row, Install{version, "Stable", Available, 0.0, {}, false, {}, CloneStep, 0.0});
        endInsertRows();
        inserted = true;
    }

    // Inserting moves the rows below, so which rows are among the newest can change. The Add dialog
    // filters on that flag, so it has to be told.
    if (inserted)
        emit dataChanged(index(0), index(m_installs.size() - 1), {RecentRole});

    setVersionFetchState(Fetched);
}
