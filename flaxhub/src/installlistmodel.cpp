#include "installlistmodel.h"
#include "installlayout.h"
#include "installrules.h"
#include "installstore.h"
#include "installtable.h"
#include "pathutils.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {

// A missing folder can be cloned into only when its parent folder exists. Otherwise Locate folder is the only way
// to fix the install.
bool isFixable(const InstallTable::Install &install)
{
    if (install.state != InstallListModel::Invalid)
        return false;

    return !install.pathMissing || QFileInfo(install.path).absoluteDir().exists();
}

} // namespace

InstallListModel::InstallListModel(QObject *parent)
    : EngineRegistry(parent)
    , m_table(std::make_unique<InstallTable>())
    , m_ownedSettings(std::make_unique<QSettings>())
    , m_settings(m_ownedSettings.get())
{
    initialize();
}

InstallListModel::InstallListModel(QSettings *settings, QObject *parent)
    : EngineRegistry(parent)
    , m_table(std::make_unique<InstallTable>())
    , m_settings(settings)
{
    initialize();
}

InstallListModel::~InstallListModel() = default;

// Connects the catalog and the steps, then restores the saved installs and starts checking their folders.
void InstallListModel::initialize()
{
    connect(&m_catalog, &EngineCatalog::versionsFetched, this, &InstallListModel::finishVersionFetch);
    connect(&m_catalog, &EngineCatalog::fetchFailed, this, [this]() {
        setVersionFetchState(FetchFailed);
    });
    connect(&m_steps, &InstallSteps::logReceived, this, &InstallListModel::appendLog);
    connect(&m_steps, &InstallSteps::progressChanged, this, &InstallListModel::setStepProgress);
    connect(&m_steps, &InstallSteps::finished, this, &InstallListModel::finishStep);
    connect(&m_steps, &InstallSteps::failed, this, &InstallListModel::failInstall);

    loadInstalls();
}

int InstallListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_table->size();
}

QVariant InstallListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_table->size())
        return {};

    const InstallTable::Install &install = m_table->at(index.row());
    switch (role) {
    case VersionRole: return install.version;
    case ChannelRole: return install.channel;
    case StateRole: return static_cast<int>(install.state);
    case ProgressRole: return install.progress;
    case PathRole: return install.path;
    case IsDefaultRole: return install.isDefault;
    case LogRole: return install.log;
    case RecentRole: return InstallTable::isRecent(index.row());
    case StepRole: return static_cast<int>(install.step);
    case StepProgressRole: return install.stepProgress;
    case InstallableRole: return m_table->canInstall(index.row());
    case ProblemRole: return install.problem;
    case RelocatableRole: return InstallTable::isRelocatable(install);
    case FixableRole: return isFixable(install) && InstallTable::isRecent(index.row());
    case AddableRole: return m_table->canAdd(index.row());
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
        {AddableRole, "addable"},
    };
}

bool InstallListModel::isInstalled(const QString &version) const
{
    return m_table->isInstalled(version);
}

QStringList InstallListModel::installedVersions() const
{
    return m_table->installedVersions();
}

bool InstallListModel::canInstall(const QString &version) const
{
    const int row = m_table->rowFor(version);
    return row >= 0 && m_table->canAdd(row);
}

QString InstallListModel::editorExecutable(const QString &version) const
{
    if (!m_table->isInstalled(version))
        return {};

    return QDir(m_table->at(m_table->rowFor(version)).path).filePath(InstallLayout::editorRelativePath());
}

QString InstallListModel::preferredVersion() const
{
    return m_table->preferredVersion();
}

bool InstallListModel::canInstallTo(const QString &path) const
{
    const QString folder = expandHome(path);
    return InstallRules::isFullPath(folder) && InstallRules::isFreeFolder(folder);
}

QString InstallListModel::installPathFor(const QString &version, const QString &destination) const
{
    QString folder = destination.trimmed();
    while (folder.endsWith(QLatin1Char('/')))
        folder.chop(1);
    if (folder.isEmpty())
        return {};

    return folder + QLatin1Char('/') + version;
}

bool InstallListModel::startInstall(const QString &version, const QString &path)
{
    // An install waits for any running install to finish, so two never write into their folders at once.
    const QString title = qtTrId("main.installlistmodel.install_title");
    if (m_busy) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.install_busy").arg(version));
        return false;
    }

    // A path the user gave must be a full path. The retry below ignores the path, so it is checked here as well.
    if (!path.isEmpty() && !InstallRules::isFullPath(expandHome(path))) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.path_not_full").arg(path));
        return false;
    }

    const int row = m_table->rowFor(version);
    if (row < 0) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.install_unknown").arg(version));
        return false;
    }

    const InstallTable::Install &install = m_table->at(row);
    if (!InstallTable::isInstallable(install.state)) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.install_not_idle").arg(version));
        return false;
    }
    if (!InstallTable::isRecent(row)) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.install_too_old")
                                      .arg(InstallTable::recentVersionCount)
                                      .arg(version));
        return false;
    }

    // A failed generate or build keeps its clone, so the retry runs that step again instead of cloning.
    // The install's path is already set, so the path argument is not needed.
    if (install.state == Failed && install.step == GenerateStep) {
        beginGenerate(version);
        return true;
    }
    if (install.state == Failed && install.step == BuildStep) {
        beginBuild(version);
        return true;
    }

    const QString target = expandHome(path.isEmpty() ? installPathFor(version, InstallLayout::defaultInstallDirectory()) : path);
    if (!InstallRules::isFullPath(target)) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.path_not_full").arg(target));
        return false;
    }
    if (!InstallRules::isFreeFolder(target)) {
        emit errorOccurred(title, qtTrId("main.installlistmodel.install_folder_not_empty").arg(collapseHome(target)));
        return false;
    }

    m_table->startClone(row, target);
    notifyStateChanged(row);

    // The folder is missing or empty here (see canInstallTo and needsClear).
    m_steps.clone(version, target);
    return true;
}

void InstallListModel::uninstall(const QString &version)
{
    const int row = m_table->rowFor(version);
    if (row < 0)
        return;

    const InstallState state = m_table->at(row).state;
    if (state != Installed && state != Failed && state != Invalid)
        return;

    m_table->setRemoving(row);
    notifyStateChanged(row);

    const QString path = m_table->at(row).path;
    const bool wasInvalid = state == Invalid;
    m_jobs.removeInstall(version, path, [this, version, path, wasInvalid](const QString &problem) {
        finishUninstall(version, path, problem, wasInvalid);
    });
}

// The delete has finished. A removed folder takes its entry out of the Installs view, which is the same as
// the entry going back to Available. A folder that was kept is Invalid with the reason, so the user can see
// the problem and try again. An entry that was already Invalid is dropped even when its folder was kept, since the
// user asked to remove it from the list, and the notice says the folder stays on disk.
void InstallListModel::finishUninstall(const QString &version, const QString &path, const QString &problem,
                                       bool wasInvalid)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || m_table->at(row).state != Removing)
        return;

    if (problem.isEmpty()) {
        m_table->resetToAvailable(row);
        notifyStateChanged(row);
    } else if (wasInvalid) {
        m_table->resetToAvailable(row);
        notifyStateChanged(row);
        emit errorOccurred(qtTrId("main.installlistmodel.removed_title"),
                           qtTrId("main.installlistmodel.removed")
                               .arg(version, collapseHome(path), problem));
    } else {
        m_table->setKept(row, problem);
        notifyStateChanged(row);
    }
}

// Only a folder that FlaxHub created can be located: it must hold the marker that the clone wrote. Any other folder is
// refused before a check runs, and the install keeps its old folder. The folder that passes is checked the same way as
// at startup, on a worker thread. An install that passes is Installed again, and one that fails stays Invalid with the
// reason for the new folder. A marked folder that is not a checkout of this engine is refused too, since Fix would clear it.
void InstallListModel::relocate(const QString &version, const QString &path)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || !InstallTable::isRelocatable(m_table->at(row)))
        return;

    const QString folder = expandHome(path);
    if (!InstallRules::isFullPath(folder)) {
        emit errorOccurred(qtTrId("main.installlistmodel.locate_title"),
                           qtTrId("main.installlistmodel.path_not_full").arg(path));
        return;
    }

    if (!InstallRules::hasMarker(folder)) {
        emit errorOccurred(qtTrId("main.installlistmodel.locate_title"),
                           qtTrId("main.installlistmodel.locate_not_created_by_hub")
                               .arg(version)
                               .arg(collapseHome(folder)));
        return;
    }

    m_table->setChecking(row);
    notifyStateChanged(row);

    m_jobs.check(version, folder, [this, version, folder](const QList<Failure> &failures) {
        const int checked = m_table->rowFor(version);
        if (checked < 0)
            return;

        if (InstallRules::needsClear(failures)) {
            m_table->refuseLocation(checked, failures);
            notifyStateChanged(checked);
            emit errorOccurred(qtTrId("main.installlistmodel.locate_title"),
                               qtTrId("main.installlistmodel.locate_incomplete")
                                   .arg(version));
            return;
        }

        m_table->relocate(checked, folder, failures);
        notifyStateChanged(checked);
    });
}

// All four checks run again, on a worker thread, since the folder may have changed since it was last checked. The
// install is Checking until decideFix has the result.
void InstallListModel::fix(const QString &version, bool clearConfirmed)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || m_table->at(row).state != Invalid)
        return;

    // A Fix may clone and build, which installs the engine, so it is held to the same rule as an install.
    if (!InstallTable::isRecent(row)) {
        emit errorOccurred(qtTrId("main.installlistmodel.fix_title"),
                           qtTrId("main.installlistmodel.fix_too_old")
                               .arg(InstallTable::recentVersionCount)
                               .arg(version));
        return;
    }

    // Fix waits for any other install to finish, so two installs never write into their folders at once.
    if (m_busy) {
        emit errorOccurred(qtTrId("main.installlistmodel.fix_title"),
                           qtTrId("main.installlistmodel.fix_busy").arg(version));
        return;
    }

    m_table->setChecking(row);
    notifyStateChanged(row);

    m_jobs.check(version, m_table->at(row).path, [this, version, clearConfirmed](const QList<Failure> &failures) {
        decideFix(version, failures, clearConfirmed);
    });
}

// Decides what a Fix does with the result of its check. Only the steps that the failed checks need run, in order:
// clone, generate, build. The install becomes Installed only when finishInstall passes. A folder that has files but is
// not a complete checkout is cleared only when clearConfirmed is set.
void InstallListModel::decideFix(const QString &version, const QList<Failure> &failures, bool clearConfirmed)
{
    const int row = m_table->rowFor(version);
    if (row < 0)
        return;

    m_table->applyCheckResult(row, failures);
    const InstallTable::Install &install = m_table->at(row);
    if (failures.isEmpty()) {
        notifyStateChanged(row);
        return;
    }

    // Another install may have started while the check ran. Its steps write into their folders, so this one waits.
    if (m_busy) {
        notifyStateChanged(row);
        emit errorOccurred(qtTrId("main.installlistmodel.fix_title"),
                           qtTrId("main.installlistmodel.fix_busy").arg(version));
        return;
    }

    // Git could not run, and no step repairs that. The install stays as it is, and the problem text says why.
    if (failures.contains(Failure::GitUnavailable)) {
        notifyStateChanged(row);
        emit errorOccurred(qtTrId("main.installlistmodel.fix_title"),
                           qtTrId("main.installlistmodel.fix_no_git")
                               .arg(version));
        return;
    }

    // Nothing is changed when the fix is not allowed, and the problem text shows the current reasons.
    if (!isFixable(install)) {
        notifyStateChanged(row);
        emit errorOccurred(qtTrId("main.installlistmodel.fix_title"),
                           qtTrId("main.installlistmodel.fix_no_parent")
                               .arg(collapseHome(install.path)));
        return;
    }

    // The view asks the user, then calls fix again with the clear confirmed.
    if (InstallRules::needsClear(failures) && !clearConfirmed) {
        notifyStateChanged(row);
        emit clearConfirmationRequired(version, install.path);
        return;
    }

    m_table->startFix(row);

    if (!InstallRules::needsClone(failures)) {
        if (failures.contains(Failure::NoGenerateOutput))
            beginGenerate(version);
        else
            beginBuild(version);
        emit fixStarted(version, m_table->at(row).path);
        return;
    }

    const QString path = install.path;
    const bool clear = InstallRules::needsClear(failures);
    m_table->startFixClone(row, clear);
    notifyStateChanged(row);
    emit fixStarted(version, path);

    if (!clear) {
        m_steps.clone(version, path);
        return;
    }

    // The clone starts once the folder is gone.
    m_jobs.clear(path, [this, version, path](bool cleared) {
        if (cleared)
            m_steps.clone(version, path);
        else
            failInstall(version, QStringLiteral("Could not clear the folder"));
    });
}

void InstallListModel::setDefault(const QString &version)
{
    const int target = m_table->rowFor(version);
    if (target < 0 || m_table->at(target).state != Installed)
        return;

    const QList<int> changed = m_table->setDefault(target);
    for (const int row : changed)
        emit dataChanged(index(row), index(row), {IsDefaultRole});

    if (!changed.isEmpty())
        saveInstalls();
}

// Restores the installs that the last run saved. Each folder is checked again, since it may have been moved or
// deleted. An install that was running when the app quit is checked the same way, since its folder may be partly
// written, and a folder that fails the check stays in the list as Invalid, so the user can see it. Until its check
// returns, an install is Checking.
void InstallListModel::loadInstalls()
{
    for (const InstallStore::SavedInstall &saved : InstallStore::load(*m_settings))
        m_table->addSaved(saved.version, saved.path, saved.isDefault);
    m_table->sortNewestFirst();

    for (const InstallTable::Install &install : m_table->installs()) {
        m_jobs.check(install.version, install.path, [this, version = install.version](const QList<Failure> &failures) {
            const int row = m_table->rowFor(version);
            if (row < 0)
                return;

            m_table->applyCheckResult(row, failures);
            notifyStateChanged(row);
        });
    }
}

// A running install, a failed one, a removal and a check are saved too, so a folder the Hub may have written into is
// never left without its entry. Each state change saves (see notifyStateChanged), so the entry is on disk before a
// step starts. If the app quits mid-step, mid-delete or mid-check, the entry is checked again on the next run.
void InstallListModel::saveInstalls()
{
    const bool saved = InstallStore::save(*m_settings, m_table->installs());

    // Each state change saves, so the notice is shown once per failure and not on every change after it.
    const bool failed = !saved;
    if (failed && !m_saveFailed) {
        emit errorOccurred(qtTrId("main.installlistmodel.save_title"),
                           qtTrId("main.installlistmodel.save_failed"));
    }
    m_saveFailed = failed;
}

// Saves the installs, tells the views that a row's state changed, and keeps busy() in step. Log and progress lines do
// not come through here, so they are not taken for a change of state.
void InstallListModel::notifyStateChanged(int row)
{
    saveInstalls();
    emit dataChanged(index(row), index(row));
    updateBusy();
    emit installStateChanged();
    emit installedEnginesChanged();
}

// Emits busyChanged only when the set of running installs goes from none to some, or back.
void InstallListModel::updateBusy()
{
    const bool running = m_table->anyRunning();
    if (m_busy == running)
        return;

    m_busy = running;
    emit busyChanged();
}

void InstallListModel::appendLog(const QString &version, const QString &text)
{
    const int row = m_table->rowFor(version);
    if (row < 0)
        return;

    m_table->appendOutput(row, text);
    emit dataChanged(index(row), index(row), {LogRole});
}

// Sets the clone step's bar from git's progress. Only the clone reports a fraction, so other steps ignore it.
void InstallListModel::setStepProgress(const QString &version, double fraction)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || !m_table->setCloneProgress(row, fraction))
        return;

    emit dataChanged(index(row), index(row), {ProgressRole, StepProgressRole});
}

// The step exited with 0, so the install moves on from the step that ran.
void InstallListModel::finishStep(const QString &version)
{
    const int row = m_table->rowFor(version);
    if (row < 0)
        return;

    switch (m_table->at(row).state) {
    case Cloning: finishClone(version); break;
    case Generating: finishGenerate(version); break;
    // The build tool exited with 0, so the install's checks run.
    case Building: finishInstall(version); break;
    default: break;
    }
}

// The clone is done, so the marker goes into the folder and the generate step starts. The marker waits for the clone,
// since git refuses a folder that already holds files.
void InstallListModel::finishClone(const QString &version)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || m_table->at(row).state != Cloning)
        return;

    const QString path = m_table->at(row).path;
    if (!InstallRules::writeMarker(path)) {
        failInstall(version, QStringLiteral("Could not write the install marker in %1").arg(path));
        return;
    }

    beginGenerate(version);
}

void InstallListModel::beginGenerate(const QString &version)
{
    const int row = m_table->rowFor(version);
    m_table->startGenerate(row);
    notifyStateChanged(row);

    m_steps.generate(version, m_table->at(row).path);
}

// The generate script has finished. The build step starts unless the editor is already there, which only a
// Fix that regenerated the project files can find. A fresh clone never has it, so its build always runs.
void InstallListModel::finishGenerate(const QString &version)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || m_table->at(row).state != Generating)
        return;

    if (!InstallValidator::hasReleaseEditor(m_table->at(row).path))
        beginBuild(version);
    else
        finishInstall(version);
}

void InstallListModel::beginBuild(const QString &version)
{
    const int row = m_table->rowFor(version);
    m_table->startBuild(row);
    notifyStateChanged(row);

    m_steps.build(version, m_table->at(row).path);
}

// The steps are done, so the folder is checked once more on a worker thread. The install is Installed only when every
// check passes, so an install is never kept without its editor. Otherwise it is Invalid with the reasons, and Fix can
// run again.
void InstallListModel::finishInstall(const QString &version)
{
    const int row = m_table->rowFor(version);
    if (row < 0)
        return;

    m_table->finishSteps(row);
    notifyStateChanged(row);

    m_jobs.check(version, m_table->at(row).path, [this, version](const QList<Failure> &failures) {
        const int checked = m_table->rowFor(version);
        if (checked < 0)
            return;

        m_table->applyInstallCheck(checked, failures);
        notifyStateChanged(checked);
    });
}

// Stops a running install on the step it is on. The log keeps the reason.
void InstallListModel::failInstall(const QString &version, const QString &message)
{
    const int row = m_table->rowFor(version);
    if (row < 0 || !m_table->fail(row, message))
        return;

    notifyStateChanged(row);
}

InstallListModel::VersionFetchState InstallListModel::versionFetchState() const
{
    return m_versionFetchState;
}

bool InstallListModel::busy() const
{
    return m_busy;
}

bool InstallListModel::removing() const
{
    return m_table->anyRemoving();
}

bool InstallListModel::checking() const
{
    return m_jobs.checking();
}

void InstallListModel::fetchAvailableVersions()
{
    if (m_versionFetchState == Fetching || m_versionFetchState == Fetched)
        return;

    setVersionFetchState(Fetching);
    m_catalog.fetch();
}

void InstallListModel::setVersionFetchState(VersionFetchState state)
{
    if (m_versionFetchState == state)
        return;

    m_versionFetchState = state;
    emit versionFetchStateChanged();
}

// The catalog has the newest-first version tags. Each one goes in at its place in that order, so the restored installs
// keep their place among the fetched ones.
void InstallListModel::finishVersionFetch(const QStringList &versions)
{
    bool inserted = false;
    for (const QString &version : versions) {
        const int row = m_table->availableRow(version);
        if (row < 0)
            continue;

        beginInsertRows(QModelIndex(), row, row);
        m_table->insertAvailable(row, version);
        endInsertRows();
        inserted = true;
    }

    // Inserting moves the rows below, so which rows are among the newest can change, and with them which can be
    // installed or fixed. The Add dialog and the views filter on those roles, so they have to be told.
    if (inserted)
        emit dataChanged(index(0), index(m_table->size() - 1), {RecentRole, InstallableRole, FixableRole});

    setVersionFetchState(Fetched);
}
