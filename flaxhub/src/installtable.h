#ifndef INSTALLTABLE_H
#define INSTALLTABLE_H

#include "installlistmodel.h"
#include "installvalidator.h"

#include <QList>
#include <QString>
#include <QStringList>

// The rows of the install list and the changes of state an install goes through, with the log text and the progress
// each change sets. It reads no files, runs nothing and emits nothing: InstallListModel does the work, applies the
// result here, and tells the views which row changed.
class InstallTable
{
public:
    using InstallState = InstallListModel::InstallState;
    using InstallStep = InstallListModel::InstallStep;
    using Failure = InstallValidator::Failure;

    struct Install {
        QString version;
        QString channel;
        InstallState state = InstallListModel::Available;
        double progress = 0.0;
        QString path;
        bool isDefault = false;
        QString log;
        // stepProgress is 0..1 for a step that reports a fraction, and -1 for one that does not.
        InstallStep step = InstallListModel::CloneStep;
        double stepProgress = 0.0;
        // The last line of log is a progress line that the next output replaces.
        bool logLineIsProgress = false;
        // The reasons an Invalid install failed its checks, one per line. Empty for every other state.
        QString problem;
        // Set when an Invalid install's folder was not found. Only that case can be located again.
        bool pathMissing = false;
    };

    // Only the newest versions can be installed or fixed. The list is newest first, so these are its first rows.
    static constexpr int recentVersionCount = 4;

    // A failed install can be tried again, so it counts as installable.
    static bool isInstallable(InstallState state);
    // A running install is cloning, generating or building.
    static bool isRunning(InstallState state);
    // Only an install whose folder was not found can be pointed at another one. Other failed checks have no missing
    // folder to locate.
    static bool isRelocatable(const Install &install);
    // Whether the row is among the newest versions. Only those can be installed or fixed.
    static bool isRecent(int row);

    int size() const;
    const Install &at(int row) const;
    const QList<Install> &installs() const;
    // The row of version, or -1 when it is not listed.
    int rowFor(const QString &version) const;
    // Whether the row can be installed now: it is installable, and it is among the newest versions.
    bool canInstall(int row) const;
    // Whether the row can be picked as a new install: it is Available, and it is among the newest versions. A failed
    // install is not picked, since its Retry in the Installs view is how it is installed again.
    bool canAdd(int row) const;

    bool isInstalled(const QString &version) const;
    QStringList installedVersions() const;
    QString preferredVersion() const;
    bool anyRunning() const;
    bool anyRemoving() const;

    // Adds an install that the last run saved, as Checking until its folder is checked. A version that is already
    // listed, or an entry without a version or a path, is skipped. Returns whether it was added.
    bool addSaved(const QString &version, const QString &path, bool isDefault);
    void sortNewestFirst();
    // The row a fetched version goes in, so the list stays newest first. -1 when the version is already listed.
    int availableRow(const QString &version) const;
    void insertAvailable(int row, const QString &version);

    void startClone(int row, const QString &path);
    // A Fix's log starts with the reasons, so the run shows what it is repairing.
    void startFix(int row);
    // A Fix that clones. Clearing reports no fraction, so the bar is indeterminate until the clone reports one.
    void startFixClone(int row, bool clear);
    void startGenerate(int row);
    void startBuild(int row);
    // The steps are done and the folder is about to be checked.
    void finishSteps(int row);
    // Returns false, and changes nothing, unless the install is running.
    bool fail(int row, const QString &message);

    void setChecking(int row);
    void setRemoving(int row);
    // The folder was kept on uninstall, so the install is Invalid with the reason.
    void setKept(int row, const QString &problem);
    // An Invalid install whose removal was refused is Invalid again, with the problem it had before.
    void restoreInvalid(int row);
    // Clears an install back to Available, as if it had never been installed. The version stays in the list, so it
    // can be installed again.
    void resetToAvailable(int row);

    // Records what a check found: whether the install passes, the reasons it does not and whether its folder is missing.
    void applyCheckResult(int row, const QList<Failure> &failures);
    // The check after the last step, which also ends the log with the outcome.
    void applyInstallCheck(int row, const QList<Failure> &failures);
    // A located folder that passed the marker check: the install moves there and takes the check's result.
    void relocate(int row, const QString &path, const QList<Failure> &failures);
    // A located folder that needs clearing is refused: the install keeps its folder and is Invalid with the reasons.
    void refuseLocation(int row, const QList<Failure> &failures);

    // Adds a step's output to the log, and keeps the log under its cap.
    void appendOutput(int row, const QString &text);
    // Only the clone reports a fraction. Returns false, and changes nothing, unless the install is cloning.
    bool setCloneProgress(int row, double fraction);
    // Makes target the only default install. Returns the rows whose flag changed.
    QList<int> setDefault(int target);

private:
    // Ends a progress line, so the next text starts on a line of its own.
    static void endLogLine(Install &install);

    QList<Install> m_installs;
};

#endif
