#include "installtable.h"

#include "versionutils.h"

#include <algorithm>

namespace {

// The most log text an install keeps. When it grows past this, the oldest lines go and the newest stay.
constexpr qsizetype maxLogSize = 256 * 1024;

} // namespace

bool InstallTable::isInstallable(InstallState state)
{
    return state == InstallListModel::Available || state == InstallListModel::Failed;
}

bool InstallTable::isRunning(InstallState state)
{
    return state == InstallListModel::Cloning || state == InstallListModel::Generating
        || state == InstallListModel::Building;
}

bool InstallTable::isRelocatable(const Install &install)
{
    return install.state == InstallListModel::Invalid && install.pathMissing;
}

bool InstallTable::isRecent(int row)
{
    return row < recentVersionCount;
}

bool InstallTable::canInstall(int row) const
{
    return isInstallable(m_installs.at(row).state) && isRecent(row);
}

bool InstallTable::canAdd(int row) const
{
    return m_installs.at(row).state == InstallListModel::Available && isRecent(row);
}

int InstallTable::size() const
{
    return m_installs.size();
}

const InstallTable::Install &InstallTable::at(int row) const
{
    return m_installs.at(row);
}

const QList<InstallTable::Install> &InstallTable::installs() const
{
    return m_installs;
}

int InstallTable::rowFor(const QString &version) const
{
    for (int row = 0; row < m_installs.size(); ++row) {
        if (m_installs.at(row).version == version)
            return row;
    }

    return -1;
}

bool InstallTable::isInstalled(const QString &version) const
{
    const int row = rowFor(version);
    return row >= 0 && m_installs.at(row).state == InstallListModel::Installed;
}

QStringList InstallTable::installedVersions() const
{
    QStringList versions;
    for (const Install &install : m_installs) {
        if (install.state == InstallListModel::Installed)
            versions.append(install.version);
    }

    return versions;
}

QString InstallTable::preferredVersion() const
{
    // The list is sorted newest first, so the first installed engine is the newest one.
    int newest = -1;
    for (int row = 0; row < m_installs.size(); ++row) {
        const Install &install = m_installs.at(row);
        if (install.state != InstallListModel::Installed)
            continue;
        if (install.isDefault)
            return install.version;
        if (newest < 0)
            newest = row;
    }

    return newest >= 0 ? m_installs.at(newest).version : QString();
}

bool InstallTable::anyRunning() const
{
    return std::any_of(m_installs.cbegin(), m_installs.cend(), [](const Install &install) {
        return isRunning(install.state);
    });
}

bool InstallTable::anyRemoving() const
{
    return std::any_of(m_installs.cbegin(), m_installs.cend(), [](const Install &install) {
        return install.state == InstallListModel::Removing;
    });
}

bool InstallTable::addSaved(const QString &version, const QString &path, bool isDefault)
{
    if (version.isEmpty() || path.isEmpty() || rowFor(version) >= 0)
        return false;

    Install install;
    install.version = version;
    install.channel = "Stable";
    install.state = InstallListModel::Checking;
    install.progress = 1.0;
    install.path = path;
    install.isDefault = isDefault;
    install.step = InstallListModel::BuildStep;
    install.stepProgress = -1.0;
    m_installs.append(install);
    return true;
}

void InstallTable::sortNewestFirst()
{
    std::sort(m_installs.begin(), m_installs.end(), [](const Install &a, const Install &b) {
        return isNewerVersion(a.version, b.version);
    });
}

int InstallTable::availableRow(const QString &version) const
{
    if (rowFor(version) >= 0)
        return -1;

    const auto position = std::find_if(m_installs.cbegin(), m_installs.cend(), [&version](const Install &install) {
        return isNewerVersion(version, install.version);
    });
    return static_cast<int>(position - m_installs.cbegin());
}

void InstallTable::insertAvailable(int row, const QString &version)
{
    Install install;
    install.version = version;
    install.channel = "Stable";
    install.state = InstallListModel::Available;
    install.progress = 0.0;
    install.isDefault = false;
    install.step = InstallListModel::CloneStep;
    install.stepProgress = 0.0;
    m_installs.insert(row, install);
}

void InstallTable::startClone(int row, const QString &path)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Cloning;
    install.progress = 0.0;
    install.step = InstallListModel::CloneStep;
    install.stepProgress = 0.0;
    install.path = path;
    install.log = "Cloning FlaxEngine " + install.version + "...\n";
    install.logLineIsProgress = false;
}

void InstallTable::startFix(int row)
{
    Install &install = m_installs[row];
    install.log = QStringLiteral("Fixing FlaxEngine %1...\n").arg(install.version) + install.problem + QLatin1Char('\n');
    install.logLineIsProgress = false;
}

void InstallTable::startFixClone(int row, bool clear)
{
    Install &install = m_installs[row];
    if (clear)
        install.log += QStringLiteral("Clearing the folder...\n");
    install.state = InstallListModel::Cloning;
    install.step = InstallListModel::CloneStep;
    install.progress = 0.0;
    install.stepProgress = clear ? -1.0 : 0.0;
}

// The log keeps what came before, so a retry shows the clone's output as well.
void InstallTable::startGenerate(int row)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Generating;
    install.step = InstallListModel::GenerateStep;
    install.progress = 1.0 / 3.0;
    install.stepProgress = -1.0;
    endLogLine(install);
    install.log += "Generating project files...\n";
}

// The build tool reports no fraction, so the bar is indeterminate.
void InstallTable::startBuild(int row)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Building;
    install.step = InstallListModel::BuildStep;
    install.progress = 2.0 / 3.0;
    install.stepProgress = -1.0;
    endLogLine(install);
    install.log += "Building...\n";
}

void InstallTable::finishSteps(int row)
{
    Install &install = m_installs[row];
    install.progress = 1.0;
    endLogLine(install);
    install.state = InstallListModel::Checking;
}

bool InstallTable::fail(int row, const QString &message)
{
    Install &install = m_installs[row];
    if (!isRunning(install.state))
        return false;

    install.state = InstallListModel::Failed;
    endLogLine(install);
    install.log += message + '\n';
    return true;
}

void InstallTable::setChecking(int row)
{
    m_installs[row].state = InstallListModel::Checking;
}

void InstallTable::setRemoving(int row)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Removing;
    install.stepProgress = -1.0;
}

void InstallTable::setKept(int row, const QString &problem)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Invalid;
    install.problem = problem;
}

void InstallTable::restoreInvalid(int row)
{
    m_installs[row].state = InstallListModel::Invalid;
}

void InstallTable::resetToAvailable(int row)
{
    Install &install = m_installs[row];
    install.state = InstallListModel::Available;
    install.progress = 0.0;
    install.step = InstallListModel::CloneStep;
    install.stepProgress = 0.0;
    install.path.clear();
    install.isDefault = false;
    install.log.clear();
    install.problem.clear();
    install.pathMissing = false;
}

void InstallTable::applyCheckResult(int row, const QList<Failure> &failures)
{
    Install &install = m_installs[row];
    install.state = failures.isEmpty() ? InstallListModel::Installed : InstallListModel::Invalid;
    install.pathMissing = failures.contains(Failure::PathMissing);
    install.problem = InstallValidator::describe(failures);
}

void InstallTable::applyInstallCheck(int row, const QList<Failure> &failures)
{
    applyCheckResult(row, failures);
    Install &install = m_installs[row];
    if (failures.isEmpty())
        install.log += "Done.\n";
    else
        install.log += "The install did not pass its checks:\n" + install.problem + '\n';
}

void InstallTable::relocate(int row, const QString &path, const QList<Failure> &failures)
{
    m_installs[row].path = path;
    applyCheckResult(row, failures);
}

void InstallTable::refuseLocation(int row, const QList<Failure> &failures)
{
    applyCheckResult(row, failures);
    m_installs[row].state = InstallListModel::Invalid;
}

// Git redraws its progress line with a carriage return, so that line is replaced by whatever comes after it instead
// of becoming a new line.
void InstallTable::appendOutput(int row, const QString &text)
{
    Install &install = m_installs[row];
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

    if (install.log.size() > maxLogSize) {
        // Cut to three quarters of the cap, at a line start, so the next chunks do not cut again at once.
        const qsizetype excess = install.log.size() - maxLogSize * 3 / 4;
        const qsizetype lineEnd = install.log.indexOf('\n', excess);
        install.log.remove(0, lineEnd >= 0 ? lineEnd + 1 : excess);
    }
}

bool InstallTable::setCloneProgress(int row, double fraction)
{
    Install &install = m_installs[row];
    if (install.state != InstallListModel::Cloning)
        return false;

    install.stepProgress = fraction;
    // The clone is the first of the three steps, so it fills the first third of the install's progress.
    install.progress = fraction / 3.0;
    return true;
}

QList<int> InstallTable::setDefault(int target)
{
    QList<int> changed;
    for (int row = 0; row < m_installs.size(); ++row) {
        Install &install = m_installs[row];
        const bool isDefault = row == target;
        if (install.isDefault != isDefault) {
            install.isDefault = isDefault;
            changed.append(row);
        }
    }

    return changed;
}

void InstallTable::endLogLine(Install &install)
{
    if (install.logLineIsProgress)
        install.log += '\n';
    install.logLineIsProgress = false;
}
