#ifndef INSTALLFOLDERJOBS_H
#define INSTALLFOLDERJOBS_H

#include "installvalidator.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

// The folder work an install needs that can take a while: the checks, which run git, and the deletes, since a cloned
// engine has thousands of files. Each job runs on a worker thread and reads only the path and the version it is given,
// so it does not touch the model. Its callback runs on the GUI thread once the job is done.
class InstallFolderJobs : public QObject
{
    Q_OBJECT

public:
    using Failure = InstallValidator::Failure;

    // What a delete of an install's folder did. Only Removed leaves no folder behind. NotOwned and UnsafePath are
    // decided before anything is deleted, so the folder is untouched. DeleteFailed can stop part way, so some files may
    // be gone.
    enum class RemoveResult {
        // The folder is gone, or was never there.
        Removed,
        // An Installed or Failed folder that the Hub did not make and that is not a complete checkout.
        NotOwned,
        // InstallRules::isSafeToRemove refused the path.
        UnsafePath,
        // The delete failed part way.
        DeleteFailed
    };

    explicit InstallFolderJobs(QObject *parent = nullptr);

    // Runs InstallValidator on the folder. onChecked gets the failed checks. checking() is true until then.
    void check(const QString &version, const QString &path, const std::function<void(const QList<Failure> &)> &onChecked);

    // Deletes an install's folder, and its marker with it. An Installed or Failed entry is deleted only when
    // InstallRules::mayUninstall allows it. An Invalid entry (invalidEntry) was listed by FlaxHub, so its folder is
    // deleted whatever it holds, and only InstallRules::isSafeToRemove applies. A path that is already gone counts as
    // removed. onRemoved gets the result once the job is done.
    void removeInstall(const QString &version, const QString &path, bool invalidEntry,
                       const std::function<void(RemoveResult)> &onRemoved);

    // Clears a folder for a Fix. The user has confirmed the clear, so only the path guards apply. onCleared gets whether
    // the folder is gone. A folder that is already gone counts as removed.
    void clear(const QString &path, const std::function<void(bool)> &onCleared);

    // True while a check that has been started has not returned yet.
    bool checking() const;

private:
    int m_pendingChecks = 0;
};

#endif
